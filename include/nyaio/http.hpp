#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace nyaio {

/// @enum http_version
/// @brief
///   HTTP version.
enum http_version : uint32_t {
    http_version_1_0 = (1 << 16) | 0,
    http_version_1_1 = (1 << 16) | 1,
    http_version_2_0 = (2 << 16) | 0,
};

/// @enum http_method
/// @brief
///   HTTP method.
enum http_method : uint16_t {
    http_method_get     = 0,
    http_method_head    = 1,
    http_method_post    = 2,
    http_method_put     = 3,
    http_method_delete  = 4,
    http_method_connect = 5,
    http_method_options = 6,
    http_method_trace   = 7,
    http_method_patch   = 8,
};

/// @enum http_status
/// @brief
///   HTTP status code.
enum http_status : uint16_t {
    http_status_continue                        = 100,
    http_status_switching_protocols             = 101,
    http_status_processing                      = 102,
    http_status_early_hints                     = 103,
    http_status_ok                              = 200,
    http_status_created                         = 201,
    http_status_accepted                        = 202,
    http_status_non_authoritative_information   = 203,
    http_status_no_content                      = 204,
    http_status_reset_content                   = 205,
    http_status_partial_content                 = 206,
    http_status_multi_status                    = 207,
    http_status_already_reported                = 208,
    http_status_im_used                         = 226,
    http_status_multiple_choices                = 300,
    http_status_moved_permanently               = 301,
    http_status_found                           = 302,
    http_status_see_other                       = 303,
    http_status_not_modified                    = 304,
    http_status_temporary_redirect              = 307,
    http_status_permanent_redirect              = 308,
    http_status_bad_request                     = 400,
    http_status_unauthorized                    = 401,
    http_status_payment_required                = 402,
    http_status_forbidden                       = 403,
    http_status_not_found                       = 404,
    http_status_method_not_allowed              = 405,
    http_status_not_acceptable                  = 406,
    http_status_proxy_authentication_required   = 407,
    http_status_request_timeout                 = 408,
    http_status_conflict                        = 409,
    http_status_gone                            = 410,
    http_status_length_required                 = 411,
    http_status_precondition_failed             = 412,
    http_status_payload_too_large               = 413,
    http_status_uri_too_long                    = 414,
    http_status_unsupported_media_type          = 415,
    http_status_range_not_satisfiable           = 416,
    http_status_expectation_failed              = 417,
    http_status_im_a_teapot                     = 418,
    http_status_misdirected_request             = 421,
    http_status_unprocessable_entity            = 422,
    http_status_locked                          = 423,
    http_status_failed_dependency               = 424,
    http_status_too_early                       = 425,
    http_status_upgrade_required                = 426,
    http_status_precondition_required           = 428,
    http_status_too_many_requests               = 429,
    http_status_request_header_fields_too_large = 431,
    http_status_unavailable_for_legal_reasons   = 451,
    http_status_internal_server_error           = 500,
    http_status_not_implemented                 = 501,
    http_status_bad_gateway                     = 502,
    http_status_service_unavailable             = 503,
    http_status_gateway_timeout                 = 504,
    http_status_http_version_not_supported      = 505,
    http_status_variant_also_negotiates         = 506,
    http_status_insufficient_storage            = 507,
    http_status_loop_detected                   = 508,
    http_status_not_extended                    = 510,
    http_status_network_authentication_required = 511,
};

/// @enum http_content_coding
/// @brief
///   HTTP content coding.
enum class http_content_coding : uint16_t {
    undefined    = 0,
    aes128gcm    = 1,
    brotli       = 2,
    compress     = 3,
    deflate      = 4,
    exi          = 5,
    gzip         = 6,
    identity     = 7,
    pack200_gzip = 8,
    zstd         = 9,
};

/// @enum http_transfer_coding
/// @brief
///   HTTP transfer coding. Defined by RFC 9112.
enum class http_transfer_coding : uint16_t {
    undefined = 0,
    chunked   = 1,
    compress  = 2,
    deflate   = 3,
    gzip      = 4,
    identity  = 5,
    trailers  = 6,
};

