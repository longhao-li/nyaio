#include "nyaio/json.hpp"

#include <cstdlib>

using namespace nyaio;
using namespace nyaio::detail;

namespace {

/// @brief
///   Convert a json error code to error message.
/// @return
///   A null-terminated string that represents the json error message.
[[nodiscard]]
constexpr auto json_error_message(json_errc e) noexcept -> const char * {
    switch (e) {
    case json_errc::ok:
        return "Ok";
    case json_errc::invalid_type:
        return "Invalid JSON type";
    [[unlikely]] default:
        return "Unknown JSON error";
    }
}

/// @class json_error_category_type
/// @brief
///   For internal usage. Can be used to convert json error code to string.
class json_error_category_type final : public std::error_category {
public:
    /// @brief
    ///   Destroy this @c json_error_category. Do nothing.
    ~json_error_category_type() override = default;

    /// @brief
    ///   Get error category name.
    /// @return
    ///   A null-terminated string that represents name of json error category.
    [[nodiscard]]
    auto name() const noexcept -> const char * override {
        return "json";
    }

    /// @brief
    ///   Convert an error code into error message.
    /// @param error
    ///   The error code to be converted into message.
    /// @return
    ///   A string that represents error message of @p error.
    [[nodiscard]]
    auto message(int error) const noexcept -> std::string override {
        return json_error_message(static_cast<json_errc>(error));
    }
};

} // namespace

auto nyaio::json_error_category() noexcept -> const std::error_category & {
    static json_error_category_type instance;
    return instance;
}

nyaio::json_error::json_error(nyaio::json_errc e) noexcept
    : std::runtime_error(json_error_message(e)), m_error(e) {}

nyaio::json_error::json_error(nyaio::json_errc e, const char *extra) noexcept
    : std::runtime_error(std::format("{}:{}", extra, json_error_message(e))), m_error(e) {}

nyaio::json_error::json_error(nyaio::json_errc e, const std::string &extra) noexcept
    : std::runtime_error(std::format("{}:{}", extra, json_error_message(e))), m_error(e) {}

nyaio::json_error::~json_error() = default;

auto nyaio::detail::json_allocator::allocate(size_t size) noexcept -> void * {
    // align up with sizeof(int64_t)
    size = (size + 7) & ~static_cast<size_t>(7);

    // happy path
    if (m_free_size >= size) [[likely]] {
        void *memory  = m_current;
        m_free_size  -= size;
        m_current    += size;
        return memory;
    }

    // allocate a whole page for huge object.
    if (size >= sizeof(page_t) / 2) [[unlikely]] {
        auto *memory = static_cast<page_t *>(std::malloc(size + sizeof(int64_t)));
        memory->next = m_pages;
        m_pages      = memory;
        return memory->data;
    }

    // allocate a new page
    auto *p = static_cast<page_t *>(std::malloc(sizeof(page_t)));
    p->next = m_pages;
    m_pages = p;

    m_current   = p->data;
    m_free_size = sizeof(p->data);

    void *memory  = m_current;
    m_current    += size;
    m_free_size  -= size;

    return memory;
}

auto nyaio::detail::json_allocator::clear() noexcept -> void {
    for (page_t *p = m_pages; p != nullptr;) {
        page_t *next = p->next;
        std::free(p);
        p = next;
    }

    m_pages     = nullptr;
    m_current   = nullptr;
    m_free_size = 0;
}

