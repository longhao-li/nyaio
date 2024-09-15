#pragma once

#include "task.hpp"

#include <bit>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/stat.h>

namespace nyaio {

/// @enum FileFlag
/// @brief
///   Flags for opening file.
enum class FileFlag : std::uint16_t {
    None     = 0,
    Read     = (1 << 0),
    Write    = (1 << 1),
    Append   = (1 << 2),
    Create   = (1 << 3),
    Direct   = (1 << 4),
    Truncate = (1 << 5),
    Sync     = (1 << 6),
};

constexpr auto operator~(FileFlag flag) noexcept -> FileFlag {
    return static_cast<FileFlag>(~static_cast<std::uint16_t>(flag));
}

constexpr auto operator&(FileFlag lhs, FileFlag rhs) noexcept -> FileFlag {
    return static_cast<FileFlag>(static_cast<std::uint16_t>(lhs) & static_cast<std::uint16_t>(rhs));
}

constexpr auto operator|(FileFlag lhs, FileFlag rhs) noexcept -> FileFlag {
    return static_cast<FileFlag>(static_cast<std::uint16_t>(lhs) | static_cast<std::uint16_t>(rhs));
}

constexpr auto operator^(FileFlag lhs, FileFlag rhs) noexcept -> FileFlag {
    return static_cast<FileFlag>(static_cast<std::uint16_t>(lhs) ^ static_cast<std::uint16_t>(rhs));
}

constexpr auto operator&=(FileFlag &lhs, FileFlag rhs) noexcept -> FileFlag & {
    lhs = lhs & rhs;
    return lhs;
}

constexpr auto operator|=(FileFlag &lhs, FileFlag rhs) noexcept -> FileFlag & {
    lhs = lhs | rhs;
    return lhs;
}

constexpr auto operator^=(FileFlag &lhs, FileFlag rhs) noexcept -> FileFlag & {
    lhs = lhs ^ rhs;
    return lhs;
}

/// @enum FileMode
/// @brief
///   File mode for creating file.
enum class FileMode : std::uint16_t {
    None          = 0,
    OwnerRead     = (1 << 8),
    OwnerWrite    = (1 << 7),
    OwnerExecute  = (1 << 6),
    OwnerAll      = OwnerRead | OwnerWrite | OwnerExecute,
    GroupRead     = (1 << 5),
    GroupWrite    = (1 << 4),
    GroupExecute  = (1 << 3),
    GroupAll      = GroupRead | GroupWrite | GroupExecute,
    OthersRead    = (1 << 2),
    OthersWrite   = (1 << 1),
    OthersExecute = (1 << 0),
    OthersAll     = OthersRead | OthersWrite | OthersExecute,
};

constexpr auto operator~(FileMode mode) noexcept -> FileMode {
    return static_cast<FileMode>(~static_cast<std::uint16_t>(mode));
}

constexpr auto operator&(FileMode lhs, FileMode rhs) noexcept -> FileMode {
    return static_cast<FileMode>(static_cast<std::uint16_t>(lhs) & static_cast<std::uint16_t>(rhs));
}

constexpr auto operator|(FileMode lhs, FileMode rhs) noexcept -> FileMode {
    return static_cast<FileMode>(static_cast<std::uint16_t>(lhs) | static_cast<std::uint16_t>(rhs));
}

constexpr auto operator^(FileMode lhs, FileMode rhs) noexcept -> FileMode {
    return static_cast<FileMode>(static_cast<std::uint16_t>(lhs) ^ static_cast<std::uint16_t>(rhs));
}

constexpr auto operator&=(FileMode &lhs, FileMode rhs) noexcept -> FileMode & {
    lhs = lhs & rhs;
    return lhs;
}

constexpr auto operator|=(FileMode &lhs, FileMode rhs) noexcept -> FileMode & {
    lhs = lhs | rhs;
    return lhs;
}

constexpr auto operator^=(FileMode &lhs, FileMode rhs) noexcept -> FileMode & {
    lhs = lhs ^ rhs;
    return lhs;
}

/// @enum SeekBase
/// @brief
///   Base position for seeking file.
enum class SeekBase : std::uint8_t {
    Begin   = SEEK_SET,
    Current = SEEK_CUR,
    End     = SEEK_END,
};

/// @class File
/// @brief
///   Wrapper class for generic regular file.
class File {
public:
    using Self = File;

    /// @brief
    ///   Create an empty file object. Empty file object cannot be used for any file operation.
    File() noexcept : m_file(-1), m_flags(), m_path() {}

    /// @brief
    ///   @c File is not copyable.
    File(const Self &other) = delete;

    /// @brief
    ///   Move constructor of @c File.
    /// @param[in, out] other
    ///   The @c File object to be moved from. The moved object will be empty after this operation.
    File(Self &&other) noexcept
        : m_file(other.m_file), m_flags(other.m_flags), m_path(std::move(other.m_path)) {
        other.m_file  = -1;
        other.m_flags = FileFlag::None;
        other.m_path.clear();
    }

    /// @brief
    ///   Close the file and destroy this object.
    ~File() {
        if (m_file != -1)
            ::close(m_file);
    }

    /// @brief
    ///   @c File is not copyable.
    auto operator=(const Self &other) = delete;

    /// @brief
    ///   Move assignment operator of @c File.
    /// @param[in, out] other
    ///   The @c File object to be moved from. The moved object will be empty after this operation.
    /// @return
    ///   Reference to this @c File object.
    auto operator=(Self &&other) noexcept -> Self & {
        if (this == &other) [[unlikely]]
            return *this;

        if (m_file != -1)
            ::close(m_file);

        m_file  = other.m_file;
        m_flags = other.m_flags;
        m_path  = std::move(other.m_path);

        other.m_file  = -1;
        other.m_flags = FileFlag::None;
        other.m_path.clear();

        return *this;
    }

    /// @brief
    ///   Checks if this file is opened.
    /// @retval true
    ///   This file is opened.
    /// @retval false
    ///   This file is not opened.
    [[nodiscard]]
    auto isOpened() const noexcept -> bool {
        return m_file != -1;
    }

    /// @brief
    ///   Checks if this file is closed.
    /// @retval true
    ///   This file is closed.
    /// @retval false
    ///   This file is not closed.
    [[nodiscard]]
    auto isClosed() const noexcept -> bool {
        return m_file == -1;
    }

    /// @brief
    ///   Checks if reading is allowed on this file.
    /// @retval true
    ///   Reading is allowed on this file.
    /// @retval false
    ///   Reading is not allowed on this file.
    [[nodiscard]]
    auto canRead() const noexcept -> bool {
        return (m_flags & FileFlag::Read) != FileFlag::None;
    }

    /// @brief
    ///   Checks if writing is allowed on this file.
    /// @retval true
    ///   Writing is allowed on this file.
    /// @retval false
    ///   Writing is not allowed on this file.
    [[nodiscard]]
    auto canWrite() const noexcept -> bool {
        return (m_flags & FileFlag::Write) != FileFlag::None;
    }

    /// @brief
    ///   Get open flags of this file. It is undefined behavior to get flags from empty file object.
    /// @return
    ///   Open flags of this file.
    [[nodiscard]]
    auto flags() const noexcept -> FileFlag {
        return m_flags;
    }

    /// @brief
    ///   Get path of this file. It is undefined behavior to get path from empty file object.
    /// @return
    ///   Path of this file.
    [[nodiscard]]
    auto path() const noexcept -> std::string_view {
        return m_path;
    }