namespace detail {

/// @struct http_header_node
/// @brief
///   For internal usage. HTTP header node type.
struct http_header_node {
    size_t next;
    std::string key;
    std::string value;
};

/// @struct http_header_hasher
/// @brief
///   For internal usage. HTTP header key hasher.
struct http_header_hasher {
    constexpr auto operator()(std::string_view key) const noexcept -> size_t {
        constexpr size_t prime  = (sizeof(size_t) >= 8) ? 1099511628211ULL : 16777619U;
        constexpr size_t offset = (sizeof(size_t) >= 8) ? 14695981039346656037ULL : 2166136261U;

        size_t hash = offset;
        for (char c : key) {
            if (c >= 'A' && c <= 'Z')
                c += 32;

            hash ^= static_cast<size_t>(c);
            hash *= prime;
        }

        return hash;
    }
};

/// @struct http_header_key_equal
/// @brief
///   For internal usage. HTTP header key equal.
struct http_header_key_equal {
    constexpr auto operator()(std::string_view lhs, std::string_view rhs) const noexcept -> bool {
        if (lhs.size() != rhs.size())
            return false;

        for (auto i = lhs.begin(), j = rhs.begin(); i != lhs.end(); ++i, ++j) {
            if (tolower(*i) != tolower(*j))
                return false;
        }

        return true;
    }
};

} // namespace detail

/// @class http_header_const_iterator
/// @brief
///   HTTP header constant element iterator.
class http_header_const_iterator {
public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type        = std::pair<std::string, std::string>;
    using difference_type   = std::ptrdiff_t;
    using reference         = std::pair<const std::string &, const std::string &>;

private:
    using internal_type = typename std::vector<detail::http_header_node>::const_iterator;

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
    ///   For internal usage. Create a new iterator for HTTP header.
    /// @param[in] position
    ///   Internal iterator to the specified HTTP header field.
    explicit http_header_const_iterator(internal_type position) noexcept : m_current(position) {}

    auto operator*() const noexcept -> reference {
        return {m_current->key, m_current->value};
    }

    auto operator->() const noexcept -> pointer {
        return pointer({m_current->key, m_current->value});
    }

    auto operator[](difference_type offset) const noexcept -> reference {
        auto &element = m_current[offset];
        return {element.key, element.value};
    }

    auto operator+(difference_type offset) const noexcept -> http_header_const_iterator {
        return http_header_const_iterator(m_current + offset);
    }

    auto operator-(difference_type offset) const noexcept -> http_header_const_iterator {
        return http_header_const_iterator(m_current - offset);
    }

    auto operator++() noexcept -> http_header_const_iterator & {
        ++m_current;
        return *this;
    }

    auto operator--() noexcept -> http_header_const_iterator & {
        --m_current;
        return *this;
    }

    auto operator++(int) noexcept -> http_header_const_iterator {
        auto ret = *this;
        ++m_current;
        return ret;
    }

    auto operator--(int) noexcept -> http_header_const_iterator {
        auto ret = *this;
        --m_current;
        return ret;
    }

    auto operator+=(difference_type offset) noexcept -> http_header_const_iterator & {
        m_current += offset;
        return *this;
    }

    auto operator-=(difference_type offset) noexcept -> http_header_const_iterator & {
        m_current -= offset;
        return *this;
    }

    auto operator-(http_header_const_iterator rhs) const noexcept -> difference_type {
        return m_current - rhs.m_current;
    }

    auto operator==(http_header_const_iterator rhs) const noexcept -> bool {
        return m_current == rhs.m_current;
    }

    auto operator<=>(http_header_const_iterator rhs) const noexcept -> std::strong_ordering {
        return m_current <=> rhs.m_current;
    }

    friend class http_header;

private:
    internal_type m_current;
};

