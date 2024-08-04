#pragma once

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <expected>
#include <format>
#include <map>
#include <memory>
#include <stack>
#include <system_error>
#include <vector>

namespace nyaio {

/// @class json_serializer
/// @tparam T
///   The JSON (de)serializable class.
/// @brief
///   ADL helper class for serialization.
template <class T>
struct json_serializer;

/// @enum json_errc
/// @brief
///   Results of json operations.
enum class json_errc {
    ok           = 0,
    invalid_type = 1,
};

/// @brief
///   Obtains a reference to the static error category object for json errors.
/// @return
///   A reference to the static object for json errors.
[[nodiscard]]
NYAIO_API auto json_error_category() noexcept -> const std::error_category &;

/// @brief
///   Construct a @c std::error_code for json error.
/// @param e
///   The json error code to be used for creating @c std::error_code.
/// @return
///   A @c std::error_code that represents a json error.
[[nodiscard]]
inline auto make_error_code(nyaio::json_errc e) noexcept -> std::error_code {
    return {static_cast<int>(e), json_error_category()};
}

/// @class json_error
/// @brief
///   Exception type for json operations.
class NYAIO_API json_error : public std::runtime_error {
public:
    /// @brief
    ///   Create a new @c json_error object from an error code.
    /// @param e
    ///   Json error code of this error.
    json_error(json_errc e) noexcept;

    /// @brief
    ///   Create a new @c json_error object from an error code with extra information.
    /// @param e
    ///   Json error code of this error.
    /// @param extra
    ///   Extra information of the error message. The error message returned by @c what() is
    ///   guaranteed to contain this parameter as a substring.
    json_error(json_errc e, const char *extra) noexcept;

    /// @brief
    ///   Create a new @c json_error object from an error code with extra information.
    /// @param e
    ///   Json error code of this error.
    /// @param extra
    ///   Extra information of the error message. The error message returned by @c what() is
    ///   guaranteed to contain this parameter as a substring.
    json_error(json_errc e, const std::string &extra) noexcept;

    /// @brief
    ///   Destroy this @c json_error object.
    ~json_error() override;

    /// @brief
    ///   Get this @c json_error as a @c std::error_code.
    /// @return
    ///   A @c std::error_code that represents this @c json_error.
    [[nodiscard]]
    auto code() const noexcept -> std::error_code {
        return make_error_code(m_error);
    }

private:
    json_errc m_error;
};

} // namespace nyaio

namespace nyaio {

/// @class json_element
/// @brief
///   Wrapper class for generic JSON type.
class json_element;

/// @class json_object
/// @brief
///   Wrapper class for JSON object.
class json_object;

/// @class json_object
/// @brief
///   Wrapper class for JSON array.
class json_array;

template <class T>
concept json_serializable = requires(const T &value, json_element j) {
    { json_serializer<std::remove_cvref_t<T>>::to_json(value, j) } -> std::same_as<json_errc>;
};

template <class T>
concept json_deserializable = requires(T &value, json_element j) {
    { json_serializer<std::remove_cvref_t<T>>::from_json(value, j) } -> std::same_as<json_errc>;
};

/// @enum json_type
/// @brief
///   Type flag for JSON element.
enum class json_type {
    null,
    boolean,
    object,
    array,
    string,
    integer,
    floating_point,
};

namespace detail {

/// @class json_allocator
/// @brief
///   Stack allocator for JSON document.
class json_allocator {
private:
    /// @brief
    ///   JSON allocator page size. Use 64 KiB here.
    static constexpr size_t page_size = 0x10000;

    /// @struct page_t
    /// @brief
    ///   JSON allocator page object.
    struct page_t {
        page_t *next;
        std::byte data[page_size - sizeof(next)];
    };

public:
    /// @brief
    ///   Create a new JSON stack allocator.
    constexpr json_allocator() noexcept : m_pages(), m_current(), m_free_size() {}

    /// @brief
    ///   @c json_allocator is not copyable.
    json_allocator(const json_allocator &other) = delete;

    /// @brief
    ///   @c json_allocator is not movable.
    json_allocator(json_allocator &&other) = delete;

    /// @brief
    ///   Destroy this JSON allocator and free all memory.
    ~json_allocator() {
        clear();
    }

    /// @brief
    ///   @c json_allocator is not copyable.
    auto operator=(const json_allocator &other) = delete;

    /// @brief
    ///   @c json_allocator is not movable.
    auto operator=(json_allocator &&other) = delete;

    /// @brief
    ///   Allocate some memory from this JSON stack allocator.
    /// @param size
    ///   Expected size in byte of memory to be allocated.
    /// @return
    ///   Pointer to start of allocated memory. It is guaranteed that the allocated memory is
    ///   aligned up with 8 bytes.
    [[nodiscard]]
    NYAIO_API auto allocate(size_t size) noexcept -> void *;

    /// @brief
    ///   Try to allocate some memory for the specified objects.
    /// @tparam T
    ///   Type of the object to allocated memory for.
    /// @param count
    ///   Number of elements to be stored in the allocated memory.
    /// @return
    ///   Pointer to start of allocated memory. It is guaranteed that the allocated memory is
    ///   aligned up with 8 bytes.
    template <class T>
    auto allocate(size_t count) noexcept -> T * {
        return static_cast<T *>(allocate(count * sizeof(T)));
    }

    /// @brief
    ///   Release all allocated memory.
    NYAIO_API auto clear() noexcept -> void;

private:
    page_t *m_pages;
    std::byte *m_current;
    size_t m_free_size;
};

/// @struct json_payload
/// @brief
///   Actual payload for JSON element.
struct json_payload;

/// @struct json_object_key
/// @brief
///   Key struct for JSON object. Generally, this is represents a read-only string.
struct json_object_key {
    char *data;
    size_t size;
};

/// @struct json_object_node
/// @brief
///   Node for JSON object.
struct json_object_node {
    json_object_key key;
    json_payload *value;
};

/// @struct json_object_node_compare
/// @brief
///   Helper struct for searching JSON object member.
struct json_object_node_compare {
    auto operator()(const json_object_node &lhs, std::string_view rhs) const noexcept -> bool {
        return std::string_view(lhs.key.data, lhs.key.size) < rhs;
    }
};

/// @struct json_object_payload
/// @brief
///   Payload content for JSON object.
struct json_object_payload {
    json_object_node *first;
    json_object_node *last;
    json_object_node *buffer_end;
};

/// @struct json_array_payload
/// @brief
///   Payload content for JSON array.
struct json_array_payload {
    json_payload **first;
    json_payload **last;
    json_payload **buffer_end;
};

/// @struct json_string_payload
/// @brief
///   Payload content for JSON string.
struct json_string_payload {
    char *data;
    size_t size;
};

/// @struct json_payload
/// @brief
///   Actual payload for JSON element.
struct json_payload {
    json_type type;
    json_allocator *allocator;
    union {
        bool boolean;
        json_object_payload object;
        json_array_payload array;
        json_string_payload string;
        int64_t integer;
        double floating_point;
    } payload;
};

} // namespace detail

/// @class json_element
/// @brief
///   Wrapper class for generic JSON type.
class json_element {
public:
    /// @brief
    ///   Create an invalid JSON element. It is undefined behavior to use invalid JSON element for
    ///   any purpose except for copy/move constructor, copy/move assignment and destructor.
    constexpr json_element() noexcept : m_payload() {}

    /// @brief
    ///   For internal usage. Create a new JSON element with internal payload.
    /// @param[in] payload
    ///   Payload data of this JSON element.
    explicit json_element(detail::json_payload *payload) noexcept : m_payload(payload) {}
    /// @brief
    ///   Copy constructor of JSON element. JSON element object only holds reference to the JSON
    ///   element data. Copy constructor does shallow copy here. Therefore, JSON element is
    ///   trivially copyable.
    /// @param other
    ///   The JSON element to be copied from.
    json_element(const json_element &other) noexcept = default;

    /// @brief
    ///   Move constructor of JSON element. JSON element object only holds reference to the JSON
    ///   element data. Therefor, move constructor is exactly the same as copy constructor. JSON
    ///   element is trivially movable.
    /// @param other
    ///   The JSON element to be moved.
    json_element(json_element &&other) noexcept = default;

    /// @brief
    ///   JSON element is trivially destructible.
    ~json_element() = default;

    /// @brief
    ///   Copy assignment of JSON element. JSON element object only holds reference to the JSON
    ///   element data. Copy assignment does shallow copy here. Therefore, JSON element is
    ///   trivially copyable.
    /// @param other
    ///   The JSON element to be copied from.
    /// @return
    ///   Reference to this JSON element.
    auto operator=(const json_element &other) noexcept -> json_element & = default;

    /// @brief
    ///   Move assignment of JSON element. JSON element object only holds reference to the JSON
    ///   element data. Therefor, move assignment is exactly the same as copy assignment. JSON
    ///   element is trivially movable.
    /// @param other
    ///   The JSON element to be moved.
    /// @return
    ///   Reference to this JSON element.
    auto operator=(json_element &&other) noexcept -> json_element & = default;

    /// @brief
    ///   Serialize an object into JSON element.
    /// @tparam T
    ///   Type of the object to be serialized.
    /// @param[in] other
    ///   The object to be serialized. To support customized serialization, please implement
    ///   @c to_json() in @c nyaio::serializer specialization for the given type.
    /// @return
    ///   Reference to this JSON element.
    /// @throws json_error
    ///   Thrown if serialization failed.
    template <json_serializable T>
    auto operator=(T &&other) -> json_element & {
        json_errc error =
            json_serializer<std::remove_cvref_t<T>>::to_json(std::forward<T>(other), *this);

        if (error != json_errc::ok) [[unlikely]]
            throw json_error(error);

        return *this;
    }

    /// @brief
    ///   Get type of this JSON element. Array and object JSON element can be safely cast to
    ///   @c json_array and @c json_object.
    /// @return
    ///   Type of this JSON element.
    [[nodiscard]]
    auto type() const noexcept -> json_type {
        return m_payload->type;
    }