auto nyaio::json_element::clone(detail::json_allocator *alloc) const noexcept -> json_element {
    struct element_state {
        json_element src;
        json_element dst;
        void *next;
    };

    auto *root_payload = alloc->allocate<json_payload>(1);
    auto root          = json_element(root_payload);

    root_payload->type      = json_type::null;
    root_payload->allocator = alloc;

    std::stack<element_state> stack;
    stack.push(element_state{
        .src  = *this,
        .dst  = root,
        .next = nullptr,
    });

    // use non-recursive method to clone the JSON element
    while (!stack.empty()) {
        auto [src, dst, next] = stack.top();
        stack.pop();

        switch (src.type()) {
        case json_type::null:
            dst.set_null();
            break;

        case json_type::boolean:
            dst.set_boolean(*src.boolean());
            break;

        case json_type::object: {
            auto &src_object = src.m_payload->payload.object;

            if (next == nullptr) {
                dst.set_object();
                next = src_object.first;
            }

            if (next != src_object.last) {
                auto dst_object    = *dst.object();
                auto *next_payload = static_cast<json_object_node *>(next);

                std::string_view next_key(next_payload->key.data, next_payload->key.size);

                stack.push(element_state{
                    .src  = src,
                    .dst  = dst,
                    .next = static_cast<json_object_node *>(next) + 1,
                });

                stack.push(element_state{
                    .src  = json_element(next_payload->value),
                    .dst  = dst_object[next_key],
                    .next = nullptr,
                });
            }

            break;
        }

        case json_type::array: {
            auto &src_array = static_cast<json_payload *>(src.m_payload)->payload.array;

            if (next == nullptr) {
                dst.set_array();
                next = src_array.first;
            }

            if (next != src_array.last) {
                auto dst_array = *dst.array();

                stack.push(element_state{
                    .src  = src,
                    .dst  = dst,
                    .next = static_cast<json_payload **>(next) + 1,
                });

                stack.push(element_state{
                    .src  = json_element(*static_cast<json_payload **>(next)),
                    .dst  = dst_array.emplace_back(nullptr),
                    .next = nullptr,
                });
            }

            break;
        }

        case json_type::string:
            dst.set_string(*src.string());
            break;

        case json_type::integer:
            dst.set_integer(*src.integer());
            break;

        case json_type::floating_point:
            dst.set_floating_point(*src.floating_point());
            break;
        }
    }

    return root;
}

auto nyaio::json_object::insert_node(key_type key, mapped_type value) noexcept -> iterator {
    auto *alloc = m_payload->allocator;
    auto &o     = m_payload->payload.object;

    // reserve memory
    if (o.last == o.buffer_end) [[unlikely]] {
        auto size    = static_cast<size_type>(o.last - o.first);
        auto cap     = std::max<size_type>(8, 2 * size);
        auto *buffer = alloc->allocate<json_object_node>(cap);

        memcpy(buffer, o.first, size * sizeof(json_object_node));

        o.first      = buffer;
        o.last       = buffer + size;
        o.buffer_end = buffer + cap;
    }

    // insert the new node
    auto *pos  = std::lower_bound(o.first, o.last, key, json_object_node_compare{});
    auto count = static_cast<size_type>(o.last - pos);

    memmove(pos + 1, pos, count * sizeof(json_object_node));
    o.last += 1;

    pos->key.data = alloc->allocate<char>(key.size());
    pos->key.size = key.size();
    memcpy(pos->key.data, key.data(), key.size());

    pos->value = value.m_payload;
    return iterator(pos);
}

auto nyaio::json_array::resize(size_type count) noexcept -> void {
    auto *alloc = m_payload->allocator;
    auto &a     = m_payload->payload.array;

    auto size = static_cast<size_type>(a.last - a.first);
    if (count <= size) {
        a.last = a.first + count;
        return;
    }

    reserve(count);

    auto *buffer = alloc->allocate<json_payload>(count - size);
    for (auto *iter = a.first + size; iter != a.first + count; ++iter, ++buffer) {
        auto *payload      = buffer;
        payload->type      = json_type::null;
        payload->allocator = alloc;
        *iter              = payload;
    }

    a.last = a.first + count;
}

auto nyaio::json_array::insert_node(size_type index, value_type element) noexcept -> iterator {
    auto &a = m_payload->payload.array;

    if (a.last == a.buffer_end) [[unlikely]]
        reserve(std::max<size_type>(8, 2 * size()));

    auto *node = a.first + index;
    auto count = static_cast<size_type>(a.last - node);
    memmove(node + 1, node, count * sizeof(json_payload *));

    *node   = element.m_payload;
    a.last += 1;

    return iterator(node);
}