/// @class http_header_iterator
/// @brief
///   HTTP header element iterator.
class http_header_iterator {
public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type        = std::pair<std::string, std::string>;
    using difference_type   = std::ptrdiff_t;
    using reference         = std::pair<const std::string &, std::string &>;

private:
    using internal_type = typename std::vector<detail::http_header_node>::iterator;

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
    ///   For internal usage. Create a new iterator for HTTP header.
    /// @param[in] position
    ///   Internal iterator to the specified HTTP header field.
    explicit http_header_iterator(internal_type position) noexcept : m_current(position) {}

    auto operator*() const noexcept -> reference {
        return {m_current->key, m_current->value};
    }

    auto operator->() const noexcept -> pointer {
        return pointer({m_current->key, m_current->value});
    }

    auto operator[](difference_type offset) const noexcept -> reference {
        auto &element = m_current[offset];
        return {element.key, element.value};
    }

    auto operator+(difference_type offset) const noexcept -> http_header_iterator {
        return http_header_iterator(m_current + offset);
    }

    auto operator-(difference_type offset) const noexcept -> http_header_iterator {
        return http_header_iterator(m_current - offset);
    }

    auto operator++() noexcept -> http_header_iterator & {
        ++m_current;
        return *this;
    }

    auto operator--() noexcept -> http_header_iterator & {
        --m_current;
        return *this;
    }

    auto operator++(int) noexcept -> http_header_iterator {
        auto ret = *this;
        ++m_current;
        return ret;
    }

    auto operator--(int) noexcept -> http_header_iterator {
        auto ret = *this;
        --m_current;
        return ret;
    }

    auto operator+=(difference_type offset) noexcept -> http_header_iterator & {
        m_current += offset;
        return *this;
    }

    auto operator-=(difference_type offset) noexcept -> http_header_iterator & {
        m_current -= offset;
        return *this;
    }

    auto operator-(http_header_iterator rhs) const noexcept -> difference_type {
        return m_current - rhs.m_current;
    }

    auto operator==(http_header_iterator rhs) const noexcept -> bool {
        return m_current == rhs.m_current;
    }

    auto operator<=>(http_header_iterator rhs) const noexcept -> std::strong_ordering {
        return m_current <=> rhs.m_current;
    }

    operator http_header_const_iterator() const noexcept {
        return http_header_const_iterator(m_current);
    }

    friend class http_header;

private:
    internal_type m_current;
};

/// @class http_header
/// @brief
///   HTTP header. HTTP header key is case-insensitive. This HTTP header implementation is not a
///   multi-map. Instead, duplicate HTTP header fields will be compressed into one line with comma
///   separated values.
class http_header {
public:
    using key_type               = std::string;
    using mapped_type            = std::string;
    using value_type             = std::pair<key_type, mapped_type>;
    using size_type              = std::size_t;
    using difference_type        = std::ptrdiff_t;
    using hasher                 = detail::http_header_hasher;
    using key_equal              = detail::http_header_key_equal;
    using reference              = std::pair<const key_type &, mapped_type &>;
    using const_reference        = std::pair<const key_type &, const mapped_type &>;
    using iterator               = http_header_iterator;
    using const_iterator         = http_header_const_iterator;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    /// @brief
    ///   Create an empty HTTP header.
    http_header() noexcept = default;

    /// @brief
    ///   Copy constructor of @c http_header. Deep copy of @c http_header may be slow.
    /// @param other
    ///   The @c http_header to be copied from.
    NYAIO_API http_header(const http_header &other) noexcept;

    /// @brief
    ///   Move constructor of @c http_header.
    /// @param[in, out] other
    ///   The @c http_header to be moved from. The moved @c http_header is in a valid but undefined
    ///   state.
    http_header(http_header &&other) noexcept = default;

    /// @brief
    ///   Destroy this @c http_header.
    NYAIO_API ~http_header();