    /// @brief
    ///   Checks if this JSON element is null.
    /// @retval true
    ///   This JSON element is null.
    /// @retval false
    ///   This JSON element is not null.
    [[nodiscard]]
    auto is_null() const noexcept -> bool {
        return type() == json_type::null;
    }

    /// @brief
    ///   Checks if this JSON element is a boolean value.
    /// @retval true
    ///   This JSON element is a boolean value.
    /// @retval false
    ///   This JSON element is not a boolean value.
    [[nodiscard]]
    auto is_boolean() const noexcept -> bool {
        return type() == json_type::boolean;
    }

    /// @brief
    ///   Checks if this JSON element is an object. This JSON element can be safely cast to
    ///   @c json_object if this JSON element is an object.
    /// @retval true
    ///   This JSON element is an object.
    /// @retval false
    ///   This JSON element is not an object.
    [[nodiscard]]
    auto is_object() const noexcept -> bool {
        return type() == json_type::object;
    }

    /// @brief
    ///   Checks if this JSON element is an array. This JSON element can be safely cast to
    ///   @c json_array if this JSON element is an array.
    /// @retval true
    ///   This JSON element is an array.
    /// @retval false
    ///   This JSON element is not an array.
    [[nodiscard]]
    auto is_array() const noexcept -> bool {
        return type() == json_type::array;
    }

    /// @brief
    ///   Checks if this JSON element is a string. This JSON element can be safely cast to
    ///   @c json_string if this JSON element is a string.
    /// @retval true
    ///   This JSON element is a string.
    /// @retval false
    ///   This JSON element is not a string.
    [[nodiscard]]
    auto is_string() const noexcept -> bool {
        return type() == json_type::string;
    }

    /// @brief
    ///   Checks if this JSON element is an integer value.
    /// @retval true
    ///   This JSON element is an integer value.
    /// @retval false
    ///   This JSON element is not an integer value.
    [[nodiscard]]
    auto is_integer() const noexcept -> bool {
        return type() == json_type::integer;
    }

    /// @brief
    ///   Checks if this JSON element is a floating point number.
    /// @retval true
    ///   This JSON element is a floating point number.
    /// @retval false
    ///   This JSON element is not a floating point number.
    [[nodiscard]]
    auto is_floating_point() const noexcept -> bool {
        return type() == json_type::floating_point;
    }

    /// @brief
    ///   Checks if this JSON element is a number. A number is either an integer or a floating point
    ///   number.
    /// @retval true
    ///   This JSON element is a number.
    /// @retval false
    ///   This JSON element is not a number.
    [[nodiscard]]
    auto is_number() const noexcept -> bool {
        return is_integer() || is_floating_point();
    }

    /// @brief
    ///   Try to get boolean value from this JSON element.
    /// @return
    ///   A boolean value if this JSON element is bool. Otherwise, return an error code that
    ///   indicates this JSON element is not a boolean value.
    [[nodiscard]]
    auto boolean() const noexcept -> std::expected<bool, json_errc> {
        if (!is_boolean()) [[unlikely]]
            return std::unexpected(json_errc::invalid_type);
        return m_payload->payload.boolean;
    }

    /// @brief
    ///   Try to get this JSON element as a JSON object.
    /// @return
    ///   A JSON object if this JSON element is a JSON object. Otherwise, return an error code that
    ///   indicates this JSON element is not a JSON object.
    [[nodiscard]]
    auto object() const noexcept -> std::expected<json_object, json_errc>;

    /// @brief
    ///   Try to get this JSON element as a JSON array.
    /// @return
    ///   A JSON array if this JSON element is a JSON array. Otherwise, return an error code that
    ///   indicates this JSON element is not a JSON array.
    [[nodiscard]]
    auto array() const noexcept -> std::expected<json_array, json_errc>;

    /// @brief
    ///   Try to get string from this JSON element.
    /// @return
    ///   A string if this JSON element is a string. Otherwise, return an error code that indicates
    ///   this JSON element is not a string.
    [[nodiscard]]
    auto string() const noexcept -> std::expected<std::string_view, json_errc> {
        if (!is_string()) [[unlikely]]
            return std::unexpected(json_errc::invalid_type);
        return std::string_view(m_payload->payload.string.data, m_payload->payload.string.size);
    }

    /// @brief
    ///   Try to get integer value from this JSON element.
    /// @return
    ///   An integer value if this JSON element is an integer. Otherwise, return an error code that
    ///   indicates this JSON element is not an integer.
    [[nodiscard]]
    auto integer() const noexcept -> std::expected<int64_t, json_errc> {
        if (!is_integer()) [[unlikely]]
            return std::unexpected(json_errc::invalid_type);
        return m_payload->payload.integer;
    }

    /// @brief
    ///   Try to get floating point value from this JSON element.
    /// @return
    ///   A floating point value if this JSON element is an integer. Otherwise, return an error code
    ///   that indicates this JSON element is not a floating point number.
    [[nodiscard]]
    auto floating_point() const noexcept -> std::expected<double, json_errc> {
        if (!is_floating_point()) [[unlikely]]
            return std::unexpected(json_errc::invalid_type);
        return m_payload->payload.floating_point;
    }

    /// @brief
    ///   Try to serialize an object into JSON. This is exactly the same as @c operator=.
    /// @tparam T
    ///   Type of the object to be serialized.
    /// @param[in, out] value
    ///   The object to be serialized.
    /// @return
    ///   An error code that indicates result of the serialization. The return value is
    ///   @c json_errc::ok if succeeded.
    template <json_serializable T>
    auto from(T &&value) -> json_errc {
        return json_serializer<std::remove_cvref_t<T>>::to_json(std::forward<T>(value), *this);
    }

    /// @brief
    ///   Try to deserialize this JSON element into object.
    /// @tparam T
    ///   Type of the object to be deserialized to.
    /// @param[in, out] value
    ///   The object to be deserialized to.
    /// @return
    ///   An error code that indicates result of the deserialization. The return value is
    ///   @c json_errc::ok if succeeded.
    template <json_deserializable T>
    auto to(T &value) const -> json_errc {
        return json_serializer<std::remove_cvref_t<T>>::from_json(value, *this);
    }

    /// @brief
    ///   Set this JSON element to be null. This method is exactly the same as converting this JSON
    ///   element into null.
    /// @param value
    ///   Null value to be set. Must be @c nullptr.
    auto set_null([[maybe_unused]] std::nullptr_t value = nullptr) noexcept -> void {
        m_payload->type = json_type::null;
    }

    /// @brief
    ///   Set a boolean value for this JSON element. Type of this JSON element will be converted to
    ///   boolean value first if it is not.
    /// @param value
    ///   The boolean value to be set.
    auto set_boolean(bool value) noexcept -> void {
        m_payload->type            = json_type::boolean;
        m_payload->payload.boolean = value;
    }

    /// @brief
    ///   Convert this JSON element to JSON object. The original data will be dropped whatever this
    ///   JSON element used to be.
    auto set_object() noexcept -> void {
        m_payload->type                      = json_type::object;
        m_payload->payload.object.first      = nullptr;
        m_payload->payload.object.last       = nullptr;
        m_payload->payload.object.buffer_end = nullptr;
    }

    /// @brief
    ///   Convert this JSON element to JSON array. The original data will be dropped whatever this
    ///   JSON element used to be.
    auto set_array() noexcept -> void {
        m_payload->type                     = json_type::array;
        m_payload->payload.array.first      = nullptr;
        m_payload->payload.array.last       = nullptr;
        m_payload->payload.array.buffer_end = nullptr;
    }

    /// @brief
    ///   Set a string for this JSON element. Type of this JSON element will be converted to string
    ///   if it is not.
    /// @param value
    ///   The string to be set.
    auto set_string(std::string_view value) noexcept -> void {
        auto &str = m_payload->payload.string;

        if (is_string() && str.size >= value.size()) {
            memcpy(str.data, value.data(), value.size());
            str.size = value.size();
            return;
        }

        m_payload->type = json_type::string;

        str.data = m_payload->allocator->allocate<char>(value.size());
        str.size = value.size();
        memcpy(str.data, value.data(), value.size());
    }

    /// @brief
    ///   Set an integer value for this JSON element. Type of this JSON element will be converted to
    ///   integer if it is not.
    /// @param value
    ///   The integer to be set.
    auto set_integer(int64_t value) noexcept -> void {
        m_payload->type            = json_type::integer;
        m_payload->payload.integer = value;
    }

    /// @brief
    ///   Set a floating point number for this JSON element. Type of this JSON element will be
    ///   converted to floating point number if it is not.
    /// @param value
    ///   The floating point number to be set.
    auto set_floating_point(double value) noexcept -> void {
        m_payload->type                   = json_type::floating_point;
        m_payload->payload.floating_point = value;
    }

    /// @brief
    ///   Convert this JSON element to the given type. The original data will be dropped whatever
    ///   this JSON element used to be.
    /// @param t
    ///   New type of this JSON element. For null, no data is set. For boolean value, this JSON
    ///   element will be set to @c false. For object and array, this JSON element will be set to
    ///   empty object and empty array. For string, this JSON element will be set to an empty
    ///   string. For number types, this JSON element will be set to 0.
    auto convert_to(json_type t) noexcept -> void {
        m_payload->type = t;
        __builtin_memset(&m_payload->payload, 0, sizeof(m_payload->payload));
    }

    /// @brief
    ///   Dump this JSON element to output iterator. The output string will be compressed into one
    ///   line.
    /// @tparam OutputIt
    ///   Type of the character output iterator.
    /// @param out
    ///   The output iterator to write the JSON string to.
    /// @return
    ///   The output iterator after writing the JSON string.
    template <std::output_iterator<char> OutputIt>
    auto dump_to(OutputIt out) const -> OutputIt;

