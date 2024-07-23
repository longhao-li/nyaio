#pragma once

#include <atomic>
#include <cstring>

#include <linux/io_uring.h>

namespace nyaio::detail {

enum io_uring_setup_flag : uint32_t {
    io_uring_setup_none               = 0,
    io_uring_setup_iopoll             = (1U << 0),
    io_uring_setup_sqpoll             = (1U << 1),
    io_uring_setup_sq_aff             = (1U << 2),
    io_uring_setup_cqsize             = (1U << 3),
    io_uring_setup_clamp              = (1U << 4),
    io_uring_setup_attach_wq          = (1U << 5),
    io_uring_setup_ring_disabled      = (1U << 6),
    io_uring_setup_submit_all         = (1U << 7),
    io_uring_setup_coop_task_run      = (1U << 8),
    io_uring_setup_task_run_flag      = (1U << 9),
    io_uring_setup_sqe128             = (1U << 10),
    io_uring_setup_cqe32              = (1U << 11),
    io_uring_setup_single_issuer      = (1U << 12),
    io_uring_setup_defer_taskrun      = (1U << 13),
    io_uring_setup_no_mmap            = (1U << 14),
    io_uring_setup_registered_fd_only = (1U << 15),
    io_uring_setup_no_sqarray         = (1U << 16),
};

enum io_uring_feature_flag : uint32_t {
    io_uring_feature_none            = 0,
    io_uring_feature_single_mmap     = (1U << 0),
    io_uring_feature_nodrop          = (1U << 1),
    io_uring_feature_submit_stable   = (1U << 2),
    io_uring_feature_rw_cur_pos      = (1U << 3),
    io_uring_feature_cur_personality = (1U << 4),
    io_uring_feature_fast_poll       = (1U << 5),
    io_uring_feature_poll_32bits     = (1U << 6),
    io_uring_feature_sqpoll_nonfixed = (1U << 7),
    io_uring_feature_ext_arg         = (1U << 8),
    io_uring_feature_native_workers  = (1U << 9),
    io_uring_feature_rsrc_tags       = (1U << 10),
    io_uring_feature_cqe_skip        = (1U << 11),
    io_uring_feature_linked_file     = (1U << 12),
    io_uring_feature_reg_reg_ring    = (1U << 13),
};

/// @class io_uring
/// @brief
///   Wrapper class for linux @c io_uring. This class uses linux system calls and does not rely on
///   @c liburing.
class io_uring {
public:
    /// @brief
    ///   Create and initialize a new @c io_uring object. This @c io_uring contains 4096 entries for
    ///   submission queue and completion queue.
    /// @throws std::system_error
    ///   Thrown if failed to create new @c io_uring and map memory.
    NYAIO_API io_uring();

    /// @brief
    ///   Create and initialize a new @c io_uring object with given options.
    /// @param count
    ///   Expected number of entries of submission queue and completion queue. This value may be
    ///   aligned up by kernel.
    /// @param flags
    ///   Setup flags for this @c io_uring.
    /// @param features
    ///   Feature flags for this @c io_uring.
    /// @throws std::system_error
    ///   Thrown if failed to create new @c io_uring and map memory.
    NYAIO_API io_uring(uint32_t count, uint32_t flags, uint32_t features);

    /// @brief
    ///   @c io_uring is not copyable.
    io_uring(const io_uring &other) = delete;

    /// @brief
    ///   @c io_uring is not movable.
    io_uring(io_uring &&other) = delete;

    /// @brief
    ///   Destroy this @c io_uring.
    NYAIO_API ~io_uring();

    /// @brief
    ///   @c io_uring is not copyable.
    auto operator=(const io_uring &other) = delete;

    /// @brief
    ///   @c io_uring is not movable.
    auto operator=(io_uring &&other) = delete;