    /// @brief
    ///   Copy assignment of @c http_header. Deep copy of @c http_header may be slow.
    /// @param other
    ///   The @c http_header to be copied from.
    /// @return
    ///   Reference to this @c http_header.
    NYAIO_API auto operator=(const http_header &other) noexcept -> http_header &;

    /// @brief
    ///   Move assignment of @c http_header.
    /// @param[in, out] other
    ///   The @c http_header to be moved from. The moved @c http_header is in a valid but undefined
    ///   state.
    /// @return
    ///   Reference to this @c http_header.
    NYAIO_API auto operator=(http_header &&other) noexcept -> http_header &;

    /// @brief
    ///   Get iterator to the first element of this @c http_header.
    /// @return
    ///   Iterator to the first element of this @c http_header.
    [[nodiscard]]
    auto begin() noexcept -> iterator {
        return iterator(m_dense.begin());
    }

    /// @brief
    ///   Get iterator to the first element of this @c http_header.
    /// @return
    ///   Iterator to the first element of this @c http_header.
    [[nodiscard]]
    auto begin() const noexcept -> const_iterator {
        return const_iterator(m_dense.begin());
    }

    /// @brief
    ///   Get iterator to the first element of this @c http_header.
    /// @return
    ///   Iterator to the first element of this @c http_header.
    [[nodiscard]]
    auto cbegin() const noexcept -> const_iterator {
        return begin();
    }

    /// @brief
    ///   Get iterator to the element after the last element of this @c http_header.
    /// @return
    ///   Iterator to the element after the last element of this @c http_header.
    [[nodiscard]]
    auto end() noexcept -> iterator {
        return iterator(m_dense.end());
    }

    /// @brief
    ///   Get iterator to the element after the last element of this @c http_header.
    /// @return
    ///   Iterator to the element after the last element of this @c http_header.
    [[nodiscard]]
    auto end() const noexcept -> const_iterator {
        return const_iterator(m_dense.end());
    }

    /// @brief
    ///   Get iterator to the element after the last element of this @c http_header.
    /// @return
    ///   Iterator to the element after the last element of this @c http_header.
    [[nodiscard]]
    auto cend() const noexcept -> const_iterator {
        return end();
    }

    /// @brief
    ///   Get reverse iterator to the first element of the reversed @c http_header.
    /// @return
    ///   Reverse iterator to the first element of the reversed @c http_header.
    [[nodiscard]]
    auto rbegin() noexcept -> reverse_iterator {
        return reverse_iterator(end());
    }

    /// @brief
    ///   Get reverse iterator to the first element of the reversed @c http_header.
    /// @return
    ///   Reverse iterator to the first element of the reversed @c http_header.
    [[nodiscard]]
    auto rbegin() const noexcept -> const_reverse_iterator {
        return const_reverse_iterator(end());
    }

    /// @brief
    ///   Get reverse iterator to the first element of the reversed @c http_header.
    /// @return
    ///   Reverse iterator to the first element of the reversed @c http_header.
    [[nodiscard]]
    auto crbegin() const noexcept -> const_reverse_iterator {
        return rbegin();
    }

    /// @brief
    ///   Get reverse iterator to the element after the last element of the reversed @c http_header.
    /// @return
    ///   Reverse iterator to the element after the last element of the reversed @c http_header.
    [[nodiscard]]
    auto rend() noexcept -> reverse_iterator {
        return reverse_iterator(begin());
    }

    /// @brief
    ///   Get reverse iterator to the element after the last element of the reversed @c http_header.
    /// @return
    ///   Reverse iterator to the element after the last element of the reversed @c http_header.
    [[nodiscard]]
    auto rend() const noexcept -> const_reverse_iterator {
        return const_reverse_iterator(begin());
    }

    /// @brief
    ///   Get reverse iterator to the element after the last element of the reversed @c http_header.
    /// @return
    ///   Reverse iterator to the element after the last element of the reversed @c http_header.
    [[nodiscard]]
    auto crend() const noexcept -> const_reverse_iterator {
        return rend();
    }