    /// @brief
    ///   Dump this JSON element to output iterator.
    ///   line.
    /// @tparam OutputIt
    ///   Type of the character output iterator.
    /// @param out
    ///   The output iterator to write the JSON string to.
    /// @param indent
    ///   Indention string to be used for each level of JSON object and array.
    /// @return
    ///   The output iterator after writing the JSON string.
    template <std::output_iterator<char> OutputIt>
    auto dump_to(OutputIt out, std::string_view indent) const -> OutputIt;

    /// @brief
    ///   Dump this JSON element into a string. The output string will be compressed into one line.
    /// @return
    ///   An UTF-8 string that represents dump result of this JSON element.
    [[nodiscard]]
    auto dump() const noexcept -> std::string {
        std::string content;
        dump_to(std::back_inserter(content));
        return content;
    }

    /// @brief
    ///   Dump this JSON element into a string.
    /// @param indent
    ///   Indention unit.
    /// @return
    ///   An UTF-8 string that represents dump result of this JSON element.
    [[nodiscard]]
    auto dump(std::string_view indent) const noexcept -> std::string {
        std::string content;
        dump_to(std::back_inserter(content), indent);
        return content;
    }

    /// @brief
    ///   Implicit cast this JSON element to deserializable type.
    /// @tparam T
    ///   Type to be deserialized to.
    /// @throws json_error
    ///   Thrown if there is any error occurs during deserialization.
    template <json_deserializable T>
        requires(std::is_default_constructible_v<T> && std::is_move_constructible_v<T>)
    operator T() const {
        T value;
        json_errc e = json_serializer<std::remove_cvref_t<T>>::from_json(value, *this);

        if (e != json_errc::ok) [[unlikely]]
            throw json_error(e);

        return value;
    }

    friend class json_document;
    friend class json_object;
    friend class json_array;

private:
    /// @brief
    ///   Acquire a new JSON element from this element. The new element will inherit allocator from
    ///   this one.
    /// @return
    ///   A new null JSON element.
    [[nodiscard]]
    auto new_element() const noexcept -> json_element {
        detail::json_allocator *alloc = m_payload->allocator;

        // allocate a new element
        auto *element = alloc->allocate<detail::json_payload>(1);

        element->type      = json_type::null;
        element->allocator = alloc;

        return json_element(element);
    }

    /// @brief
    ///   For internal usage. Perform deep copy of this JSON element.
    /// @param[in] alloc
    ///   Allocator to be used for the new JSON element.
    /// @return
    ///   A deep copy of this JSON element.
    [[nodiscard]]
    NYAIO_API auto clone(detail::json_allocator *alloc) const noexcept -> json_element;

private:
    detail::json_payload *m_payload;
};

namespace detail {

/// @brief
///   Helper function to append escaped character to output iterator.
/// @tparam OutputIt
///   Type of the output iterator.
/// @param out
///   The output iterator to write the escaped character to.
/// @param c
///   The character to be escaped.
/// @return
///   The output iterator after writing the escaped character.
template <std::output_iterator<char> OutputIt>
auto json_append_escaped_char(OutputIt out, char c) -> OutputIt {
    switch (c) {
    case '\0':
        *out = '\\';
        ++out;
        *out = '0';
        ++out;
        return out;

    case '\a':
        *out = '\\';
        ++out;
        *out = 'a';
        ++out;
        return out;

    case '\b':
        *out = '\\';
        ++out;
        *out = 'b';
        ++out;
        return out;

    case '\t':
        *out = '\\';
        ++out;
        *out = 't';
        ++out;
        return out;

    case '\n':
        *out = '\\';
        ++out;
        *out = 'n';
        ++out;
        return out;

    case '\v':
        *out = '\\';
        ++out;
        *out = 'v';
        ++out;
        return out;

    case '\f':
        *out = '\\';
        ++out;
        *out = 'f';
        ++out;
        return out;

    case '\r':
        *out = '\\';
        ++out;
        *out = 'r';
        ++out;
        return out;

    case '\"':
        *out = '\\';
        ++out;
        *out = '\"';
        ++out;
        return out;

    case '\'':
        *out = '\\';
        ++out;
        *out = '\'';
        ++out;
        return out;

    case '\\':
        *out = '\\';
        ++out;
        *out = '\\';
        ++out;
        return out;

    default:
        if (c >= 0 && c <= 0x1F) {
            *out = '\\';
            ++out;
            *out = 'u';
            ++out;
            *out = '0';
            ++out;
            *out = '0';
            ++out;
            *out = "0123456789ABCDEF"[c >> 4];
            ++out;
            *out = "0123456789ABCDEF"[c & 0xF];
            ++out;
            return out;
        }

        *out = c;
        ++out;
        return out;
    }
}

} // namespace detail

template <std::output_iterator<char> OutputIt>
auto json_element::dump_to(OutputIt out) const -> OutputIt {
    struct element_state {
        detail::json_payload *payload;
        void *next;
    };

    std::stack<element_state> stack;
    stack.push({
        .payload = m_payload,
        .next    = nullptr,
    });

    while (!stack.empty()) {
        auto [payload, next] = stack.top();
        stack.pop();

        switch (payload->type) {
        case json_type::null:
            out = std::format_to(out, "null");
            break;

        case json_type::boolean:
            if (payload->payload.boolean) {
                for (auto c : std::string_view("true")) {
                    *out = c;
                    ++out;
                }
            } else {
                for (auto c : std::string_view("false")) {
                    *out = c;
                    ++out;
                }
            }
            break;

        case json_type::object: {
            detail::json_object_payload &object = payload->payload.object;
            auto *iter                          = static_cast<detail::json_object_node *>(next);

            if (iter == nullptr) {
                iter = object.first;
                *out = '{';
                ++out;
            }

            if (iter == object.last) {
                *out = '}';
                ++out;
                break;
            }

            stack.push({
                .payload = payload,
                .next    = iter + 1,
            });

            if (iter != object.first) {
                *out = ',';
                ++out;
            }

            // append object key
            *out = '\"';
            ++out;
            for (auto c : std::string_view(iter->key.data, iter->key.size))
                out = detail::json_append_escaped_char(out, c);
            *out = '\"';
            ++out;
            *out = ':';
            ++out;

            // append object value
            stack.push({
                .payload = iter->value,
                .next    = nullptr,
            });

            break;
        }

        case json_type::array: {
            detail::json_array_payload &array = payload->payload.array;
            auto *iter                        = static_cast<detail::json_payload **>(next);

            if (iter == nullptr) {
                iter = array.first;
                *out = '[';
                ++out;
            }

            if (iter == array.last) {
                *out = ']';
                ++out;
                break;
            }

            stack.push({
                .payload = payload,
                .next    = iter + 1,
            });

            if (iter != array.first) {
                *out = ',';
                ++out;
            }

            stack.push({
                .payload = *iter,
                .next    = nullptr,
            });

            break;
        }

        case json_type::string: {
            detail::json_string_payload &string = payload->payload.string;

            *out = '\"';
            ++out;
            for (auto c : std::string_view(string.data, string.size))
                out = detail::json_append_escaped_char(out, c);
            *out = '\"';
            ++out;

            break;
        }

        case json_type::integer:
            out = std::format_to(out, "{}", payload->payload.integer);
            break;

        case json_type::floating_point:
            out = std::format_to(out, "{}", payload->payload.floating_point);
            break;
        }
    }

    return out;
}

template <std::output_iterator<char> OutputIt>
auto json_element::dump_to(OutputIt out, std::string_view indent) const -> OutputIt {
    struct element_state {
        detail::json_payload *payload;
        void *next;
    };

    std::stack<element_state> stack;
    stack.push({
        .payload = m_payload,
        .next    = nullptr,
    });

    // helper function to append space
    auto append_space = [&out, &stack, indent]() noexcept -> void {
        for (size_t i = 0; i < stack.size(); ++i)
            out = std::format_to(out, "{}", indent);
    };

    // helper function to append newline
    auto append_newline = [&out]() noexcept -> void {
        *out = '\n';
        ++out;
    };

    while (!stack.empty()) {
        auto [payload, next] = stack.top();
        stack.pop();

        switch (payload->type) {
        case json_type::null:
            out = std::format_to(out, "null");
            break;

        case json_type::boolean:
            if (payload->payload.boolean) {
                for (auto c : std::string_view("true")) {
                    *out = c;
                    ++out;
                }
            } else {
                for (auto c : std::string_view("false")) {
                    *out = c;
                    ++out;
                }
            }
            break;

        case json_type::object: {
            detail::json_object_payload &object = payload->payload.object;
            auto *iter                          = static_cast<detail::json_object_node *>(next);

            if (iter == nullptr) {
                iter = object.first;
                *out = '{';
                ++out;
            }

            if (iter == object.last) {
                append_newline();
                append_space();

                *out = '}';
                ++out;
                break;
            }

            stack.push({
                .payload = payload,
                .next    = iter + 1,
            });

            if (iter != object.first) {
                *out = ',';
                ++out;
            }

            append_newline();
            append_space();

            // append object key
            *out = '\"';
            ++out;
            for (auto c : std::string_view(iter->key.data, iter->key.size))
                out = detail::json_append_escaped_char(out, c);
            *out = '\"';
            ++out;
            *out = ':';
            ++out;
            *out = ' ';
            ++out;

            // append object value
            stack.push({
                .payload = iter->value,
                .next    = nullptr,
            });

            break;
        }

        case json_type::array: {
            detail::json_array_payload &array = payload->payload.array;
            auto *iter                        = static_cast<detail::json_payload **>(next);

            if (iter == nullptr) {
                iter = array.first;
                *out = '[';
                ++out;
            }

            if (iter == array.last) {
                append_newline();
                append_space();
                *out = ']';
                ++out;
                break;
            }

            stack.push({
                .payload = payload,
                .next    = iter + 1,
            });

            if (iter != array.first) {
                *out = ',';
                ++out;
            }

            append_newline();
            append_space();

            stack.push({
                .payload = *iter,
                .next    = nullptr,
            });

            break;
        }

        case json_type::string: {
            detail::json_string_payload &string = payload->payload.string;

            *out = '\"';
            ++out;
            for (auto c : std::string_view(string.data, string.size))
                out = detail::json_append_escaped_char(out, c);
            *out = '\"';
            ++out;

            break;
        }

        case json_type::integer:
            out = std::format_to(out, "{}", payload->payload.integer);
            break;

        case json_type::floating_point:
            out = std::format_to(out, "{}", payload->payload.floating_point);
            break;
        }
    }

    return out;
}

} // namespace nyaio