    /// @brief
    ///   Get size of this file. It is undefined behavior to get size from empty file object.
    /// @return
    ///   Size in byte of this file.
    [[nodiscard]]
    auto size() const noexcept -> std::size_t {
        struct stat st;
        if (::fstat(m_file, &st) == -1) [[unlikely]]
            return 0;
        return st.st_size;
    }

    /// @brief
    ///   Try to open a file with specified flags. File mode will be set to 0644 by default if
    ///   @c FileFlag::Create is specified and the file does not exist.
    /// @param path
    ///   Path to the file to be opened.
    /// @param flags
    ///   Flags for opening the file.
    /// @return
    ///   An error code that indicates the result of the open operation. The error code is
    ///   @c std::errc{} if succeeded to open the file.
    auto open(std::string_view path, FileFlag flags) noexcept -> std::errc {
        return open(path, flags,
                    FileMode::OwnerRead | FileMode::OwnerWrite | FileMode::GroupRead |
                        FileMode::OthersRead);
    }

    /// @brief
    ///   Try to open a file with specified flags.
    /// @param path
    ///   Path to the file to be opened.
    /// @param flags
    ///   Flags for opening the file.
    /// @param mode
    ///   Mode of the new file if a new file is created. This parameter will be ignored if
    ///   @c FileFlag::Create is not specified.
    /// @return
    ///   An error code that indicates the result of the open operation. The error code is
    ///   @c std::errc{} if succeeded to open the file.
    NYAIO_API auto open(std::string_view path, FileFlag flags, FileMode mode) noexcept -> std::errc;

    /// @brief
    ///   Close the file. It is safe to close an empty file object.
    auto close() noexcept -> void {
        if (m_file != -1) {
            ::close(m_file);
            m_file  = -1;
            m_flags = FileFlag::None;
            m_path.clear();
        }
    }

    /// @brief
    ///   Read some data from this file.
    /// @param[out] buffer
    ///   Pointer to the buffer to store the read data.
    /// @param size
    ///   Size in byte of the buffer.
    /// @return
    ///   A struct that contains number of bytes read and an error code. The error code is
    ///   @c std::errc{} if succeeded and the number of bytes read is valid.
    auto read(void *buffer, std::uint32_t size) noexcept -> SystemIoResult {
        int result = ::read(m_file, buffer, size);
        if (result < 0) [[unlikely]]
            return {0, std::errc{errno}};
        return {static_cast<std::uint32_t>(result), std::errc{}};
    }

    /// @brief
    ///   Read some data from this file at specified offset.
    /// @param[out] buffer
    ///   Pointer to the buffer to store the read data.
    /// @param size
    ///   Size in byte of the buffer.
    /// @param offset
    ///   Offset in byte from the beginning of the file to start reading.
    /// @return
    ///   A struct that contains number of bytes read and an error code. The error code is
    ///   @c std::errc{} if succeeded and the number of bytes read is valid. The returned number of
    ///   bytes is 0 if end of file is reached.
    auto read(void *buffer, std::uint32_t size, std::size_t offset) noexcept -> SystemIoResult {
        int result = ::pread(m_file, buffer, size, offset);
        if (result < 0) [[unlikely]]
            return {0, std::errc{errno}};
        return {static_cast<std::uint32_t>(result), std::errc{}};
    }

    /// @brief
    ///   Async read some data from this file. This method will suspend current coroutine until the
    ///   reading operation is completed or any error occurs.
    /// @param[out] buffer
    ///   Pointer to the buffer to store the read data.
    /// @param size
    ///   Size in byte of the buffer.
    /// @return
    ///   A struct that contains number of bytes read and an error code. The error code is
    ///   @c std::errc{} if succeeded and the number of bytes read is valid. The returned number of
    ///   bytes is 0 if end of file is reached.
    [[nodiscard]]
    auto readAsync(void *buffer, std::uint32_t size) noexcept -> ReadAwaitable {
        return {m_file, buffer, size, std::uint64_t(-1)};
    }

    /// @brief
    ///   Async read some data from this file at specified offset. This method will suspend current
    ///   coroutine until the reading operation is completed or any error occurs.
    /// @param[out] buffer
    ///   Pointer to the buffer to store the read data.
    /// @param size
    ///   Size in byte of the buffer.
    /// @param offset
    ///   Offset in byte from the beginning of the file to start reading.
    /// @return
    ///   A struct that contains number of bytes read and an error code. The error code is
    ///   @c std::errc{} if succeeded and the number of bytes read is valid. The returned number of
    ///   bytes is 0 if end of file is reached.
    [[nodiscard]]
    auto readAsync(void *buffer, std::uint32_t size, std::size_t offset) noexcept -> ReadAwaitable {
        return {m_file, buffer, size, offset};
    }

    /// @brief
    ///   Write some data to this file.
    /// @param data
    ///   Pointer to start of the data to be written.
    /// @param size
    ///   Expected size in byte of data to be written.
    /// @return
    ///   A struct that contains number of bytes written and an error code. The error code is
    ///   @c std::errc{} if succeeded and the number of bytes written is valid.
    auto write(const void *data, std::uint32_t size) noexcept -> SystemIoResult {
        int result = ::write(m_file, data, size);
        if (result < 0) [[unlikely]]
            return {0, std::errc{errno}};
        return {static_cast<std::uint32_t>(result), std::errc{}};
    }

    /// @brief
    ///   Write some data to this file at specified offset.
    /// @param data
    ///   Pointer to start of the data to be written.
    /// @param size
    ///   Expected size in byte of data to be written.
    /// @param offset
    ///   Offset in byte from the beginning of the file to start writing.
    /// @return
    ///   A struct that contains number of bytes written and an error code. The error code is
    ///   @c std::errc{} if succeeded and the number of bytes written is valid.
    auto write(const void *data, std::uint32_t size, std::size_t offset) noexcept
        -> SystemIoResult {
        int result = ::pwrite(m_file, data, size, offset);
        if (result < 0) [[unlikely]]
            return {0, std::errc{errno}};
        return {static_cast<std::uint32_t>(result), std::errc{}};
    }

    /// @brief
    ///   Async write some data to this file. This method will suspend current coroutine until the
    ///   writing operation is completed or any error occurs.
    /// @param data
    ///   Pointer to start of the data to be written.
    /// @param size
    ///   Expected size in byte of data to be written.
    /// @return
    ///   A struct that contains number of bytes written and an error code. The error code is
    ///   @c std::errc{} if succeeded and the number of bytes written is valid.
    [[nodiscard]]
    auto writeAsync(const void *data, std::uint32_t size) noexcept -> WriteAwaitable {
        return {m_file, data, size, std::uint64_t(-1)};
    }

    /// @brief
    ///   Async write some data to this file at specified offset. This method will suspend current
    ///   coroutine until the writing operation is completed or any error occurs.
    /// @param data
    ///   Pointer to start of the data to be written.
    /// @param size
    ///   Expected size in byte of data to be written.
    /// @param offset
    ///   Offset in byte from the beginning of the file to start writing.
    /// @return
    ///   A struct that contains number of bytes written and an error code. The error code is
    ///   @c std::errc{} if succeeded and the number of bytes written is valid.
    [[nodiscard]]
    auto writeAsync(const void *data, std::uint32_t size, std::size_t offset) noexcept
        -> WriteAwaitable {
        return {m_file, data, size, offset};
    }

    /// @brief
    ///   Flush the file to disk. This method will block current thread until the file is flushed.
    /// @return
    ///   An error code that indicates the result of the flush operation. The error code is
    ///   @c std::errc{} if succeeded to flush the file.
    auto flush() noexcept -> std::errc {
        if (::fsync(m_file) == -1) [[unlikely]]
            return std::errc{errno};
        return std::errc{};
    }