    /// @brief
    ///   Checks if this @c http_header is empty.
    /// @retval true
    ///   This @c http_header is empty.
    /// @retval false
    ///   This @c http_header is not empty.
    [[nodiscard]]
    auto empty() const noexcept -> bool {
        return m_dense.empty();
    }

    /// @brief
    ///   Get number of fields in this @c http_header.
    /// @remarks
    ///   This @c http_header implementation is not a multi-map. Duplicate header fields will be
    ///   compressed into one line with separated comma and therefore only be counted once.
    /// @return
    ///   Number of fields in this @c http_header.
    [[nodiscard]]
    auto size() const noexcept -> size_type {
        return m_dense.size();
    }

    /// @brief
    ///   Checks if this HTTP header contains a field that matches the specified key.
    /// @param key
    ///   Key to the header field to be searched for. HTTP header key is case insensitive.
    /// @retval true
    ///   This HTTP header contains a field that matches @p key.
    /// @retval false
    ///   This HTTP header does not have a field that matches @p key.
    [[nodiscard]]
    auto contains(std::string_view key) const noexcept -> bool {
        if (m_dense.empty())
            return false;

        size_type hash = hasher{}(key) % m_sparse.size();

        for (auto i = m_sparse[hash]; i != std::numeric_limits<size_type>::max();
             i      = m_dense[i].next) {
            if (key_equal{}(m_dense[i].key, key))
                return true;
        }

        return false;
    }

    /// @brief
    ///   Get number of elements that matches the specified key.
    /// @param key
    ///   Key to the header field to be searched for. HTTP header key is case insensitive.
    /// @return
    ///   Number of header fields that matches the given key. The return value is always 0 or 1.
    [[nodiscard]]
    auto count(std::string_view key) const noexcept -> size_type {
        return contains(key) ? 1 : 0;
    }

    /// @brief
    ///   Try to find the HTTP header field that matches the specified key.
    /// @param key
    ///   Key of the header field to be searched for. HTTP header key is case insensitive.
    /// @return
    ///   Iterator to the HTTP header field if found. Otherwise, return an iterator to the end of
    ///   this HTTP header.
    [[nodiscard]]
    auto find(std::string_view key) noexcept -> iterator {
        if (m_dense.empty()) [[unlikely]]
            return end();

        size_type hash = hasher{}(key) % m_sparse.size();

        for (auto i = m_sparse[hash]; i != std::numeric_limits<size_type>::max();
             i      = m_dense[i].next) {
            if (key_equal{}(m_dense[i].key, key))
                return iterator(m_dense.begin() + static_cast<difference_type>(i));
        }

        return iterator(m_dense.end());
    }

    /// @brief
    ///   Try to find the HTTP header field that matches the specified key.
    /// @param key
    ///   Key of the header field to be searched for. HTTP header key is case insensitive.
    /// @return
    ///   Iterator to the HTTP header field if found. Otherwise, return an iterator to the end of
    ///   this HTTP header.
    [[nodiscard]]
    auto find(std::string_view key) const noexcept -> const_iterator {
        if (m_dense.empty()) [[unlikely]]
            return end();

        size_type hash = hasher{}(key) % m_sparse.size();

        for (auto i = m_sparse[hash]; i != std::numeric_limits<size_type>::max();
             i      = m_dense[i].next) {
            if (key_equal{}(m_dense[i].key, key))
                return const_iterator(m_dense.begin() + static_cast<difference_type>(i));
        }

        return const_iterator(m_dense.end());
    }

    /// @brief
    ///   Remove all elements in this @c http_header.
    auto clear() noexcept -> void {
        m_sparse.clear();
        m_dense.clear();
    }

    /// @brief
    ///   Insert a new field to this @c http_header if there is no such field with the key in this
    ///   header.
    /// @param value
    ///   The field to be inserted.
    /// @return
    ///   An iterator to the inserted field and a boolean value indicating whether the field is
    ///   inserted.
    NYAIO_API auto insert(const value_type &value) noexcept -> std::pair<iterator, bool>;