namespace nyaio {

/// @class json_object_const_iterator
/// @brief
///   Const iterator type for JSON object.
class json_object_const_iterator {
public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type        = std::pair<std::string_view, const json_element>;
    using difference_type   = std::ptrdiff_t;
    using reference         = std::pair<std::string_view, const json_element>;

private:
    class pointer_proxy {
    public:
        explicit pointer_proxy(reference value) noexcept : m_value(value) {}

        auto operator->() noexcept -> reference * {
            return &m_value;
        }

        auto operator->() const noexcept -> const reference * {
            return &m_value;
        }

    private:
        reference m_value;
    };

public:
    using pointer = pointer_proxy;

    /// @brief
    ///   For internal usage. Create a new JSON object iterator from payload.
    /// @param[in] node
    ///   Pointer to JSON object node payload.
    explicit json_object_const_iterator(detail::json_object_node *node) noexcept
        : m_current(node) {}

    auto operator*() const noexcept -> reference {
        std::string_view key(m_current->key.data, m_current->key.size);
        json_element value(m_current->value);
        return std::make_pair(key, value);
    }

    auto operator->() const noexcept -> pointer {
        std::string_view key(m_current->key.data, m_current->key.size);
        json_element value(m_current->value);
        return pointer(std::make_pair(key, value));
    }

    auto operator[](difference_type offset) const noexcept -> reference {
        auto *node = m_current + offset;
        std::string_view key(node->key.data, node->key.size);
        json_element value(node->value);
        return std::make_pair(key, value);
    }

    auto operator+(difference_type offset) const noexcept -> json_object_const_iterator {
        return json_object_const_iterator(m_current + offset);
    }

    auto operator-(difference_type offset) const noexcept -> json_object_const_iterator {
        return json_object_const_iterator(m_current - offset);
    }

    auto operator++() noexcept -> json_object_const_iterator & {
        m_current += 1;
        return *this;
    }

    auto operator--() noexcept -> json_object_const_iterator & {
        m_current -= 1;
        return *this;
    }

    auto operator++(int) noexcept -> json_object_const_iterator {
        json_object_const_iterator ret  = *this;
        m_current                      += 1;
        return ret;
    }

    auto operator--(int) noexcept -> json_object_const_iterator {
        json_object_const_iterator ret  = *this;
        m_current                      -= 1;
        return ret;
    }

    auto operator+=(difference_type offset) noexcept -> json_object_const_iterator & {
        m_current += offset;
        return *this;
    }

    auto operator-=(difference_type offset) noexcept -> json_object_const_iterator & {
        m_current -= offset;
        return *this;
    }

    auto operator-(json_object_const_iterator rhs) const noexcept -> difference_type {
        return m_current - rhs.m_current;
    }

    auto operator==(json_object_const_iterator rhs) const noexcept -> bool {
        return m_current == rhs.m_current;
    }

    auto operator<=>(json_object_const_iterator rhs) const noexcept -> std::strong_ordering {
        return m_current <=> rhs.m_current;
    }

    friend class json_object;

private:
    detail::json_object_node *m_current;
};

/// @class json_object_iterator
/// @brief
///   Iterator type for JSON object.
class json_object_iterator {
public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type        = std::pair<std::string_view, json_element>;
    using difference_type   = std::ptrdiff_t;
    using reference         = std::pair<std::string_view, json_element>;

private:
    class pointer_proxy {
    public:
        explicit pointer_proxy(reference value) noexcept : m_value(value) {}

        auto operator->() noexcept -> reference * {
            return &m_value;
        }

        auto operator->() const noexcept -> const reference * {
            return &m_value;
        }

    private:
        reference m_value;
    };

public:
    using pointer = pointer_proxy;

    /// @brief
    ///   For internal usage. Create a new JSON object iterator from payload.
    /// @param[in] node
    ///   Pointer to JSON object node payload.
    explicit json_object_iterator(detail::json_object_node *node) noexcept : m_current(node) {}

    auto operator*() const noexcept -> reference {
        std::string_view key(m_current->key.data, m_current->key.size);
        json_element value(m_current->value);
        return std::make_pair(key, value);
    }

    auto operator->() const noexcept -> pointer {
        std::string_view key(m_current->key.data, m_current->key.size);
        json_element value(m_current->value);
        return pointer(std::make_pair(key, value));
    }

    auto operator[](difference_type offset) const noexcept -> reference {
        auto *node = m_current + offset;
        std::string_view key(node->key.data, node->key.size);
        json_element value(node->value);
        return std::make_pair(key, value);
    }

    auto operator+(difference_type offset) const noexcept -> json_object_iterator {
        return json_object_iterator(m_current + offset);
    }

    auto operator-(difference_type offset) const noexcept -> json_object_iterator {
        return json_object_iterator(m_current - offset);
    }

    auto operator++() noexcept -> json_object_iterator & {
        m_current += 1;
        return *this;
    }

    auto operator--() noexcept -> json_object_iterator & {
        m_current -= 1;
        return *this;
    }

    auto operator++(int) noexcept -> json_object_iterator {
        json_object_iterator ret  = *this;
        m_current                += 1;
        return ret;
    }

    auto operator--(int) noexcept -> json_object_iterator {
        json_object_iterator ret  = *this;
        m_current                -= 1;
        return ret;
    }

    auto operator+=(difference_type offset) noexcept -> json_object_iterator & {
        m_current += offset;
        return *this;
    }

    auto operator-=(difference_type offset) noexcept -> json_object_iterator & {
        m_current -= offset;
        return *this;
    }

    auto operator-(json_object_iterator rhs) const noexcept -> difference_type {
        return m_current - rhs.m_current;
    }

    auto operator==(json_object_iterator rhs) const noexcept -> bool {
        return m_current == rhs.m_current;
    }

    auto operator<=>(json_object_iterator rhs) const noexcept -> std::strong_ordering {
        return m_current <=> rhs.m_current;
    }

    operator json_object_const_iterator() const noexcept {
        return json_object_const_iterator(m_current);
    }

    friend class json_object;

private:
    detail::json_object_node *m_current;
};

/// @class json_object
/// @brief
///   Wrapper class for JSON object.
class json_object : public json_element {
public:
    using key_type               = std::string_view;
    using mapped_type            = json_element;
    using value_type             = std::pair<key_type, mapped_type>;
    using reference              = std::pair<key_type, mapped_type>;
    using const_reference        = std::pair<key_type, const mapped_type>;
    using size_type              = size_t;
    using difference_type        = ptrdiff_t;
    using iterator               = json_object_iterator;
    using const_iterator         = json_object_const_iterator;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    /// @brief
    ///   Get iterator to the first element of this @c json_object.
    /// @return
    ///   Iterator to the first element of this @c json_object.
    [[nodiscard]]
    auto begin() noexcept -> iterator {
        return iterator(m_payload->payload.object.first);
    }

    /// @brief
    ///   Get iterator to the first element of this @c json_object.
    /// @return
    ///   Iterator to the first element of this @c json_object.
    [[nodiscard]]
    auto begin() const noexcept -> const_iterator {
        return const_iterator(m_payload->payload.object.first);
    }

    /// @brief
    ///   Get iterator to the first element of this @c json_object.
    /// @return
    ///   Iterator to the first element of this @c json_object.
    [[nodiscard]]
    auto cbegin() const noexcept -> const_iterator {
        return begin();
    }

    /// @brief
    ///   Get iterator to the element following the last element of this @c json_object.
    /// @return
    ///   Iterator to the element following the last element of this @c json_object.
    [[nodiscard]]
    auto end() noexcept -> iterator {
        return iterator(m_payload->payload.object.last);
    }

    /// @brief
    ///   Get iterator to the element following the last element of this @c json_object.
    /// @return
    ///   Iterator to the element following the last element of this @c json_object.
    [[nodiscard]]
    auto end() const noexcept -> const_iterator {
        return const_iterator(m_payload->payload.object.last);
    }

    /// @brief
    ///   Get iterator to the element following the last element of this @c json_object.
    /// @return
    ///   Iterator to the element following the last element of this @c json_object.
    [[nodiscard]]
    auto cend() const noexcept -> const_iterator {
        return end();
    }

    /// @brief
    ///   Get reverse iterator to the first element of the reversed @c json_object.
    /// @return
    ///   Reverse iterator to the first element of the reversed @c json_object.
    [[nodiscard]]
    auto rbegin() noexcept -> reverse_iterator {
        return reverse_iterator(end());
    }

    /// @brief
    ///   Get reverse iterator to the first element of the reversed @c json_object.
    /// @return
    ///   Reverse iterator to the first element of the reversed @c json_object.
    [[nodiscard]]
    auto rbegin() const noexcept -> const_reverse_iterator {
        return const_reverse_iterator(end());
    }

    /// @brief
    ///   Get reverse iterator to the first element of the reversed @c json_object.
    /// @return
    ///   Reverse iterator to the first element of the reversed @c json_object.
    [[nodiscard]]
    auto crbegin() const noexcept -> const_reverse_iterator {
        return rbegin();
    }

    /// @brief
    ///   Get reverse iterator to the element after the last element of the reversed @c json_object.
    /// @return
    ///   Reverse iterator to the element after the last element of the reversed @c json_object.
    [[nodiscard]]
    auto rend() noexcept -> reverse_iterator {
        return reverse_iterator(begin());
    }

    /// @brief
    ///   Get reverse iterator to the element after the last element of the reversed @c json_object.
    /// @return
    ///   Reverse iterator to the element after the last element of the reversed @c json_object.
    [[nodiscard]]
    auto rend() const noexcept -> const_reverse_iterator {
        return const_reverse_iterator(begin());
    }