    /// @brief
    ///   Async flush the file to disk. This method will suspend current coroutine until the flush
    ///   operation is completed.
    /// @return
    ///   An error code that indicates the result of the flush operation. The error code is
    ///   @c std::errc{} if succeeded to flush the file.
    [[nodiscard]]
    auto flushAsync() noexcept -> FileSyncAwaitable {
        return {m_file};
    }

    /// @brief
    ///   Truncate the file to specified size. This method will block current thread until the file
    ///   truncate operation is completed. If currently this file is larger than the specified size,
    ///   the extra data will be discarded. If currently this file is smaller than the specified
    ///   size, the file will be extended and the extended part will be filled with zero.
    /// @param size
    ///   New size in byte of the file.
    /// @return
    ///   An error code that indicates the result of the truncate operation. The error code is
    ///   @c std::errc{} if succeeded to truncate the file.
    auto truncate(std::size_t size) noexcept -> std::errc {
        if (::ftruncate(m_file, size) == -1) [[unlikely]]
            return std::errc{errno};
        return std::errc{};
    }

    /// @brief
    ///   Seek the file pointer to specified position.
    /// @param base
    ///   Base position for seeking.
    /// @param offset
    ///   Offset in byte from the base position to seek.
    /// @return
    ///   An error code that indicates the result of the seek operation. The error code is
    ///   @c std::errc{} if succeeded to seek the file.
    auto seek(SeekBase base, std::ptrdiff_t offset) noexcept -> std::errc {
        if (::lseek(m_file, offset, static_cast<int>(base)) == -1) [[unlikely]]
            return std::errc{errno};
        return std::errc{};
    }

private:
    int m_file;
    FileFlag m_flags;
    std::string m_path;
};

/// @brief
///   Convert a multi-byte integer from host endian to network endian.
/// @tparam T
///   Type of the integer to be converted.
/// @param value
///   The integer to be converted.
/// @return
///   The value in network endian byte order.
template <class T>
    requires(std::is_integral_v<T> && !std::is_same_v<T, bool> && sizeof(T) <= 8)
constexpr auto toNetworkEndian(T value) noexcept -> T {
    if constexpr (std::endian::native == std::endian::little) {
        if constexpr (sizeof(T) == 1)
            return value;
        else if constexpr (sizeof(T) == 2)
            return static_cast<T>(__builtin_bswap16(static_cast<std::uint16_t>(value)));
        else if constexpr (sizeof(T) == 4)
            return static_cast<T>(__builtin_bswap32(static_cast<std::uint32_t>(value)));
        else if constexpr (sizeof(T) == 8)
            return static_cast<T>(__builtin_bswap64(static_cast<std::uint64_t>(value)));
    } else {
        return value;
    }
}

/// @brief
///   Convert a multi-byte integer from network endian to host endian.
/// @tparam T
///   Type of the integer to be converted.
/// @param value
///   The integer to be converted.
/// @return
///   The value in host endian byte order.
template <class T>
    requires(std::is_integral_v<T> && !std::is_same_v<T, bool> && sizeof(T) <= 8)
constexpr auto toHostEndian(T value) noexcept -> T {
    if constexpr (std::endian::native == std::endian::little) {
        if constexpr (sizeof(T) == 1)
            return value;
        else if constexpr (sizeof(T) == 2)
            return static_cast<T>(__builtin_bswap16(static_cast<std::uint16_t>(value)));
        else if constexpr (sizeof(T) == 4)
            return static_cast<T>(__builtin_bswap32(static_cast<std::uint32_t>(value)));
        else if constexpr (sizeof(T) == 8)
            return static_cast<T>(__builtin_bswap64(static_cast<std::uint64_t>(value)));
    } else {
        return value;
    }
}

/// @class IpAddress
/// @brief
///   Wrapper class for IP address. Both IPv4 and IPv6 are supported.
class IpAddress {
public:
    using Self = IpAddress;

    /// @brief
    ///   Create an empty IP address.
    constexpr IpAddress() noexcept : m_isV6(), m_addr() {}

    /// @brief
    ///   Create a new IP address from string. IPv4 and IPv6 are determined by the address format.
    /// @param addr
    ///   String representation of the IP address. Both IPv4 and IPv6 are supported.
    /// @throws std::invalid_argument
    ///   Thrown if @p addr is not a valid IP address.
    NYAIO_API explicit IpAddress(std::string_view addr);

    /// @brief
    ///   Create an IPv4 address from integers.
    /// @param v0
    ///   First value of the IPv4 address.
    /// @param v1
    ///   Second value of the IPv4 address.
    /// @param v2
    ///   Third value of the IPv4 address.
    /// @param v3
    ///   Fourth value of the IPv4 address.
    constexpr IpAddress(std::uint8_t v0, std::uint8_t v1, std::uint8_t v2, std::uint8_t v3) noexcept
        : m_isV6(), m_addr() {
        m_addr.v4.s_addr = toNetworkEndian(
            static_cast<std::uint32_t>(v0) << 24 | static_cast<std::uint32_t>(v1) << 16 |
            static_cast<std::uint32_t>(v2) << 8 | static_cast<std::uint32_t>(v3));
    }

    /// @brief
    ///   Create an IPv6 address from integers.
    /// @param v0
    ///   First value of the IPv6 address in host endian.
    /// @param v1
    ///   Second value of the IPv6 address in host endian.
    /// @param v2
    ///   Third value of the IPv6 address in host endian.
    /// @param v3
    ///   Fourth value of the IPv6 address in host endian.
    /// @param v4
    ///   Fifth value of the IPv6 address in host endian.
    /// @param v5
    ///   Sixth value of the IPv6 address in host endian.
    /// @param v6
    ///   Seventh value of the IPv6 address in host endian.
    /// @param v7
    ///   Eighth value of the IPv6 address in host endian.
    constexpr IpAddress(std::uint16_t v0, std::uint16_t v1, std::uint16_t v2, std::uint16_t v3,
                        std::uint16_t v4, std::uint16_t v5, std::uint16_t v6,
                        std::uint16_t v7) noexcept
        : m_isV6(true), m_addr() {
        m_addr.v6.s6_addr16[0] = toNetworkEndian(v0);
        m_addr.v6.s6_addr16[1] = toNetworkEndian(v1);
        m_addr.v6.s6_addr16[2] = toNetworkEndian(v2);
        m_addr.v6.s6_addr16[3] = toNetworkEndian(v3);
        m_addr.v6.s6_addr16[4] = toNetworkEndian(v4);
        m_addr.v6.s6_addr16[5] = toNetworkEndian(v5);
        m_addr.v6.s6_addr16[6] = toNetworkEndian(v6);
        m_addr.v6.s6_addr16[7] = toNetworkEndian(v7);
    }

    /// @brief
    ///   @c IpAddress is trivially copyable.
    /// @param other
    ///   The @c IpAddress to be copied from.
    constexpr IpAddress(const Self &other) noexcept = default;

    /// @brief
    ///   @c IpAddress is trivially movable.
    /// @param other
    ///   The @c IpAddress to be moved from.
    constexpr IpAddress(Self &&other) noexcept = default;

    /// @brief
    ///   @c IpAddress is trivially destructible.
    constexpr ~IpAddress() = default;

    /// @brief
    ///   @c IpAddress is trivially copyable.
    /// @param other
    ///   The @c IpAddress to be copied from.
    /// @return
    ///   Reference to this @c ip_address.
    constexpr auto operator=(const Self &other) noexcept -> Self & = default;