    /// @brief
    ///   Insert a new field to this @c http_header if there is no such field with the key in this
    ///   header.
    /// @param value
    ///   The field to be inserted.
    /// @return
    ///   An iterator to the inserted field and a boolean value indicating whether the field is
    ///   inserted.
    NYAIO_API auto insert(value_type &&value) noexcept -> std::pair<iterator, bool>;

    /// @brief
    ///   Insert a new field to this @c http_header if there is no such field with the key in this
    ///   header.
    /// @param key
    ///   Key of the field to be inserted.
    /// @param value
    ///   Value of the field to be inserted.
    /// @return
    ///   An iterator to the inserted field and a boolean value indicating whether the field is
    ///   inserted.
    auto insert(key_type key, mapped_type value) noexcept -> std::pair<iterator, bool> {
        return insert({std::move(key), std::move(value)});
    }

    /// @brief
    ///   Insert a new field to this @c http_header if there is no such field with the key in this
    ///   header.
    /// @param value
    ///   The field to be inserted.
    /// @return
    ///   An iterator to the inserted field and a boolean value indicating whether the field is
    ///   inserted.
    auto emplace(const value_type &value) noexcept -> std::pair<iterator, bool> {
        return insert(value);
    }

    /// @brief
    ///   Insert a new field to this @c http_header if there is no such field with the key in this
    ///   header.
    /// @param value
    ///   The field to be inserted.
    /// @return
    ///   An iterator to the inserted field and a boolean value indicating whether the field is
    ///   inserted.
    auto emplace(value_type &&value) noexcept -> std::pair<iterator, bool> {
        return insert(std::move(value));
    }

    /// @brief
    ///   Insert a new field to this @c http_header if there is no such field with the key in this
    ///   header.
    /// @param key
    ///   Key of the field to be inserted.
    /// @param value
    ///   Value of the field to be inserted.
    /// @return
    ///   An iterator to the inserted field and a boolean value indicating whether the field is
    ///   inserted.
    auto emplace(key_type key, mapped_type value) noexcept -> std::pair<iterator, bool> {
        return insert({std::move(key), std::move(value)});
    }

    /// @brief
    ///   Remove the specified field from this HTTP header.
    /// @param position
    ///   Iterator to the element to be removed.
    /// @return
    ///   Iterator to the element after the removed element.
    NYAIO_API auto erase(const_iterator position) noexcept -> iterator;

    /// @brief
    ///   Remove the specified field from this HTTP header.
    /// @param key
    ///   Key to the element to be removed.
    /// @return
    ///   Actual number of elements removed. The return value is 1 if succeeded to remove the item.
    ///   The return value is 0 if no such element found.
    auto erase(std::string_view key) noexcept -> size_type {
        auto iter = find(key);
        if (iter == end())
            return 0;

        erase(iter);
        return 1;
    }

    /// @brief
    ///   Swap content of this HTTP header with another one.
    /// @param[in, out] other
    ///   The HTTP header to be swapped with.
    auto swap(http_header &other) noexcept -> void {
        m_sparse.swap(other.m_sparse);
        m_dense.swap(other.m_dense);
    }

    /// @brief
    ///   Reserve buckets and rehash elements.
    /// @param count
    ///   New size of hash bucket. Nothing will be done if @p count is not greater than current
    ///   bucket size.
    NYAIO_API auto rehash(size_type count) noexcept -> void;

    /// @brief
    ///   Access the value of the specified key. If the key does not exist, a new field will be
    ///   inserted with an empty value.
    /// @param key
    ///   Key of the field to be accessed.
    /// @return
    ///   Reference to the value of the specified key.
    auto operator[](const key_type &key) noexcept -> mapped_type & {
        return insert({key, std::string{}}).first->second;
    }

private:
    std::vector<size_type> m_sparse;
    std::vector<detail::http_header_node> m_dense;
};

} // namespace nyaio