    /// @brief
    ///   Get reverse iterator to the element after the last element of the reversed @c json_object.
    /// @return
    ///   Reverse iterator to the element after the last element of the reversed @c json_object.
    [[nodiscard]]
    auto crend() const noexcept -> const_reverse_iterator {
        return rend();
    }

    /// @brief
    ///   Checks if this is an empty JSON object.
    /// @retval true
    ///   This JSON object has no member.
    /// @retval false
    ///   This JSON object has members.
    [[nodiscard]]
    auto empty() const noexcept -> bool {
        return m_payload->payload.object.first == m_payload->payload.object.last;
    }

    /// @brief
    ///   Get number of members in this JSON object.
    /// @return
    ///   Number of members in this JSON object.
    [[nodiscard]]
    auto size() const noexcept -> size_type {
        auto &o = m_payload->payload.object;
        return static_cast<size_type>(o.last - o.first);
    }

    /// @brief
    ///   Try to find the JSON object member with the specified name.
    /// @param key
    ///   Name of the JSON object member to be found.
    /// @return
    ///   Iterator to the member if found. Otherwise, return iterator equivalent to @c end().
    [[nodiscard]]
    auto find(key_type key) noexcept -> iterator {
        auto &o   = m_payload->payload.object;
        auto *pos = std::lower_bound(o.first, o.last, key, detail::json_object_node_compare{});

        if (pos != o.last && std::string_view(pos->key.data, pos->key.size) == key)
            return iterator(pos);

        return iterator(o.last);
    }

    /// @brief
    ///   Try to find the JSON object member with the specified name.
    /// @param key
    ///   Name of the JSON object member to be found.
    /// @return
    ///   Iterator to the member if found. Otherwise, return iterator equivalent to @c end().
    [[nodiscard]]
    auto find(key_type key) const noexcept -> const_iterator {
        auto &o   = m_payload->payload.object;
        auto *pos = std::lower_bound(o.first, o.last, key, detail::json_object_node_compare{});

        if (pos != o.last && std::string_view(pos->key.data, pos->key.size) == key)
            return iterator(pos);

        return const_iterator(o.last);
    }

    /// @brief
    ///   Get number of JSON object members with the specified name.
    /// @param key
    ///   Name of the JSON object member to be counted.
    /// @return
    ///   Number of JSON object members with the specified name. The return value is always 0 or 1.
    [[nodiscard]]
    auto count(key_type key) const noexcept -> size_type {
        return contains(key) ? 1 : 0;
    }

    /// @brief
    ///   Checks if this JSON object has the specified member.
    /// @param key
    ///   Name of the member to be checked.
    /// @retval true
    ///   This JSON object has a member with the specified name.
    /// @retval false
    ///   This JSON object does not have a member with the specified name.
    [[nodiscard]]
    auto contains(key_type key) const noexcept -> bool {
        auto &o   = m_payload->payload.object;
        auto *pos = std::lower_bound(o.first, o.last, key, detail::json_object_node_compare{});
        return pos != o.last && std::string_view(pos->key.data, pos->key.size) == key;
    }

    /// @brief
    ///   Insert a new member into this JSON object if not exist.
    /// @tparam T
    ///   Type of the object to be serialized as a JSON object.
    /// @param key
    ///   Name of the JSON member.
    /// @param[in] value
    ///   The value to be serialized into JSON.
    template <json_serializable T>
    auto emplace(key_type key, T &&value) -> std::pair<iterator, bool> {
        return insert(key, std::forward<T>(value));
    }

    /// @brief
    ///   Insert a new member into this JSON object if not exist.
    /// @tparam T
    ///   Type of the object to be serialized as a JSON object.
    /// @param key
    ///   Name of the JSON member.
    /// @param[in] value
    ///   The value to be serialized into JSON.
    /// @throws json_error
    ///   Thrown if failed to serialize the value into JSON.
    template <json_serializable T>
    auto insert(key_type key, T &&value) -> std::pair<iterator, bool> {
        // return existing value if found.
        iterator iter = find(key);
        if (iter != end())
            return std::make_pair(iter, false);

        // create the new node
        json_element e = new_element();

        json_errc error =
            json_serializer<std::remove_cvref_t<T>>::to_json(std::forward<T>(value), e);
        if (error != json_errc::ok) [[unlikely]]
            throw json_error(error);

        return std::make_pair(insert_node(key, e), true);
    }

    /// @brief
    ///   Remove the JSON member at @p position.
    /// @param position
    ///   Position of the element to be removed.
    /// @return
    ///   Iterator to the element after the removed element.
    auto erase(const_iterator position) noexcept -> iterator {
        auto &o = m_payload->payload.object;

        auto *node = position.m_current;
        auto *next = node + 1;
        auto count = static_cast<size_type>(o.last - next);

        memmove(node, next, count * sizeof(detail::json_object_node));
        o.last = node + count;

        return iterator(node);
    }

    /// @brief
    ///   Remove JSON members between @p first (included) and @p last (excluded).
    /// @param first
    ///   Iterator to the first JSON member to be removed.
    /// @param last
    ///   Iterator to the element after the last JSON member to be removed.
    /// @param
    ///   Iterator to the element after the last removed element.
    auto erase(const_iterator first, const_iterator last) noexcept -> iterator {
        auto &o = m_payload->payload.object;

        auto *node = first.m_current;
        auto *next = last.m_current;
        auto count = static_cast<size_type>(o.last - next);

        memmove(node, next, count * sizeof(detail::json_object_node));
        o.last = node + count;

        return iterator(node);
    }

    /// @brief
    ///   Remove the JSON object member with the same name as @p key.
    /// @param key
    ///   Name of the JSON object member to be removed.
    /// @return
    ///   Number of elements removed. The return value is always 0 or 1.
    auto erase(key_type key) noexcept -> size_type {
        if (auto iter = find(key); iter != end()) {
            erase(iter);
            return 1;
        }

        return 0;
    }

    /// @brief
    ///   Remove all JSON object members. @c size() returns 0 after this call.
    auto clear() noexcept -> void {
        auto &o = m_payload->payload.object;
        o.last  = o.first;
    }

    /// @brief
    ///   Try to get the JSON object member with the specified name. Please notice that a new null
    ///   member will be created if this object does not have a member with the specified name.
    /// @param key
    ///   Key of the member to be retrieved.
    /// @return
    ///   The JSON object member with the specified name.
    auto operator[](key_type key) noexcept -> mapped_type {
        // return existing value if found
        iterator iter = find(key);
        if (iter != end())
            return iter->second;

        // create the new node
        json_element element = new_element();
        insert_node(key, element);

        return element;
    }

private:
    /// @brief
    ///   For internal usage. Insert a serialized node. It is assumed that @p key does not exist.
    /// @param key
    ///   Key of the member to be inserted.
    /// @param value
    ///   The JSON member value to be inserted.
    /// @return
    ///   Iterator to the inserted object.
    NYAIO_API auto insert_node(key_type key, mapped_type value) noexcept -> iterator;
};

/// @brief
///   Try to get this JSON element as a JSON object.
/// @return
///   A JSON object if this JSON element is a JSON object. Otherwise, return an error code that
///   indicates this JSON element is not a JSON object.
inline auto json_element::object() const noexcept -> std::expected<json_object, json_errc> {
    if (is_object()) [[likely]]
        return json_object(*this);
    return std::unexpected(json_errc::invalid_type);
}

} // namespace nyaio

namespace nyaio {

/// @class json_array_const_iterator
/// @brief
///   Const iterator type for JSON array.
class json_array_const_iterator {
public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type        = json_element;
    using difference_type   = std::ptrdiff_t;
    using reference         = const json_element;

private:
    class pointer_proxy {
    public:
        explicit pointer_proxy(reference value) noexcept : m_value(value) {}

        auto operator->() noexcept -> reference * {
            return &m_value;
        }

        auto operator->() const noexcept -> const reference * {
            return &m_value;
        }

    private:
        value_type m_value;
    };

public:
    using pointer = pointer_proxy;

    /// @brief
    ///   For internal usage. Create a new JSON array iterator from payload.
    /// @param[in] node
    ///   Pointer to JSON array node payload.
    explicit json_array_const_iterator(detail::json_payload **node) noexcept : m_current(node) {}

    auto operator*() const noexcept -> reference {
        return reference(*m_current);
    }

    auto operator->() const noexcept -> pointer {
        return pointer(reference(*m_current));
    }

    auto operator[](difference_type offset) const noexcept -> reference {
        auto *node = m_current + offset;
        return reference(*node);
    }

    auto operator+(difference_type offset) const noexcept -> json_array_const_iterator {
        return json_array_const_iterator(m_current + offset);
    }

    auto operator-(difference_type offset) const noexcept -> json_array_const_iterator {
        return json_array_const_iterator(m_current - offset);
    }

    auto operator++() noexcept -> json_array_const_iterator & {
        m_current += 1;
        return *this;
    }

    auto operator--() noexcept -> json_array_const_iterator & {
        m_current -= 1;
        return *this;
    }

    auto operator++(int) noexcept -> json_array_const_iterator {
        json_array_const_iterator ret  = *this;
        m_current                     += 1;
        return ret;
    }

    auto operator--(int) noexcept -> json_array_const_iterator {
        json_array_const_iterator ret  = *this;
        m_current                     -= 1;
        return ret;
    }

    auto operator+=(difference_type offset) noexcept -> json_array_const_iterator & {
        m_current += offset;
        return *this;
    }

    auto operator-=(difference_type offset) noexcept -> json_array_const_iterator & {
        m_current -= offset;
        return *this;
    }

    auto operator-(json_array_const_iterator rhs) const noexcept -> difference_type {
        return m_current - rhs.m_current;
    }

    auto operator==(json_array_const_iterator rhs) const noexcept -> bool {
        return m_current == rhs.m_current;
    }