    /// @brief
    ///   @c IpAddress is trivially movable.
    /// @param other
    ///   The @c IpAddress to be moved from.
    /// @return
    ///   Reference to this @c IpAddress.
    constexpr auto operator=(Self &&other) noexcept -> Self & = default;

    /// @brief
    ///   Checks if this is an IPv4 address.
    /// @retval true
    ///   This is an IPv4 address.
    /// @retval false
    ///   This is not an IPv4 address.
    [[nodiscard]]
    constexpr auto isIpv4() const noexcept -> bool {
        return !m_isV6;
    }

    /// @brief
    ///   Checks if this is an IPv6 address.
    /// @retval true
    ///   This is an IPv6 address.
    /// @retval false
    ///   This is not an IPv6 address.
    [[nodiscard]]
    constexpr auto isIpv6() const noexcept -> bool {
        return m_isV6;
    }

    /// @brief
    ///   Get pointer to the raw address data. Length of the address data is determined by address
    ///   type. For IPv4, the length is 4 bytes. For IPv6, the length is 16 bytes.
    /// @note
    ///   Address is stored in network byte order.
    /// @return
    ///   Pointer to the raw IP address data.
    [[nodiscard]]
    auto address() noexcept -> void * {
        return &m_addr;
    }

    /// @brief
    ///   Get pointer to the raw address data. Length of the address data is determined by address
    ///   type. For IPv4, the length is 4 bytes. For IPv6, the length is 16 bytes.
    /// @note
    ///   Address is stored in network byte order.
    /// @return
    ///   Pointer to the raw IP address data.
    [[nodiscard]]
    auto address() const noexcept -> const void * {
        return &m_addr;
    }

    /// @brief
    ///   Checks if this address is an IPv4 loopback address. IPv4 loopback address is @c 127.0.0.1.
    /// @retval true
    ///   This address is an IPv4 loopback address.
    /// @retval false
    ///   This address is not an IPv4 loopback address.
    [[nodiscard]]
    constexpr auto isIpv4Loopback() const noexcept -> bool {
        return isIpv4() && (m_addr.v4.s_addr == toNetworkEndian(INADDR_LOOPBACK));
    }

    /// @brief
    ///   Checks if this address is an IPv4 any address. IPv4 any address is @c 0.0.0.0.
    /// @retval true
    ///   This address is an IPv4 any address.
    /// @retval false
    ///   This address is not an IPv4 any address.
    [[nodiscard]]
    constexpr auto isIpv4Any() const noexcept -> bool {
        return isIpv4() && (m_addr.v4.s_addr == toNetworkEndian(INADDR_ANY));
    }

    /// @brief
    ///   Checks if this address is an IPv4 broadcast address. IPv4 broadcast address is @c
    ///   255.255.255.255.
    /// @retval true
    ///   This address is an IPv4 broadcast address.
    /// @retval false
    ///   This address is not an IPv4 broadcast address.
    [[nodiscard]]
    constexpr auto isIpv4Broadcast() const noexcept -> bool {
        return isIpv4() && (m_addr.v4.s_addr == toNetworkEndian(INADDR_BROADCAST));
    }

    /// @brief
    ///   Checks if this address is an IPv4 private address. An IPv4 private network is a network
    ///   that used for local area networks. Private address ranges are defined in RFC 1918 as
    ///   follows:
    ///   - @c 10.0.0.0/8
    ///   - @c 172.16.0.0/12
    ///   - @c 192.168.0.0/16
    /// @retval true
    ///   This address is an IPv4 private address.
    /// @retval false
    ///   This address is not an IPv4 private address.
    [[nodiscard]]
    constexpr auto isIpv4Private() const noexcept -> bool {
        if (!isIpv4())
            return false;

        // 10.0.0.0/8
        if ((toHostEndian(m_addr.v4.s_addr) >> 24) == 0x0A)
            return true;

        // 172.16.0.0/12
        if ((toHostEndian(m_addr.v4.s_addr) >> 20) == 0xAC1)
            return true;

        // 192.168.0.0/16
        if ((toHostEndian(m_addr.v4.s_addr) >> 16) == 0xC0A8)
            return true;

        return false;
    }

    /// @brief
    ///   Checks if this address is an IPv4 link local address. IPv4 link local address is @c
    ///   169.254.0.0/16 as defined in RFC 3927.
    /// @retval true
    ///   This address is an IPv4 link local address.
    /// @retval false
    ///   This address is not an IPv4 link local address.
    [[nodiscard]]
    constexpr auto isIpv4Linklocal() const noexcept -> bool {
        if (!isIpv4())
            return false;

        if ((toHostEndian(m_addr.v4.s_addr) >> 16) == 0xA9FE)
            return true;

        return false;
    }

    /// @brief
    ///   Checks if this address is an IPv4 multicast address. IPv4 multicast address is @c
    ///   224.0.0.0/4 as defined in RFC 5771.
    /// @retval true
    ///   This address is an IPv4 multicast address.
    /// @retval false
    ///   This address is not an IPv4 multicast address.
    [[nodiscard]]
    constexpr auto isIpv4Multicast() const noexcept -> bool {
        if (!isIpv4())
            return false;

        if ((toHostEndian(m_addr.v4.s_addr) >> 28) == 0xE)
            return true;

        return false;
    }

    /// @brief
    ///   Checks if this address is an IPv6 loopback address. IPv6 loopback address is @c ::1.
    /// @retval true
    ///   This address is an IPv6 loopback address.
    /// @retval false
    ///   This address is not an IPv6 loopback address.
    [[nodiscard]]
    constexpr auto isIpv6Loopback() const noexcept -> bool {
        if (!isIpv6())
            return false;

        return ((m_addr.v6.s6_addr16[0] == toNetworkEndian<std::uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[1] == toNetworkEndian<std::uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[2] == toNetworkEndian<std::uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[3] == toNetworkEndian<std::uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[4] == toNetworkEndian<std::uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[5] == toNetworkEndian<std::uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[6] == toNetworkEndian<std::uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[7] == toNetworkEndian<std::uint16_t>(1)));
    }

    /// @brief
    ///   Checks if this address is an IPv6 any address. IPv6 any address is @c ::.
    /// @retval true
    ///   This address is an IPv6 any address.
    /// @retval false
    ///   This address is not an IPv6 any address.
    [[nodiscard]]
    constexpr auto isIpv6Any() const noexcept -> bool {
        if (!isIpv6())
            return false;

        return ((m_addr.v6.s6_addr16[0] == toNetworkEndian<std::uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[1] == toNetworkEndian<std::uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[2] == toNetworkEndian<std::uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[3] == toNetworkEndian<std::uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[4] == toNetworkEndian<std::uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[5] == toNetworkEndian<std::uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[6] == toNetworkEndian<std::uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[7] == toNetworkEndian<std::uint16_t>(0)));
    }

    /// @brief
    ///   Checks if this address is an IPv6 multicast address. IPv6 multicast address is @c FF00::/8
    ///   as defined in RFC 4291.
    /// @retval true
    ///   This address is an IPv6 multicast address.
    /// @retval false
    ///   This address is not an IPv6 multicast address.
    [[nodiscard]]
    constexpr auto isIpv6Multicast() const noexcept -> bool {
        if (!isIpv6())
            return false;
        return (m_addr.v6.s6_addr[0] == 0xFF);
    }

