#include "nyaio/http.hpp"

using namespace nyaio;
using namespace nyaio::detail;

nyaio::http_header::http_header(const http_header &other) noexcept = default;

nyaio::http_header::~http_header() = default;

auto nyaio::http_header::operator=(const http_header &other) noexcept -> http_header & = default;

auto nyaio::http_header::operator=(http_header &&other) noexcept -> http_header & = default;

auto nyaio::http_header::insert(const value_type &value) noexcept -> std::pair<iterator, bool> {
    // checks if the key already exists
    if (auto iter = find(value.first); iter != end()) [[unlikely]]
        return {iter, false};

    // checks if the dense array is full
    // assumes that integer overflow would never happen
    if (m_dense.size() * 5 >= m_sparse.size() * 4) [[unlikely]]
        rehash(std::max<size_type>(m_sparse.size() * 2 + 3, 7));

    // inserts the value
    size_type hash  = hasher{}(value.first) % m_sparse.size();
    size_type index = m_dense.size();

    m_dense.push_back({
        .next  = m_sparse[hash],
        .key   = value.first,
        .value = value.second,
    });

    m_sparse[hash] = index;

    auto iter = iterator(m_dense.begin() + static_cast<difference_type>(index));
    return std::make_pair(iter, true);
}

auto nyaio::http_header::insert(value_type &&value) noexcept -> std::pair<iterator, bool> {
    // checks if the key already exists
    if (auto iter = find(value.first); iter != end()) [[unlikely]]
        return {iter, false};

    // checks if the dense array is full
    // assumes that integer overflow would never happen
    if (m_dense.size() * 5 >= m_sparse.size() * 4) [[unlikely]]
        rehash(std::max<size_type>(m_sparse.size() * 2 + 3, 7));

    // inserts the value
    size_type hash  = hasher{}(value.first) % m_sparse.size();
    size_type index = m_dense.size();

    m_dense.push_back({
        .next  = m_sparse[hash],
        .key   = std::move(value.first),
        .value = std::move(value.second),
    });

    m_sparse[hash] = index;

    auto iter = iterator(m_dense.begin() + static_cast<difference_type>(index));
    return std::make_pair(iter, true);
}

auto nyaio::http_header::erase(const_iterator position) noexcept -> iterator {
    size_type index = static_cast<size_type>(position.m_current - m_dense.begin());

    { // remove this node from the linked list
        auto &node = m_dense[index];
        auto hash  = hasher{}(node.key) % m_sparse.size();

        size_type node_next = node.next;
        size_type *next     = &m_sparse[hash];
        while (*next != index)
            next = &m_dense[*next].next;
        *next = node_next;
    }

    // swap and pop
    if (position.m_current + 1 != m_dense.end()) {
        size_type last_index = m_dense.size() - 1;
        size_type hash       = hasher{}(m_dense.back().key) % m_sparse.size();

        using std::swap;
        swap(m_dense[last_index], m_dense[index]);

        // update the linked list
        size_type *next = &m_sparse[hash];
        while (*next != last_index)
            next = &m_dense[*next].next;
        *next = index;
    }

    m_dense.pop_back();
    return begin() + static_cast<difference_type>(index);
}

auto nyaio::http_header::rehash(size_type count) noexcept -> void {
    if (count <= m_sparse.size()) [[unlikely]]
        return;

    m_dense.reserve(count);
    m_sparse.resize(count);
    std::fill(m_sparse.begin(), m_sparse.end(), std::numeric_limits<size_type>::max());

    for (size_type i = 0; i < m_dense.size(); ++i) {
        auto &node     = m_dense[i];
        size_type hash = hasher{}(node.key) % count;
        node.next      = m_sparse[hash];
        m_sparse[hash] = i;
    }
}