    auto operator<=>(json_array_const_iterator rhs) const noexcept -> std::strong_ordering {
        return m_current <=> rhs.m_current;
    }

    friend class nyaio::json_array;

private:
    detail::json_payload **m_current;
};

/// @class json_array_iterator
/// @brief
///   Iterator type for JSON array.
class json_array_iterator {
public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type        = json_element;
    using difference_type   = std::ptrdiff_t;
    using reference         = json_element;

private:
    class pointer_proxy {
    public:
        explicit pointer_proxy(reference value) noexcept : m_value(value) {}

        auto operator->() noexcept -> reference * {
            return &m_value;
        }

        auto operator->() const noexcept -> const reference * {
            return &m_value;
        }

    private:
        value_type m_value;
    };

public:
    using pointer = pointer_proxy;

    /// @brief
    ///   For internal usage. Create a new JSON array iterator from payload.
    /// @param[in] node
    ///   Pointer to JSON array node payload.
    explicit json_array_iterator(detail::json_payload **node) noexcept : m_current(node) {}

    auto operator*() const noexcept -> reference {
        return reference(*m_current);
    }

    auto operator->() const noexcept -> pointer {
        return pointer(reference(*m_current));
    }

    auto operator[](difference_type offset) const noexcept -> reference {
        auto *node = m_current + offset;
        return reference(*node);
    }

    auto operator+(difference_type offset) const noexcept -> json_array_iterator {
        return json_array_iterator(m_current + offset);
    }

    auto operator-(difference_type offset) const noexcept -> json_array_iterator {
        return json_array_iterator(m_current - offset);
    }

    auto operator++() noexcept -> json_array_iterator & {
        m_current += 1;
        return *this;
    }

    auto operator--() noexcept -> json_array_iterator & {
        m_current -= 1;
        return *this;
    }

    auto operator++(int) noexcept -> json_array_iterator {
        json_array_iterator ret  = *this;
        m_current               += 1;
        return ret;
    }

    auto operator--(int) noexcept -> json_array_iterator {
        json_array_iterator ret  = *this;
        m_current               -= 1;
        return ret;
    }

    auto operator+=(difference_type offset) noexcept -> json_array_iterator & {
        m_current += offset;
        return *this;
    }

    auto operator-=(difference_type offset) noexcept -> json_array_iterator & {
        m_current -= offset;
        return *this;
    }

    auto operator-(json_array_iterator rhs) const noexcept -> difference_type {
        return m_current - rhs.m_current;
    }

    auto operator==(json_array_iterator rhs) const noexcept -> bool {
        return m_current == rhs.m_current;
    }

    auto operator<=>(json_array_iterator rhs) const noexcept -> std::strong_ordering {
        return m_current <=> rhs.m_current;
    }

    operator json_array_const_iterator() const noexcept {
        return json_array_const_iterator(m_current);
    }

    friend class nyaio::json_array;

private:
    detail::json_payload **m_current;
};

/// @class json_array
/// @brief
///   Wrapper class for JSON array.
class json_array : public json_element {
public:
    using value_type             = json_element;
    using size_type              = size_t;
    using difference_type        = ptrdiff_t;
    using reference              = json_element;
    using const_reference        = const json_element;
    using iterator               = json_array_iterator;
    using const_iterator         = json_array_const_iterator;
    using pointer                = typename iterator::pointer;
    using const_pointer          = typename const_iterator::pointer;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    /// @brief
    ///   Get iterator to the first element of this array.
    /// @return
    ///   Iterator to the first element of this array.
    [[nodiscard]]
    auto begin() noexcept -> iterator {
        return iterator(m_payload->payload.array.first);
    }

    /// @brief
    ///   Get iterator to the first element of this array.
    /// @return
    ///   Iterator to the first element of this array.
    [[nodiscard]]
    auto begin() const noexcept -> const_iterator {
        return const_iterator(m_payload->payload.array.first);
    }

    /// @brief
    ///   Get iterator to the first element of this array.
    /// @return
    ///   Iterator to the first element of this array.
    [[nodiscard]]
    auto cbegin() const noexcept -> const_iterator {
        return begin();
    }

    /// @brief
    ///   Get iterator to the element after the last element in this array.
    /// @return
    ///   Iterator to the element after the last element in this array.
    [[nodiscard]]
    auto end() noexcept -> iterator {
        return iterator(m_payload->payload.array.last);
    }

    /// @brief
    ///   Get iterator to the element after the last element in this array.
    /// @return
    ///   Iterator to the element after the last element in this array.
    [[nodiscard]]
    auto end() const noexcept -> const_iterator {
        return const_iterator(m_payload->payload.array.last);
    }

    /// @brief
    ///   Get iterator to the element after the last element in this array.
    /// @return
    ///   Iterator to the element after the last element in this array.
    [[nodiscard]]
    auto cend() const noexcept -> const_iterator {
        return end();
    }

    /// @brief
    ///   Get reverse iterator to the first element of the reversed @c json_array.
    /// @return
    ///   Reverse iterator to the first element of the reversed @c json_array.
    [[nodiscard]]
    auto rbegin() noexcept -> reverse_iterator {
        return reverse_iterator(end());
    }

    /// @brief
    ///   Get reverse iterator to the first element of the reversed @c json_array.
    /// @return
    ///   Reverse iterator to the first element of the reversed @c json_array.
    [[nodiscard]]
    auto rbegin() const noexcept -> const_reverse_iterator {
        return const_reverse_iterator(end());
    }

    /// @brief
    ///   Get reverse iterator to the first element of the reversed @c json_array.
    /// @return
    ///   Reverse iterator to the first element of the reversed @c json_array.
    [[nodiscard]]
    auto crbegin() const noexcept -> const_reverse_iterator {
        return const_reverse_iterator(end());
    }

    /// @brief
    ///   Get reverse iterator to the element after the last element of the reversed @c json_array.
    /// @return
    ///   Reverse iterator to the element after the last element of the reversed @c json_array.
    [[nodiscard]]
    auto rend() noexcept -> reverse_iterator {
        return reverse_iterator(begin());
    }

    /// @brief
    ///   Get reverse iterator to the element after the last element of the reversed @c json_array.
    /// @return
    ///   Reverse iterator to the element after the last element of the reversed @c json_array.
    [[nodiscard]]
    auto rend() const noexcept -> const_reverse_iterator {
        return const_reverse_iterator(begin());
    }

    /// @brief
    ///   Get reverse iterator to the element after the last element of the reversed @c json_array.
    /// @return
    ///   Reverse iterator to the element after the last element of the reversed @c json_array.
    [[nodiscard]]
    auto crend() const noexcept -> const_reverse_iterator {
        return const_reverse_iterator(begin());
    }

    /// @brief
    ///   Checks if this JSON array is empty.
    /// @retval true
    ///   This JSON array is an empty array.
    /// @retval false
    ///   This JSON array is not an empty array.
    [[nodiscard]]
    auto empty() const noexcept -> bool {
        auto &a = m_payload->payload.array;
        return a.first == a.last;
    }

    /// @brief
    ///   Get number of elements in this JSON array.
    /// @return
    ///   Number of elements in this JSON array.
    [[nodiscard]]
    auto size() const noexcept -> size_type {
        auto &a = m_payload->payload.array;
        return static_cast<size_type>(a.last - a.first);
    }

    /// @brief
    ///   Reserve memory for this JSON array to store at least @p count number of elements.
    /// @param count
    ///   Number of elements to be held in this JSON array.
    auto reserve(size_type count) noexcept -> void {
        auto *alloc = m_payload->allocator;
        auto &a     = m_payload->payload.array;

        auto cap = static_cast<size_type>(a.buffer_end - a.first);
        if (cap >= count) [[unlikely]]
            return;

        auto size    = static_cast<size_type>(a.last - a.first);
        auto *buffer = alloc->allocate<detail::json_payload *>(count);
        memcpy(buffer, a.first, size * sizeof(detail::json_payload *));

        a.first      = buffer;
        a.last       = buffer + size;
        a.buffer_end = buffer + count;
    }

    /// @brief
    ///   Get number of elements that can be held in current allocated memory.
    /// @return
    ///   Number of elements that can be held in current allocated memory.
    [[nodiscard]]
    auto capacity() const noexcept -> size_type {
        auto &a = m_payload->payload.array;
        return static_cast<size_type>(a.buffer_end - a.first);
    }

    /// @brief
    ///   Get the first element in this JSON array. It is undefined behavior if this JSON array is
    ///   an empty array.
    /// @return
    ///   The first element in this JSON array.
    [[nodiscard]]
    auto front() noexcept -> reference {
        auto &a = m_payload->payload.array;
        return reference(*a.first);
    }

    /// @brief
    ///   Get the first element in this JSON array. It is undefined behavior if this JSON array is
    ///   an empty array.
    /// @return
    ///   The first element in this JSON array.
    [[nodiscard]]
    auto front() const noexcept -> const_reference {
        auto &a = m_payload->payload.array;
        return const_reference(*a.first);
    }

    /// @brief
    ///   Get the last element in this JSON array. It is undefined behavior if this JSON array is an
    ///   empty array.
    /// @return
    ///   The last element in this JSON array.
    [[nodiscard]]
    auto back() noexcept -> reference {
        auto &a = m_payload->payload.array;
        return reference(*(a.last - 1));
    }

    /// @brief
    ///   Get the last element in this JSON array. It is undefined behavior if this JSON array is an
    ///   empty array.
    /// @return
    ///   The last element in this JSON array.
    [[nodiscard]]
    auto back() const noexcept -> const_reference {
        auto &a = m_payload->payload.array;
        return const_reference(*(a.last - 1));
    }

    /// @brief
    ///   Access the specified element with index.
    /// @param index
    ///   Index of the element to be accessed. It is undefined behavior if @p index is out of range.
    auto operator[](size_type index) noexcept -> reference {
        auto &a = m_payload->payload.array;
        return reference(a.first[index]);
    }