    /// @brief
    ///   Checks if this address is an IPv4 mapped IPv6 address. IPv4 mapped IPv6 address is @c
    ///   ::FFFF:0:0/96.
    /// @retval true
    ///   This is an IPv4 mapped IPv6 address.
    /// @retval false
    ///   This is not an IPv4 mapped IPv6 address.
    [[nodiscard]]
    constexpr auto isIpv4MappedIpv6() const noexcept -> bool {
        if (!isIpv6())
            return false;

        return ((m_addr.v6.s6_addr16[0] == toNetworkEndian<std::uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[1] == toNetworkEndian<std::uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[2] == toNetworkEndian<std::uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[3] == toNetworkEndian<std::uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[4] == toNetworkEndian<std::uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[5] == toNetworkEndian<std::uint16_t>(0xFFFF)));
    }

    /// @brief
    ///   Converts this IP address to IPv4 address.
    /// @return
    ///   Return this address if this is an IPv4 or IPv4-mapped IPv6 address. It is undefined
    ///   behavior if this is either not an IPv4 address or an IPv4-mapped IPv6 address.
    [[nodiscard]]
    constexpr auto toIpv4() const noexcept -> IpAddress {
        if (isIpv4())
            return *this;
        return {m_addr.v6.s6_addr[12], m_addr.v6.s6_addr[13], m_addr.v6.s6_addr[14],
                m_addr.v6.s6_addr[15]};
    }

    /// @brief
    ///   Converts this IP address to IPv6 address.
    /// @return
    ///   Return an IPv4-mapped IPv6 address if this is an IPv4 address. Otherwise, return this IPv6
    ///   address itself.
    [[nodiscard]]
    constexpr auto toIpv6() const noexcept -> IpAddress {
        if (isIpv6())
            return *this;

        const std::uint32_t v4 = toHostEndian(m_addr.v4.s_addr);
        const auto high        = static_cast<std::uint16_t>(v4 >> 16);
        const auto low         = static_cast<std::uint16_t>(v4 & 0xFFFF);

        return {0, 0, 0, 0, 0, 0xFFFF, high, low};
    }

    /// @brief
    ///   Checks if this @c IpAddress is the same as another one.
    /// @param other
    ///   The @c IpAddress to be compared with.
    /// @retval true
    ///   This @c IpAddress is the same as @p other.
    /// @retval false
    ///   This @c IpAddress is not the same as @p other.
    constexpr auto operator==(const Self &other) const noexcept -> bool {
        if (m_isV6 != other.m_isV6)
            return false;

        if (isIpv4())
            return !__builtin_memcmp(&m_addr.v4, &other.m_addr.v4, sizeof(m_addr.v4));
        return !__builtin_memcmp(&m_addr.v6, &other.m_addr.v6, sizeof(m_addr.v6));
    }

    friend class InetAddress;

private:
    bool m_isV6;
    union {
        struct in_addr v4;
        struct in6_addr v6;
    } m_addr;
};

/// @brief
///   IPv4 loopback address.
inline constexpr IpAddress Ipv4Loopback{127, 0, 0, 1};

/// @brief
///   IPv4 address that listens to any incoming address.
inline constexpr IpAddress Ipv4Any(0, 0, 0, 0);

/// @brief
///   IPv4 broadcast address.
inline constexpr IpAddress Ipv4Broadcast(255, 255, 255, 255);

/// @brief
///   IPv6 loopback address.
inline constexpr IpAddress Ipv6Loopback(0, 0, 0, 0, 0, 0, 0, 1);

/// @brief
///   IPv6 address that listens to any incoming address.
inline constexpr IpAddress Ipv6Any(0, 0, 0, 0, 0, 0, 0, 0);

/// @class InetAddress
/// @brief
///   Wrapper class for Internet socket address. This class could be directly used as @c sockaddr_in
///   and @c sockaddr_in6.
class InetAddress {
public:
    using Self = InetAddress;

    /// @brief
    ///   Create an empty Internet address. Empty Internet address cannot be used for any network
    ///   operation.
    constexpr InetAddress() noexcept : m_addr() {}

    /// @brief
    ///   Create a new Internet address from an IP address and port.
    /// @param ip
    ///   IP address of this Internet address.
    /// @param port
    ///   Port number in host endian.
    constexpr InetAddress(const IpAddress &ip, std::uint16_t port) noexcept : m_addr() {
        if (ip.isIpv4()) {
            m_addr.v4.sin_family = AF_INET;
            m_addr.v4.sin_port   = toNetworkEndian(port);
            m_addr.v4.sin_addr   = ip.m_addr.v4;
        } else {
            m_addr.v6.sin6_family   = AF_INET6;
            m_addr.v6.sin6_port     = toNetworkEndian(port);
            m_addr.v6.sin6_flowinfo = 0;
            m_addr.v6.sin6_addr     = ip.m_addr.v6;
            m_addr.v6.sin6_scope_id = 0;
        }
    }

    /// @brief
    ///   @c InetAddress is trivially copyable.
    /// @param other
    ///   The Internet address to be copied from.
    constexpr InetAddress(const Self &other) noexcept = default;

    /// @brief
    ///   @c InetAddress is trivially movable.
    /// @param other
    ///   The Internet address to be moved from.
    constexpr InetAddress(Self &&other) noexcept = default;

    /// @brief
    ///   @c InetAddress is trivially destructible.
    constexpr ~InetAddress() = default;

    /// @brief
    ///   @c InetAddress is trivially copyable.
    /// @param other
    ///   The Internet address to be copied from.
    /// @return
    ///   Reference to this Internet address.
    constexpr auto operator=(const Self &other) noexcept -> Self & = default;

    /// @brief
    ///   @c InetAddress is trivially movable.
    /// @param other
    ///   The Internet address to be moved from.
    /// @return
    ///   Reference to this Internet address.
    constexpr auto operator=(Self &&other) noexcept -> Self & = default;

    /// @brief
    ///   Checks if this is an IPv4 Internet address.
    /// @retval true
    ///   This is an IPv4 Internet address.
    /// @retval false
    ///   This is not an IPv4 Internet address.
    [[nodiscard]]
    constexpr auto isIpv4() const noexcept -> bool {
        return m_addr.v4.sin_family == AF_INET;
    }

    /// @brief
    ///   Checks if this is an IPv6 Internet address.
    /// @retval true
    ///   This is an IPv6 Internet address.
    /// @retval false
    ///   This is not an IPv6 Internet address.
    [[nodiscard]]
    constexpr auto isIpv6() const noexcept -> bool {
        return m_addr.v6.sin6_family == AF_INET6;
    }

    /// @brief
    ///   Get actual size in byte of this Internet address object.
    /// @return
    ///   Actual size in byte of this Internet address object. It is undefined behavior to get size
    ///   for empty Internet address object.
    [[nodiscard]]
    constexpr auto size() const noexcept -> socklen_t {
        return isIpv4() ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);
    }

    /// @brief
    ///   Get IP address of this Internet address. It is undefined behavior to get IP address from
    ///   empty Internet address.
    /// @return
    ///   IP address of this Internet address.
    [[nodiscard]]
    constexpr auto ip() const noexcept -> IpAddress {
        if (isIpv4()) {
            IpAddress addr;
            addr.m_isV6    = false;
            addr.m_addr.v4 = m_addr.v4.sin_addr;
            return addr;
        } else {
            IpAddress addr;
            addr.m_isV6    = true;
            addr.m_addr.v6 = m_addr.v6.sin6_addr;
            return addr;
        }
    }

    /// @brief
    ///   Get port of this Internet address. It is undefined behavior to get port number from empty
    ///   Internet address.
    /// @return
    ///   Port number in host endian.
    [[nodiscard]]
    constexpr auto port() const noexcept -> std::uint16_t {
        return toHostEndian(m_addr.v4.sin_port);
    }

