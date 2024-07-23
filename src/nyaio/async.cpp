#include "nyaio/async.hpp"

#include <algorithm>
#include <csignal>
#include <system_error>

#include <sys/mman.h>
#include <unistd.h>

#ifndef __NR_io_uring_setup
#    define __NR_io_uring_setup 425
#endif
#ifndef __NR_io_uring_enter
#    define __NR_io_uring_enter 426
#endif

/// @brief
///   @c io_uring system call. Create a new @c io_uring.
/// @param entries
///   Expected number of entries for submission queue and completion queue. This value may be
///   aligned up.
/// @param[in, out] p
///   Parameters to setup this @c io_uring.
/// @return
///   File descriptor of the new @c io_uring if succeeded. Otherwise, return @c -errno.
[[nodiscard]]
static auto nyaio_io_uring_setup(unsigned entries, io_uring_params *p) noexcept -> int {
#if defined(__x86_64__) || defined(__x86_64)
    intptr_t rax;

    __asm__ volatile("syscall"
                     : "=a"(rax)
                     : "a"(__NR_io_uring_setup), "D"(entries), "S"(p)
                     : "rcx", "r11", "memory");

    return static_cast<int>(rax);
#elif defined(__i386__)
    intptr_t eax;

    __asm__ volatile("int $0x80"
                     : "=a"(eax)
                     : "a"(__NR_io_uring_setup), "b"(entries), "c"(p)
                     : "memory");

    return static_cast<int>(eax);
#else
#    error "Unsupported CPU architecture."
#endif
}

/// @brief
///   Initialize and complete @c io_uring IO operations.
/// @param fd
///   File descriptor of the @c io_uring.
/// @param to_submit
///   Number of submission queue entries to be submitted.
/// @param min_complete
///   Minimum expected IO operations to be completed before return.
/// @param flags
///   Flags of this system call.
/// @param sig
///   Signal set of this system call.
/// @return
///   Returns number of IO operations consumed if succeeded. Return @c -errno on error.
static auto nyaio_io_uring_enter(int fd, unsigned to_submit, unsigned min_complete, unsigned flags,
                                 sigset_t *sig) noexcept -> int {
#if defined(__x86_64__) || defined(__x86_64)
    intptr_t rax;
    intptr_t nsig      = _NSIG / 8;
    intptr_t flags_ptr = flags;

    __asm__ volatile("movq %[sig], %%r8\n\t"
                     "movq %[nsig], %%r9\n\t"
                     "movq %[flags], %%r10\n\t"
                     "syscall"
                     : "=a"(rax)
                     : [sig] "m"(sig), [nsig] "m"(nsig), [flags] "m"(flags_ptr),
                       "a"(__NR_io_uring_enter), "D"(fd), "S"(to_submit), "d"(min_complete)
                     : "rcx", "r11", "memory");

    return static_cast<int>(rax);
#elif defined(__i386__)
    intptr_t eax  = __NR_io_uring_enter;
    intptr_t arg6 = _NSIG / 8;

    __asm__ volatile("pushl	%[nsig]\n\t"
                     "pushl	%%ebp\n\t"
                     "movl	4(%%esp), %%ebp\n\t"
                     "int	$0x80\n\t"
                     "popl	%%ebp\n\t"
                     "addl	$4, %%esp"
                     : "+a"(eax)
                     : "b"(fd), "c"(to_submit), "d"(min_complete), "S"(flags),
                       "D"(sig), [nsig] "m"(arg6)
                     : "memory", "cc");

    return static_cast<int>(eax);
#else
#    error "Unsupported CPU architecture."
#endif
}

nyaio::detail::io_uring::io_uring() : m_ring(), m_flags(), m_features(), m_sq(), m_cq() {
    // Create io_uring.
    io_uring_params params{};
    m_ring = nyaio_io_uring_setup(4096, &params);

    m_flags    = params.flags;
    m_features = params.features;

    // Map memory.
    if (int ret = map_memory(params); ret < 0) [[unlikely]] {
        close(m_ring);
        throw std::system_error(-ret, std::system_category(), "mmap");
    }
}

nyaio::detail::io_uring::io_uring(uint32_t count, uint32_t flags, uint32_t features)
    : m_ring(), m_flags(), m_features(), m_sq(), m_cq() {
    // Create io_uring.
    io_uring_params params{};

    params.flags    = flags;
    params.features = features;

    m_ring = nyaio_io_uring_setup(count, &params);

    m_flags    = params.flags;
    m_features = params.features;

    // Map memory.
    if (int ret = map_memory(params); ret < 0) [[unlikely]] {
        close(m_ring);
        throw std::system_error(-ret, std::system_category(), "mmap");
    }
}

nyaio::detail::io_uring::~io_uring() {
    munmap(m_sq.mapped_data, m_sq.mapped_size);
    munmap(m_sq.sqes, m_sq.sqe_mapped_size);
    if (m_features & io_uring_feature_single_mmap)
        munmap(m_cq.mapped_data, m_cq.mapped_size);
    close(m_ring);
}

auto nyaio::detail::io_uring::wait_cqe() noexcept -> io_uring_cqe * {
    while (true) {
        if (auto *cqe = poll_cqe(); cqe != nullptr)
            return cqe;

        unsigned flags = IORING_ENTER_GETEVENTS;
        if (m_sq.flags->load(std::memory_order_relaxed) & IORING_SQ_NEED_WAKEUP) [[unlikely]]
            flags |= IORING_ENTER_SQ_WAKEUP;

        nyaio_io_uring_enter(m_ring, 0, 1, flags, nullptr);
    }
}