    /// @brief
    ///   Access the specified element with index.
    /// @param index
    ///   Index of the element to be accessed. It is undefined behavior if @p index is out of range.
    auto operator[](size_type index) const noexcept -> const_reference {
        auto &a = m_payload->payload.array;
        return const_reference(a.first[index]);
    }

    /// @brief
    ///   Remove all elements. @c size() returns 0 after this call.
    auto clear() noexcept -> void {
        auto &a = m_payload->payload.array;
        a.last  = a.first;
    }

    /// @brief
    ///   Insert a new element at the specified place.
    /// @tparam T
    ///   Type of the object to be serialized into JSON element.
    /// @param pos
    ///   Position to insert the new element.
    /// @param[in] value
    ///   The object to be serialized into JSON element.
    /// @throws json_error
    ///   Thrown if failed to serialize the object into JSON element.
    template <json_serializable T>
    auto insert(const_iterator pos, T &&value) -> iterator {
        value_type element = new_element();
        json_errc error =
            json_serializer<std::remove_cvref_t<T>>::to_json(std::forward<T>(value), element);

        if (error != json_errc::ok) [[unlikely]]
            throw json_error(error);

        return insert_node(static_cast<size_type>(pos - begin()), element);
    }

    /// @brief
    ///   Insert a new element at the specified place.
    /// @tparam T
    ///   Type of the object to be serialized into JSON element.
    /// @param index
    ///   Position to insert the new element. It is undefined behavior if @p index is greater than
    ///   size of this array. This method is the same as @c push_back if @p index is equal to size.
    /// @param[in] value
    ///   The object to be serialized into JSON element.
    template <json_serializable T>
    auto insert(size_type index, T &&value) -> reference {
        value_type element = new_element();
        json_errc error =
            json_serializer<std::remove_cvref_t<T>>::to_json(std::forward<T>(value), element);

        if (error != json_errc::ok) [[unlikely]]
            throw json_error(error);

        return *insert_node(index, element);
    }

    /// @brief
    ///   Insert a new element at the specified place.
    /// @tparam T
    ///   Type of the object to be serialized into JSON element.
    /// @param pos
    ///   Position to insert the new element.
    /// @param[in] value
    ///   The object to be serialized into JSON element.
    template <json_serializable T>
    auto emplace(const_iterator pos, T &&value) -> iterator {
        return insert(pos, std::forward<T>(value));
    }

    /// @brief
    ///   Insert a new element at the specified place.
    /// @tparam T
    ///   Type of the object to be serialized into JSON element.
    /// @param index
    ///   Position to insert the new element. It is undefined behavior if @p index is greater than
    ///   size of this array. This method is the same as @c push_back if @p index is equal to size.
    /// @param[in] value
    ///   The object to be serialized into JSON element.
    template <json_serializable T>
    auto emplace(size_type index, T &&value) -> reference {
        return insert(index, std::forward<T>(value));
    }

    /// @brief
    ///   Remove the element at @p position.
    /// @param position
    ///   Position of the element to be removed.
    /// @return
    ///   Iterator to the element after the removed element.
    auto erase(const_iterator position) noexcept -> iterator {
        auto &a = m_payload->payload.array;

        auto *node = position.m_current;
        auto *next = node + 1;
        auto count = static_cast<size_type>(a.last - next);

        memmove(node, next, count * sizeof(detail::json_payload *));
        a.last = node + count;

        return iterator(node);
    }

    /// @brief
    ///   Remove elements between @p first (included) and @p last (excluded).
    /// @param first
    ///   Iterator to the first element to be removed.
    /// @param last
    ///   Iterator to the element after the last element to be removed.
    /// @param
    ///   Iterator to the element after the last removed element.
    auto erase(const_iterator first, const_iterator last) noexcept -> iterator {
        auto &a = m_payload->payload.array;

        auto *node = first.m_current;
        auto *next = last.m_current;
        auto count = static_cast<size_type>(a.last - next);

        memmove(node, next, count * sizeof(detail::json_payload *));
        a.last = node + count;

        return iterator(node);
    }

    /// @brief
    ///   Append the given element at the end of this array.
    /// @tparam T
    ///   Type of the object to be serialized to JSON.
    /// @param[in] value
    ///   The object to be appended.
    template <json_serializable T>
    auto push_back(T &&value) -> void {
        value_type element = new_element();

        json_errc error =
            json_serializer<std::remove_cvref_t<T>>::to_json(std::forward<T>(value), element);

        if (error != json_errc::ok) [[unlikely]]
            throw json_error(error);

        auto &a = m_payload->payload.array;
        if (a.last == a.buffer_end) [[unlikely]]
            reserve(std::max<size_type>(8, size() * 2));

        *a.last  = element.m_payload;
        a.last  += 1;
    }

    /// @brief
    ///   Append the given element at the end of this array.
    /// @tparam T
    ///   Type of the object to be serialized to JSON.
    /// @param[in] value
    ///   The object to be appended.
    /// @return
    ///   Reference to the appended JSON element.
    /// @throws json_error
    ///   Thrown if failed to serialize the object into JSON element.
    template <json_serializable T>
    auto emplace_back(T &&value) -> reference {
        value_type element = new_element();

        json_errc error =
            json_serializer<std::remove_cvref_t<T>>::to_json(std::forward<T>(value), element);

        if (error != json_errc::ok) [[unlikely]]
            throw json_error(error);

        auto &a = m_payload->payload.array;
        if (a.last == a.buffer_end) [[unlikely]]
            reserve(std::max<size_type>(8, size() * 2));

        *a.last  = element.m_payload;
        a.last  += 1;

        return element;
    }

    /// @brief
    ///   Remove the last element of this array. It is undefined behavior if this is an empty array.
    auto pop_back() noexcept -> void {
        auto &a  = m_payload->payload.array;
        a.last  -= 1;
    }

    /// @brief
    ///   Resize this array to contain @p count elements. Extra elements will be removed if current
    ///   size is greater than @p count. Null JSON elements will be appended if @p count is greater
    ///   than current size.
    /// @param count
    ///   New size of this array.
    NYAIO_API auto resize(size_type count) noexcept -> void;

private:
    /// @brief
    ///   For internal usage. Insert an element at the specified place.
    /// @param index
    ///   Position to insert the element.
    /// @param element
    ///   The element to be inserted.
    /// @return
    ///   Iterator to the inserted element.
    NYAIO_API auto insert_node(size_type index, value_type element) noexcept -> iterator;
};

/// @brief
///   Try to get this JSON element as a JSON array.
/// @return
///   A JSON array if this JSON element is a JSON array. Otherwise, return an error code that
///   indicates this JSON element is not a JSON array.
inline auto json_element::array() const noexcept -> std::expected<json_array, json_errc> {
    if (is_array()) [[likely]]
        return json_array(*this);
    return std::unexpected(json_errc::invalid_type);
}

} // namespace nyaio

namespace nyaio {

/// @class json_document
/// @brief
///   JSON document holds a JSON tree.
class json_document {
public:
    /// @brief
    ///   Create an empty JSON document. An empty JSON document has a null root.
    json_document() noexcept : m_allocator(std::make_unique<detail::json_allocator>()), m_root() {
        auto *payload      = m_allocator->allocate<detail::json_payload>(1);
        payload->type      = json_type::null;
        payload->allocator = m_allocator.get();
        m_root.m_payload   = payload;
    }

    /// @brief
    ///   Create a new JSON document from a JSON sub-tree.
    /// @param element
    ///   Root JSON element node to be copied. Deep copy is performed.
    explicit json_document(json_element element) noexcept
        : m_allocator(std::make_unique<detail::json_allocator>()),
          m_root(element.clone(m_allocator.get())) {}

    /// @brief
    ///   Perform a deep copy of the JSON document.
    /// @param other
    ///   The JSON document to be copied.
    json_document(const json_document &other) noexcept
        : m_allocator(std::make_unique<detail::json_allocator>()),
          m_root(other.m_root.clone(m_allocator.get())) {}

    /// @brief
    ///   Move constructor of @c json_document.
    /// @param[in, out] other
    ///   The JSON document to be moved. The moved JSON document is in a valid but undefined state.
    ///   The moved JSON document should not be used anymore.
    json_document(json_document &&other) noexcept
        : m_allocator(std::move(other.m_allocator)), m_root(other.m_root) {}

    /// @brief
    ///   Release all resources and destroy this JSON document.
    ~json_document() = default;

    /// @brief
    ///   Copy assignment of @c json_document. Perform deep copy of the JSON tree.
    /// @param other
    ///   The JSON document to be copied.
    /// @return
    ///   Reference to this JSON document.
    auto operator=(const json_document &other) noexcept -> json_document & {
        if (this == &other) [[unlikely]]
            return *this;

        m_allocator->clear();

        // do not clone if other is empty
        if (other.m_allocator == nullptr) [[unlikely]]
            return *this;

        m_root = other.m_root.clone(m_allocator.get());

        return *this;
    }

    /// @brief
    ///   Move assignment of @c json_document.
    /// @param[in, out] other
    ///   The JSON document to be moved. The moved JSON document is in a valid but undefined state.
    ///   The moved JSON document should not be used anymore.
    /// @return
    ///   Reference to this JSON document.
    auto operator=(json_document &&other) noexcept -> json_document & {
        if (this == &other) [[unlikely]]
            return *this;

        m_allocator = std::move(other.m_allocator);
        m_root      = other.m_root;

        return *this;
    }

    /// @brief
    ///   Get root node of this JSON document.
    /// @return
    ///   Root node of this JSON document.
    [[nodiscard]]
    auto root() const noexcept -> json_element {
        return m_root;
    }

    /// @brief
    ///   Dump this JSON document into a string. The output string will be compressed into one line.
    /// @return
    ///   An UTF-8 string that represents dump result of this JSON element.
    [[nodiscard]]
    auto dump() const noexcept -> std::string {
        return m_root.dump();
    }

    /// @brief
    ///   Dump this JSON element into a string.
    /// @param indent
    ///   Indention unit.
    /// @return
    ///   An UTF-8 string that represents dump result of this JSON element.
    [[nodiscard]]
    auto dump(std::string_view indent) const noexcept -> std::string {
        return m_root.dump(indent);
    }