    /// @brief
    ///   Set port of this Internet address.
    /// @param port
    ///   The port in host endian to be set.
    constexpr auto setPort(std::uint16_t port) noexcept -> void {
        m_addr.v4.sin_port = toNetworkEndian(port);
    }

    /// @brief
    ///   Get IPv6 flow label.
    /// @return
    ///   IPv6 flow label. The return value is undefined if this is not an IPv6 address.
    [[nodiscard]]
    constexpr auto flowLabel() const noexcept -> std::uint32_t {
        return toHostEndian(m_addr.v6.sin6_flowinfo);
    }

    /// @brief
    ///   Set flow label for this IPv6 Internet address. It is undefined behavior to set flow label
    ///   for IPv4 address.
    /// @param label
    ///   The flow label in host endian to be set.
    constexpr auto setFlowLabel(std::uint32_t label) noexcept -> void {
        m_addr.v6.sin6_flowinfo = toNetworkEndian(label);
    }

    /// @brief
    ///   Get IPv6 scope ID.
    /// @return
    ///   IPv6 scope ID in host endian. It is undefined behavior to get scope ID from IPv4 Internet
    ///   address.
    [[nodiscard]]
    constexpr auto scopeId() const noexcept -> std::uint32_t {
        return toHostEndian(m_addr.v6.sin6_scope_id);
    }

    /// @brief
    ///   Set scope ID for this IPv6 Internet address. It is undefined behavior to set scope ID for
    ///   IPv4 Internet address.
    /// @param id
    ///   The scope ID in host endian to be set.
    constexpr auto setScopeId(std::uint32_t id) noexcept -> void {
        m_addr.v6.sin6_scope_id = toNetworkEndian(id);
    }

    /// @brief
    ///   Checks if this @c InetAddress is the same as another one.
    /// @param other
    ///   The @c InetAddress to be compared with.
    /// @retval true
    ///   This @c InetAddress is the same as @p other.
    /// @retval false
    ///   This @c InetAddress is not the same as @p other.
    constexpr auto operator==(const Self &other) const noexcept -> bool {
        if (isIpv4())
            return !__builtin_memcmp(&m_addr.v4, &other.m_addr.v4, sizeof(m_addr.v4));
        return !__builtin_memcmp(&m_addr.v6, &other.m_addr.v6, sizeof(m_addr.v6));
    }

private:
    union {
        struct sockaddr_in v4;
        struct sockaddr_in6 v6;
    } m_addr;
};

/// @class TcpStream
/// @brief
///   Wrapper class for TCP connection. This class is used for TCP socket IO.
class TcpStream {
public:
    using Self = TcpStream;

    /// @class ConnectAwaiter
    /// @brief
    ///   Customized connect awaitable for @c TcpStream. Using awaitable to avoid memory allocation
    ///   by @c Task.
    class ConnectAwaiter {
    public:
        using Self = ConnectAwaiter;

        /// @brief
        ///   Create a new @c ConnectAwaiter for establishing a TCP connection.
        /// @param[in, out] stream
        ///   The @c TcpStream to establish connection.
        /// @param address
        ///   The peer address to connect.
        ConnectAwaiter(TcpStream &stream, const InetAddress &address) noexcept
            : m_promise(nullptr), m_socket(-1), m_address(&address), m_stream(&stream) {}

        /// @brief
        ///   C++20 coroutine API method. Always execute @c await_suspend().
        /// @return
        ///   This function always returns @c false.
        [[nodiscard]]
        static constexpr auto await_ready() noexcept -> bool {
            return false;
        }

        /// @brief
        ///   Prepare for async connect operation and suspend the coroutine.
        /// @tparam T
        ///   Type of promise of current coroutine.
        /// @param coro
        ///   Current coroutine handle.
        template <class T>
            requires(std::is_base_of_v<detail::PromiseBase, T>)
        auto await_suspend(std::coroutine_handle<T> coro) noexcept -> bool {
            auto &promise = static_cast<detail::PromiseBase &>(coro.promise());
            m_promise     = &promise;

            // Create a new socket to establish connection.
            auto *addr = reinterpret_cast<const struct sockaddr *>(m_address);
            m_socket   = ::socket(addr->sa_family, SOCK_STREAM | SOCK_CLOEXEC, IPPROTO_TCP);
            if (m_socket == -1) [[unlikely]] {
                promise.ioResult = -errno;
                return false;
            }

            auto *worker      = promise.worker;
            io_uring_sqe *sqe = worker->pollSubmissionQueueEntry();
            while (sqe == nullptr) [[unlikely]] {
                worker->submit();
                sqe = worker->pollSubmissionQueueEntry();
            }

            sqe->opcode    = IORING_OP_CONNECT;
            sqe->fd        = m_socket;
            sqe->addr      = reinterpret_cast<std::uintptr_t>(m_address);
            sqe->off       = m_address->size();
            sqe->user_data = reinterpret_cast<std::uintptr_t>(&promise);

            worker->flushSubmissionQueue();
            return true;
        }

        /// @brief
        ///   Resume this coroutine and get result of the async connect operation.
        /// @return
        ///   Error code of the connect operation. The error code is 0 if succeeded.
        auto await_resume() const noexcept -> std::errc {
            if (m_promise->ioResult < 0) [[unlikely]] {
                if (m_socket != -1)
                    ::close(m_socket);
                return static_cast<std::errc>(-m_promise->ioResult);
            }

            if (m_stream->m_socket != -1)
                ::close(m_stream->m_socket);

            m_stream->m_socket  = m_socket;
            m_stream->m_address = *m_address;

            return {};
        }

    private:
        detail::PromiseBase *m_promise;
        int m_socket;
        const InetAddress *m_address;
        TcpStream *m_stream;
    };

public:
    /// @brief
    ///   Create an empty @c TcpStream. Empty @c TcpStream object cannot be used for IO operations.
    TcpStream() noexcept : m_socket(-1), m_address() {}

    /// @brief
    ///   For internal usage. Wrap a raw socket and address into a @c TcpStream object.
    /// @param socket
    ///   Raw socket file descriptor.
    /// @param address
    ///   Remote address of this TCP stream.
    TcpStream(int socket, const InetAddress &address) noexcept
        : m_socket(socket), m_address(address) {}

    /// @brief
    ///   @c TcpStream is not copyable.
    TcpStream(const Self &other) = delete;

    /// @brief
    ///   Move constructor of @c TcpStream.
    /// @param[in, out] other
    ///   The @c TcpStream object to be moved. The moved object will be empty and can not be used
    ///   for IO operations.
    TcpStream(Self &&other) noexcept : m_socket(other.m_socket), m_address(other.m_address) {
        other.m_socket = -1;
    }

    /// @brief
    ///   Close the TCP connection and destroy this object.
    ~TcpStream() {
        if (m_socket != -1)
            ::close(m_socket);
    }

    /// @brief
    ///   @c TcpStream is not copyable.
    auto operator=(const Self &other) = delete;

    /// @brief
    ///   Move assignment of @c TcpStream.
    /// @param[in, out] other
    ///   The @c TcpStream object to be moved. The moved object will be empty and can not be used
    ///   for IO operations.
    /// @return
    ///   Reference to this @c TcpStream.
    auto operator=(Self &&other) noexcept -> Self & {
        if (this == &other) [[unlikely]]
            return *this;

        if (m_socket != -1)
            ::close(m_socket);

        m_socket       = other.m_socket;
        m_address      = other.m_address;
        other.m_socket = -1;

        return *this;
    }

    /// @brief
    ///   Get remote address of this TCP stream. It is undefined behavior to get remote address from
    ///   an empty @c TcpStream.
    /// @return
    ///   Remote address of this TCP stream.
    [[nodiscard]]
    auto remoteAddress() const noexcept -> const InetAddress & {
        return m_address;
    }