    /// @brief
    ///   Try to acquire a new @c io_uring submission queue entry.
    /// @return
    ///   Pointer to a submission queue entry if succeeded. All fields of the submission queue entry
    ///   will be initialized to 0 if succeeded to acquire one. Return @c nullptr if there is no
    ///   available submission queue entry currently.
    [[nodiscard]]
    auto poll_sqe() noexcept -> io_uring_sqe * {
        unsigned head = m_sq.head->load(std::memory_order_acquire);
        unsigned next = m_sq.sqe_tail + 1;

        if (next - head <= m_sq.sqe_count) {
            const size_t shift = (m_flags & io_uring_setup_sqe128) ? 1 : 0;
            io_uring_sqe *sqe  = m_sq.sqes + ((m_sq.sqe_tail & m_sq.mask) << shift);
            m_sq.sqe_tail      = next;

            // Initialize the new sqe.
            memset(sqe, 0, sizeof(io_uring_sqe));
            return sqe;
        }

        return nullptr;
    }

    /// @brief
    ///   Flush the submission queue so that @c io_uring can see new submission queue entries. This
    ///   method does not trigger system call.
    auto flush_sq() noexcept -> void {
        if (m_sq.sqe_head != m_sq.sqe_tail) [[likely]] {
            m_sq.sqe_head = m_sq.sqe_tail;
            m_sq.tail->store(m_sq.sqe_tail, std::memory_order_release);
        }
    }

    /// @brief
    ///   Try to get a completion queue entry from the completion queue immediately.
    /// @return
    ///   Pointer to a new completion queue entry if succeeded. Return @c nullptr if there is no
    ///   completion queue entry available.
    [[nodiscard]]
    auto poll_cqe() noexcept -> io_uring_cqe * {
        const size_t shift = (m_flags & io_uring_setup_cqe32) ? 1 : 0;

        unsigned tail = m_cq.tail->load(std::memory_order_acquire);
        unsigned head = m_cq.head->load(std::memory_order_relaxed);

        if (head == tail)
            return nullptr;

        io_uring_cqe *cqe = m_cq.cqes + ((head & m_cq.mask) << shift);
        return cqe;
    }

    /// @brief
    ///   Wait for a completion queue entry until at least one is available.
    /// @return
    ///   Pointer to a new completion queue entry.
    [[nodiscard]]
    NYAIO_API auto wait_cqe() noexcept -> io_uring_cqe *;

    /// @brief
    ///   Marks that @p count @c io_uring_cqes are consumed and could be overriden.
    /// @param count
    ///   Number of completion queue entries to be consumed.
    auto consume_cqes(unsigned count) noexcept -> void {
        m_cq.head->fetch_add(count, std::memory_order_release);
    }

    /// @brief
    ///   Flush the submission queue and tell the kernel to handle the new submission entries.
    NYAIO_API auto submit() noexcept -> void;

private:
    /// @brief
    ///   For internal usage. Do memory mapping for submission queue and completion queue.
    /// @param[in] params
    ///   Pointer to @c io_uring_params object for this ring.
    /// @return
    ///   An integer that indicates the memory mapping result. Return 0 if succeeded.
    auto map_memory(io_uring_params &params) noexcept -> int;

private:
    int m_ring;
    uint32_t m_flags;
    uint32_t m_features;

    struct {
        void *mapped_data;
        size_t mapped_size;

        io_uring_sqe *sqes;
        size_t sqe_mapped_size;

        uint32_t sqe_head;
        uint32_t sqe_tail;
        uint32_t sqe_count;
        uint32_t mask;

        std::atomic<unsigned> *head;
        std::atomic<unsigned> *tail;
        std::atomic<unsigned> *flags;
    } m_sq;

    struct {
        void *mapped_data;
        size_t mapped_size;

        io_uring_cqe *cqes;

        uint32_t cqe_count;
        uint32_t mask;

        std::atomic<unsigned> *head;
        std::atomic<unsigned> *tail;
        std::atomic<unsigned> *flags;
    } m_cq;
};

} // namespace nyaio::detail