    /// @brief
    ///   Try to initialize this JSON document from a JSON string.
    ///   TODO: implement this.
    /// @param string
    ///   A string that contains the JSON content to be parsed.
    NYAIO_API auto parse(std::string_view string) noexcept -> json_errc;

private:
    std::unique_ptr<detail::json_allocator> m_allocator;
    json_element m_root;
};

} // namespace nyaio

namespace nyaio {

template <>
struct json_serializer<std::nullptr_t> {
    static auto to_json(std::nullptr_t, json_element j) noexcept -> json_errc {
        j.set_null();
        return json_errc::ok;
    }

    static auto from_json(std::nullptr_t &value, json_element j) noexcept -> json_errc {
        if (j.is_null()) [[likely]] {
            value = nullptr;
            return json_errc::ok;
        }

        return json_errc::invalid_type;
    }
};

template <>
struct json_serializer<bool> {
    static auto to_json(bool value, json_element j) noexcept -> json_errc {
        j.set_boolean(value);
        return json_errc::ok;
    }

    static auto from_json(bool &value, json_element j) noexcept -> json_errc {
        auto result = j.boolean();
        if (result.has_value()) [[likely]] {
            value = *result;
            return json_errc::ok;
        }

        return json_errc::invalid_type;
    }
};

template <>
struct json_serializer<int8_t> {
    static auto to_json(int8_t value, json_element j) noexcept -> json_errc {
        j.set_integer(value);
        return json_errc::ok;
    }

    static auto from_json(int8_t &value, json_element j) noexcept -> json_errc {
        auto result = j.integer();
        if (result.has_value()) [[likely]] {
            value = static_cast<int8_t>(*result);
            return json_errc::ok;
        }

        return json_errc::invalid_type;
    }
};

template <>
struct json_serializer<int16_t> {
    static auto to_json(int16_t value, json_element j) noexcept -> json_errc {
        j.set_integer(value);
        return json_errc::ok;
    }

    static auto from_json(int16_t &value, json_element j) noexcept -> json_errc {
        auto result = j.integer();
        if (result.has_value()) [[likely]] {
            value = static_cast<int16_t>(*result);
            return json_errc::ok;
        }

        return json_errc::invalid_type;
    }
};

template <>
struct json_serializer<int32_t> {
    static auto to_json(int32_t value, json_element j) noexcept -> json_errc {
        j.set_integer(value);
        return json_errc::ok;
    }

    static auto from_json(int32_t &value, json_element j) noexcept -> json_errc {
        auto result = j.integer();
        if (result.has_value()) [[likely]] {
            value = static_cast<int32_t>(*result);
            return json_errc::ok;
        }

        return json_errc::invalid_type;
    }
};

template <>
struct json_serializer<int64_t> {
    static auto to_json(int64_t value, json_element j) noexcept -> json_errc {
        j.set_integer(value);
        return json_errc::ok;
    }

    static auto from_json(int64_t &value, json_element j) noexcept -> json_errc {
        auto result = j.integer();
        if (result.has_value()) [[likely]] {
            value = *result;
            return json_errc::ok;
        }

        return json_errc::invalid_type;
    }
};

template <>
struct json_serializer<uint8_t> {
    static auto to_json(uint8_t value, json_element j) noexcept -> json_errc {
        j.set_integer(value);
        return json_errc::ok;
    }

    static auto from_json(uint8_t &value, json_element j) noexcept -> json_errc {
        auto result = j.integer();
        if (result.has_value()) [[likely]] {
            value = static_cast<uint8_t>(*result);
            return json_errc::ok;
        }

        return json_errc::invalid_type;
    }
};

template <>
struct json_serializer<uint16_t> {
    static auto to_json(uint16_t value, json_element j) noexcept -> json_errc {
        j.set_integer(value);
        return json_errc::ok;
    }

    static auto from_json(uint16_t &value, json_element j) noexcept -> json_errc {
        auto result = j.integer();
        if (result.has_value()) [[likely]] {
            value = static_cast<uint16_t>(*result);
            return json_errc::ok;
        }

        return json_errc::invalid_type;
    }
};

template <>
struct json_serializer<uint32_t> {
    static auto to_json(uint32_t value, json_element j) noexcept -> json_errc {
        j.set_integer(value);
        return json_errc::ok;
    }

    static auto from_json(uint32_t &value, json_element j) noexcept -> json_errc {
        auto result = j.integer();
        if (result.has_value()) [[likely]] {
            value = static_cast<uint32_t>(*result);
            return json_errc::ok;
        }

        return json_errc::invalid_type;
    }
};

template <>
struct json_serializer<uint64_t> {
    static auto to_json(uint64_t value, json_element j) noexcept -> json_errc {
        j.set_integer(static_cast<int64_t>(value));
        return json_errc::ok;
    }

    static auto from_json(uint64_t &value, json_element j) noexcept -> json_errc {
        auto result = j.integer();
        if (result.has_value()) [[likely]] {
            value = static_cast<uint64_t>(*result);
            return json_errc::ok;
        }

        return json_errc::invalid_type;
    }
};

template <>
struct json_serializer<float> {
    static auto to_json(float value, json_element j) noexcept -> json_errc {
        j.set_floating_point(value);
        return json_errc::ok;
    }

    static auto from_json(float &value, json_element j) noexcept -> json_errc {
        auto result = j.floating_point();
        if (result.has_value()) [[likely]] {
            value = static_cast<float>(*result);
            return json_errc::ok;
        }

        return json_errc::invalid_type;
    }
};

template <>
struct json_serializer<double> {
    static auto to_json(double value, json_element j) noexcept -> json_errc {
        j.set_floating_point(value);
        return json_errc::ok;
    }

    static auto from_json(double &value, json_element j) noexcept -> json_errc {
        auto result = j.floating_point();
        if (result.has_value()) [[likely]] {
            value = *result;
            return json_errc::ok;
        }

        return json_errc::invalid_type;
    }
};

template <>
struct json_serializer<std::string_view> {
    static auto to_json(std::string_view value, json_element j) noexcept -> json_errc {
        j.set_string(value);
        return json_errc::ok;
    }

    static auto from_json(std::string_view &value, json_element j) noexcept -> json_errc {
        auto result = j.string();
        if (result.has_value()) [[likely]] {
            value = *result;
            return json_errc::ok;
        }

        return json_errc::invalid_type;
    }
};

template <>
struct json_serializer<std::string> {
    static auto to_json(const std::string &value, json_element j) noexcept -> json_errc {
        j.set_string(value);
        return json_errc::ok;
    }

    static auto from_json(std::string &value, json_element j) noexcept -> json_errc {
        auto result = j.string();
        if (result.has_value()) [[likely]] {
            value.assign(*result);
            return json_errc::ok;
        }

        return json_errc::invalid_type;
    }
};

template <class T>
struct json_serializer<std::vector<T>> {
    static auto to_json(const std::vector<T> &value, json_element j) -> json_errc
        requires(json_serializable<T>)
    {
        json_errc error = json_errc::ok;

        j.set_array();
        auto array = static_cast<json_array>(j);
        array.resize(value.size());

        for (size_t i = 0; i < value.size(); ++i) {
            error = json_serializer<T>::to_json(value[i], array[i]);
            if (error != json_errc::ok) [[unlikely]]
                return error;
        }

        return json_errc::ok;
    }

    static auto from_json(std::vector<T> &value, json_element j) -> json_errc
        requires(json_deserializable<T>)
    {
        auto array = j.array();
        if (!array.has_value())
            return json_errc::invalid_type;

        value.resize(array->size());
        for (size_t i = 0; i < array->size(); ++i) {
            auto result = json_serializer<T>::from_json(value[i], (*array)[i]);
            if (result != json_errc::ok)
                return result;
        }

        return json_errc::ok;
    }
};

template <class T, size_t N>
struct json_serializer<T[N]> {
    static auto to_json(const T (&value)[N], json_element j) -> json_errc
        requires(json_serializable<T>)
    {
        json_errc error = json_errc::ok;

        j.set_array();
        auto array = static_cast<json_array>(j);
        array.resize(N);

        for (size_t i = 0; i < N; ++i) {
            error = json_serializer<T>::to_json(value[i], array[i]);
            if (error != json_errc::ok) [[unlikely]]
                return error;
        }

        return json_errc::ok;
    }

    static auto from_json(T (&value)[N], json_element j) -> json_errc
        requires(json_deserializable<T>)
    {
        auto array = j.array();
        if (!array.has_value())
            return json_errc::invalid_type;

        if (array->size() != N)
            return json_errc::invalid_type;

        for (size_t i = 0; i < N; ++i) {
            auto result = json_serializer<T>::from_json(value[i], (*array)[i]);
            if (result != json_errc::ok)
                return result;
        }

        return json_errc::ok;
    }
};

template <class T>
struct json_serializer<std::map<std::string, T>> {
    static auto to_json(const std::map<std::string, T> &value, json_element j) noexcept -> json_errc
        requires(json_serializable<T>)
    {
        json_errc error = json_errc::ok;

        j.set_object();
        auto object = static_cast<json_object>(j);

        for (const auto &kv : value) {
            error = object[kv.first].from(kv.second);
            if (error != json_errc::ok) [[unlikely]]
                return error;
        }

        return json_errc::ok;
    }

    static auto from_json(std::map<std::string, T> &value, json_element j) -> json_errc
        requires(json_deserializable<T> && std::default_initializable<T>)
    {
        if (!j.is_object())
            return json_errc::invalid_type;

        json_errc error = json_errc::ok;
        auto object     = static_cast<json_object>(j);

        value.clear();
        for (const auto kv : object) {
            error = json_serializer<std::remove_cvref_t<T>>::from_json(value[kv.first], kv.second);
            if (error != json_errc::ok) [[unlikely]]
                return error;
        }

        return json_errc::ok;
    }
};

} // namespace nyaio