    /// @brief
    ///   Connect to the specified peer address. This method will block current thread until the
    ///   connection is established or any error occurs. If this TCP stream is currently not empty,
    ///   the old connection will be closed once the new connection is established. The old
    ///   connection will not be closed if the new connection fails.
    /// @param address
    ///   Peer Internet address to connect.
    /// @return
    ///   Error code of the connect operation. The error code is 0 if succeeded.
    NYAIO_API auto connect(const InetAddress &address) noexcept -> std::errc;

    /// @brief
    ///   Connect to the specified peer address. This method will suspend this coroutine until the
    ///   new connection is established or any error occurs. If this TCP stream is currently not
    ///   empty, the old connection will be closed once the new connection is established. The old
    ///   connection will not be closed if the new connection fails.
    /// @param address
    ///   Peer Internet address to connect.
    /// @return
    ///   Error code of the connect operation. The error code is 0 if succeeded.
    [[nodiscard]]
    auto connectAsync(const InetAddress &address) noexcept -> ConnectAwaiter {
        return {*this, address};
    }

    /// @brief
    ///   Send data to peer TCP endpoint. This method will block current thread until all data is
    ///   sent or any error occurs.
    /// @param data
    ///   Pointer to start of data to be sent.
    /// @param size
    ///   Expected size in byte of data to be sent.
    /// @return
    ///   A struct that contains number of bytes sent and an error code. The error code is
    ///   @c std::errc{} if succeeded and the number of bytes sent is valid.
    auto send(const void *data, std::uint32_t size) const noexcept -> SystemIoResult {
        ssize_t result = ::send(m_socket, data, size, MSG_NOSIGNAL);
        if (result == -1) [[unlikely]]
            return {0, std::errc{-errno}};
        return {static_cast<std::uint32_t>(result), std::errc{}};
    }

    /// @brief
    ///   Async send data to peer TCP endpoint. This method will suspend this coroutine until any
    ///   data is sent or any error occurs.
    /// @param data
    ///   Pointer to start of data to be sent.
    /// @param size
    ///   Expected size in byte of data to be sent.
    /// @return
    ///   A struct that contains number of bytes sent and an error code. The error code is
    ///   @c std::errc{} if succeeded and the number of bytes sent is valid.
    [[nodiscard]]
    auto sendAsync(const void *data, std::uint32_t size) const noexcept -> SendAwaitable {
        return {m_socket, data, size, MSG_NOSIGNAL};
    }

    /// @brief
    ///   Receive data from peer TCP endpoint. This method will block current thread until any data
    ///   is received or error occurs.
    /// @param[out] buffer
    ///   Pointer to start of buffer to store the received data.
    /// @param size
    ///   Maximum available size to be received.
    /// @return
    ///   A struct that contains number of bytes received and an error code. The error code is
    ///   @c std::errc{} if succeeded and the number of bytes received is valid.
    auto receive(void *buffer, std::uint32_t size) const noexcept -> SystemIoResult {
        ssize_t result = ::recv(m_socket, buffer, size, 0);
        if (result == -1) [[unlikely]]
            return {0, std::errc{-errno}};
        return {static_cast<std::uint32_t>(result), std::errc{}};
    }

    /// @brief
    ///   Async receive data from peer TCP endpoint. This method will suspend this coroutine until
    ///   any data is received or any error occurs.
    /// @param[out] buffer
    ///   Pointer to start of buffer to store the received data.
    /// @param size
    ///   Maximum available size to be received.
    /// @return
    ///   A struct that contains number of bytes received and an error code. The error code is
    ///   @c std::errc{} if succeeded and the number of bytes received is valid.
    [[nodiscard]]
    auto receiveAsync(void *buffer, std::uint32_t size) const noexcept -> ReceiveAwaitable {
        return {m_socket, buffer, size, 0};
    }

    /// @brief
    ///   Enable or disable keepalive for this TCP connection.
    /// @param enable
    ///   Specifies whether to enable or disable keepalive for this TCP stream.
    /// @return
    ///   An error code that indicates whether succeeded to enable or disable keepalive for this TCP
    ///   stream. The error code is 0 if succeeded to set keepalive attribute for this TCP stream.
    auto setKeepAlive(bool enable) noexcept -> std::errc {
        const int value = enable ? 1 : 0;
        int ret         = ::setsockopt(m_socket, SOL_SOCKET, SO_KEEPALIVE, &value, sizeof(value));
        if (ret == -1) [[unlikely]]
            return std::errc{errno};
        return {};
    }

    /// @brief
    ///   Enable or disable nodelay for this TCP stream.
    /// @param enable
    ///   Specifies whether to enable or disable nodelay for this TCP stream.
    /// @return
    ///   An error code that indicates whether succeeded to enable or disable nodelay for this TCP
    ///   stream. The error code is 0 if succeeded to set nodelay attribute for this TCP stream.
    auto setNoDelay(bool enable) noexcept -> std::errc {
        const int value = enable ? 1 : 0;
        int ret         = ::setsockopt(m_socket, SOL_TCP, TCP_NODELAY, &value, sizeof(value));
        if (ret == -1) [[unlikely]]
            return std::errc{errno};
        return {};
    }

    /// @brief
    ///   Set timeout event for send operation. @c TcpStream::send and @c TcpStream::sendAsync may
    ///   generate an error that indicates the timeout event if timeout event occurs. The TCP
    ///   connection may be in an undefined state and should be closed if send timeout event occurs.
    /// @tparam Rep
    ///   Type representation of duration type. See @c std::chrono::duration for details.
    /// @tparam Period
    ///   Ratio type that is used to measure how to do conversion between different duration types.
    ///   See @c std::chrono::duration for details.
    /// @param duration
    ///   Timeout duration of send operation. Ratios less than microseconds are not allowed.
    /// @return
    ///   An error code that indicates whether succeeded to set the timeout event. The error code is
    ///   0 if succeeded to set or remove send timeout event for this TCP connection.
    template <class Rep, class Period>
        requires(std::ratio_less_equal_v<std::micro, Period>)
    auto setSendTimeout(std::chrono::duration<Rep, Period> duration) noexcept -> std::errc {
        auto microsec = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
        if (microsec < 0) [[unlikely]]
            return std::errc::invalid_argument;

        const struct timeval timeout{
            .tv_sec  = static_cast<std::uint32_t>(microsec / 1000000),
            .tv_usec = static_cast<std::uint32_t>(microsec % 1000000),
        };

        int ret = ::setsockopt(m_socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        if (ret == -1) [[unlikely]]
            return std::errc{errno};

        return {};
    }

    /// @brief
    ///   Set timeout event for receive operation. @c TcpStream::receive and @c
    ///   TcpStream::receiveAsync may generate an error that indicates the timeout event if timeout
    ///   event occurs. The TCP connection may be in an undefined state and should be closed if
    ///   receive timeout event occurs.
    /// @tparam Rep
    ///   Type representation of duration type. See @c std::chrono::duration for details.
    /// @tparam Period
    ///   Ratio type that is used to measure how to do conversion between different duration types.
    ///   See @c std::chrono::duration for details.
    /// @param duration
    ///   Timeout duration of receive operation. Ratios less than microseconds are not allowed.
    /// @return
    ///   An error code that indicates whether succeeded to set the timeout event. The error code is
    ///   0 if succeeded to set or remove receive timeout event for this TCP connection.
    template <class Rep, class Period>
        requires(std::ratio_less_equal_v<std::micro, Period>)
    auto setReceiveTimeout(std::chrono::duration<Rep, Period> duration) noexcept -> std::errc {
        auto microsec = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
        if (microsec < 0) [[unlikely]]
            return std::errc::invalid_argument;

        const struct timeval timeout{
            .tv_sec  = static_cast<std::uint32_t>(microsec / 1000000),
            .tv_usec = static_cast<std::uint32_t>(microsec % 1000000),
        };

        int ret = ::setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        if (ret == -1) [[unlikely]]
            return std::errc{errno};

        return {};
    }

    /// @brief
    ///   Close this TCP stream and release all resources. Closing a TCP stream with pending IO
    ///   requirements may cause errors for the IO results. This method does nothing if current TCP
    ///   stream is empty.
    auto close() noexcept -> void {
        if (m_socket != -1) {
            ::close(m_socket);
            m_socket = -1;
        }
    }

private:
    int m_socket;
    InetAddress m_address;
};

/// @class TcpServer
/// @brief
///   Wrapper class for TCP server. This class is used for accepting incoming TCP connections.
class TcpServer {
public:
    using Self = TcpServer;