auto nyaio::detail::io_uring::submit() noexcept -> void {
    flush_sq();

    unsigned pending_sqes = m_sq.sqe_tail - m_sq.head->load(std::memory_order_relaxed);
    unsigned flags        = 0;

    unsigned sq_flags   = m_sq.flags->load(std::memory_order_relaxed);
    bool cq_needs_enter = (m_flags & io_uring_setup_iopoll) ||
                          (sq_flags & (IORING_SQ_CQ_OVERFLOW | IORING_SQ_TASKRUN));

    if (cq_needs_enter)
        flags |= IORING_ENTER_GETEVENTS;

    bool sq_needs_enter = false;
    do {
        if (pending_sqes == 0)
            break;

        if ((m_flags & io_uring_setup_sqpoll) == 0) {
            sq_needs_enter = true;
            break;
        }

        std::atomic_thread_fence(std::memory_order_seq_cst);
        if (m_sq.flags->load(std::memory_order_relaxed) & IORING_SQ_NEED_WAKEUP) {
            flags          |= IORING_ENTER_SQ_WAKEUP;
            sq_needs_enter  = true;
            break;
        }
    } while (false);

    if (sq_needs_enter || cq_needs_enter)
        nyaio_io_uring_enter(m_ring, pending_sqes, 0, flags, nullptr);
}

auto nyaio::detail::io_uring::map_memory(io_uring_params &params) noexcept -> int {
    { // Calculate required size.
        const size_t cqe_size = (m_flags & io_uring_setup_cqe32) ? 32 : sizeof(io_uring_cqe);
        m_sq.mapped_size      = params.sq_off.array + params.sq_entries * sizeof(unsigned);
        m_cq.mapped_size      = params.cq_off.cqes + params.cq_entries * cqe_size;
    }

    if (m_features & io_uring_feature_single_mmap) {
        size_t size      = std::max(m_sq.mapped_size, m_cq.mapped_size);
        m_sq.mapped_size = size;
        m_cq.mapped_size = size;
    }

    { // Map memory for submission queue.
        void *data = mmap(nullptr, m_sq.mapped_size, PROT_READ | PROT_WRITE,
                          MAP_SHARED | MAP_POPULATE, m_ring, IORING_OFF_SQ_RING);
        if (data == MAP_FAILED) [[unlikely]]
            return -errno;

        m_sq.mapped_data = data;
    }

    // Map memory for completion queue.
    if (m_features & io_uring_feature_single_mmap) {
        m_cq.mapped_data = m_sq.mapped_data;
    } else {
        void *data = mmap(nullptr, m_cq.mapped_size, PROT_READ | PROT_WRITE,
                          MAP_SHARED | MAP_POPULATE, m_ring, IORING_OFF_CQ_RING);
        if (data == MAP_FAILED) [[unlikely]] {
            int error = errno;
            munmap(m_sq.mapped_data, m_sq.mapped_size);
            return -error;
        }

        m_cq.mapped_data = data;
    }

    { // Map memory for submission queue entries.
        const size_t sqe_size = (m_flags & io_uring_setup_sqe128) ? 128 : sizeof(io_uring_sqe);
        m_sq.sqe_mapped_size  = sqe_size * params.sq_entries;

        void *data = mmap(nullptr, m_sq.sqe_mapped_size, PROT_READ | PROT_WRITE,
                          MAP_SHARED | MAP_POPULATE, m_ring, IORING_OFF_SQES);
        if (data == MAP_FAILED) [[unlikely]] {
            int error = errno;

            munmap(m_sq.mapped_data, m_sq.mapped_size);
            if (!(m_features & io_uring_feature_single_mmap))
                munmap(m_cq.mapped_data, m_cq.mapped_size);

            return -error;
        }

        m_sq.sqes = static_cast<io_uring_sqe *>(data);
    }

    { // Setup pointers for submission queue.
        auto *base = static_cast<uint8_t *>(m_sq.mapped_data);

        m_sq.head  = reinterpret_cast<std::atomic<unsigned> *>(base + params.sq_off.head);
        m_sq.tail  = reinterpret_cast<std::atomic<unsigned> *>(base + params.sq_off.tail);
        m_sq.flags = reinterpret_cast<std::atomic<unsigned> *>(base + params.sq_off.flags);

        m_sq.mask      = *reinterpret_cast<unsigned *>(base + params.sq_off.ring_mask);
        m_sq.sqe_count = *reinterpret_cast<unsigned *>(base + params.sq_off.ring_entries);
    }

    { // Setup pointers for completion queue.
        auto *base = static_cast<uint8_t *>(m_cq.mapped_data);

        m_cq.head = reinterpret_cast<std::atomic<unsigned> *>(base + params.cq_off.head);
        m_cq.tail = reinterpret_cast<std::atomic<unsigned> *>(base + params.cq_off.tail);
        m_cq.cqes = reinterpret_cast<io_uring_cqe *>(base + params.cq_off.cqes);
        if (params.cq_off.flags != 0)
            m_cq.flags = reinterpret_cast<std::atomic<unsigned> *>(base + params.cq_off.flags);

        m_cq.mask      = *reinterpret_cast<unsigned *>(base + params.cq_off.ring_mask);
        m_cq.cqe_count = *reinterpret_cast<unsigned *>(base + params.cq_off.ring_entries);
    }

    return 0;
}