    /// @struct AcceptResult
    /// @brief
    ///   Result for accept operation. This struct contains the accepted TCP stream and error code.
    ///   The TCP stream is valid only if the error code is 0.
    struct AcceptResult {
        TcpStream stream;
        std::errc error;
    };

    /// @class AcceptAwaiter
    /// @brief
    ///   Customized accept awaitable for @c TcpServer. Using awaitable to avoid memory allocation
    ///   by @c Task.
    class AcceptAwaiter {
    public:
        using Self = AcceptAwaiter;

        /// @brief
        ///   Create a new @c AcceptAwaiter for accepting a new TCP connection.
        /// @param server
        ///   The server TCP socket.
        AcceptAwaiter(int server) noexcept
            : m_promise(), m_socket(server), m_addrLen(sizeof(InetAddress)), m_address() {}

        /// @brief
        ///   C++20 coroutine API method. Always execute @c await_suspend().
        /// @return
        ///   This function always returns @c false.
        [[nodiscard]]
        static constexpr auto await_ready() noexcept -> bool {
            return false;
        }

        /// @brief
        ///   Prepare for async accept operation and suspend the coroutine.
        /// @tparam T
        ///   Type of promise of current coroutine.
        /// @param coro
        ///   Current coroutine handle.
        template <class T>
            requires(std::is_base_of_v<detail::PromiseBase, T>)
        auto await_suspend(std::coroutine_handle<T> coro) noexcept -> void {
            auto &promise = static_cast<detail::PromiseBase &>(coro.promise());
            m_promise     = &promise;
            auto *worker  = promise.worker;

            io_uring_sqe *sqe = worker->pollSubmissionQueueEntry();
            while (sqe == nullptr) [[unlikely]] {
                worker->submit();
                sqe = worker->pollSubmissionQueueEntry();
            }

            sqe->opcode       = IORING_OP_ACCEPT;
            sqe->fd           = m_socket;
            sqe->addr         = reinterpret_cast<std::uintptr_t>(&m_address);
            sqe->off          = reinterpret_cast<std::uintptr_t>(&m_addrLen);
            sqe->accept_flags = SOCK_CLOEXEC;
            sqe->user_data    = reinterpret_cast<std::uintptr_t>(&promise);

            worker->flushSubmissionQueue();
        }

        /// @brief
        ///   Resume this coroutine and get result of the async connect operation.
        /// @return
        ///   A struct of accepted TCP stream and error code. The error code is 0 if succeeded to
        ///   accept a new connection.
        [[nodiscard]]
        auto await_resume() noexcept -> AcceptResult {
            if (m_promise->ioResult < 0) [[unlikely]]
                return {{}, std::errc{-m_promise->ioResult}};
            return {{m_promise->ioResult, m_address}, std::errc{}};
        }

    private:
        detail::PromiseBase *m_promise;
        int m_socket;
        socklen_t m_addrLen;
        InetAddress m_address;
    };

public:
    /// @brief
    ///   Create an empty @c TcpServer. Empty @c TcpServer object cannot be used for accepting new
    ///   TCP connections before binding.
    TcpServer() noexcept : m_socket(-1), m_address() {}

    /// @brief
    ///   @c TcpServer is not copyable.
    TcpServer(const Self &other) = delete;

    /// @brief
    ///   Move constructor of @c TcpServer.
    /// @param[in, out] other
    ///   The @c TcpServer object to be moved from. The moved object will be empty and can not be
    ///   used for accepting new TCP connections.
    TcpServer(Self &&other) noexcept : m_socket(other.m_socket), m_address(other.m_address) {
        other.m_socket = -1;
    }

    /// @brief
    ///   Stop listening to incoming TCP connections and destroy this object.
    ~TcpServer() {
        if (m_socket != -1)
            ::close(m_socket);
    }

    /// @brief
    ///   @c TcpServer is not copyable.
    auto operator=(const Self &other) = delete;

    /// @brief
    ///   Move assignment of @c TcpServer.
    /// @param[in, out] other
    ///   The @c TcpServer object to be moved from. The moved object will be empty and can not be
    ///   used for accepting new TCP connections.
    /// @return
    ///   Reference to this @c TcpServer.
    auto operator=(Self &&other) noexcept -> Self & {
        if (this == &other) [[unlikely]]
            return *this;

        if (m_socket != -1)
            ::close(m_socket);

        m_socket       = other.m_socket;
        m_address      = other.m_address;
        other.m_socket = -1;

        return *this;
    }

    /// @brief
    ///   Get local listening address. Get local listening address from an empty @c TcpServer is
    ///   undefined behavior.
    /// @return
    ///   Local listening address of this TCP server. The return value is undefined if this TCP
    ///   server is empty.
    [[nodiscard]]
    auto address() const noexcept -> const InetAddress & {
        return m_address;
    }

    /// @brief
    ///   Start listening to incoming TCP connections. This method will create a new TCP server
    ///   socket and bind to the specified address. The old TCP server socket will be closed if
    ///   succeeded to listen to the new address.
    /// @param address
    ///   The local address to bind for listening incoming TCP connections.
    /// @return
    ///   A system error code that indicates whether succeeded to start listening to incoming TCP
    ///   connections. The error code is 0 if succeeded to start listening. The original TCP server
    ///   socket will not be affected if any error occurs.
    NYAIO_API auto bind(const InetAddress &address) noexcept -> std::errc;

    /// @brief
    ///   Accept a new incoming TCP connection. This method will block current thread until a new
    ///   TCP connection is established or any error occurs.
    /// @return
    ///   A struct of accepted TCP stream and error code. The error code is 0 if succeeded to accept
    ///   a new connection.
    NYAIO_API auto accept() const noexcept -> AcceptResult;

    /// @brief
    ///   Async accept a new incoming TCP connection. This method will suspend this coroutine until
    ///   a new TCP connection is established or any error occurs.
    /// @return
    ///   A struct of accepted TCP stream and error code. The error code is 0 if succeeded to accept
    ///   a new connection.
    [[nodiscard]]
    auto acceptAsync() const noexcept -> AcceptAwaiter {
        return {m_socket};
    }

    /// @brief
    ///   Stop listening to incoming TCP connections and close this TCP server. This TCP server will
    ///   be set to empty after this call. Call @c TcpServer::bind() to start listening to incoming
    ///   again.
    auto close() noexcept -> void {
        if (m_socket != -1) {
            ::close(m_socket);
            m_socket = -1;
        }
    }

private:
    int m_socket;
    InetAddress m_address;
};

} // namespace nyaio
