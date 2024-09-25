#include "nyaio/task.hpp"

#include <cassert>
#include <csignal>
#include <string_view>
#include <system_error>

#include <sys/mman.h>
#include <sys/utsname.h>

using namespace nyaio;
using namespace nyaio::detail;

namespace {

/// @class Defer
/// @brief
///   For internal usage. Used for temporary defer operations.
template <class Func>
class Defer {
public:
    using Self = Defer;

    /// @brief
    ///   Create a @c Defer object with the specified function.
    /// @param[in] func
    ///   The function to be deferred.
    Defer(Func &&func) noexcept : m_cancelled(false), m_function(std::move(func)) {}

    /// @brief
    ///   Defer is not copyable.
    Defer(const Self &other) = delete;

    /// @brief
    ///   Move constructor of @c Defer.
    /// @param[in, out] other
    ///   The @c Defer to be moved. @p other will be empty after this movement.
    Defer(Self &&other) noexcept
        : m_cancelled(other.m_cancelled), m_function(std::move(other.m_function)) {
        other.m_cancelled = true;
    }

    /// @brief
    ///   Destroy this @c Defer object. The deferred function will be executed if not cancelled.
    ~Defer() {
        if (!m_cancelled)
            m_function();
    }

    /// @brief
    ///   Defer is not copyable.
    auto operator=(const Self &other) = delete;

    /// @brief
    ///   Move assignment of @c Defer.
    /// @param[in, out] other
    ///   The @c Defer to be moved. @p other will be empty after this movement.
    /// @return
    ///   Reference to this @c Defer object.
    auto operator=(Self &&other) noexcept -> Self & {
        if (this == &other) [[unlikely]]
            return *this;

        m_cancelled       = other.m_cancelled;
        m_function        = std::move(other.m_function);
        other.m_cancelled = true;

        return *this;
    }

    /// @brief
    ///   Cancel this @c Defer object. The deferred function will not be executed.
    auto cancel() noexcept -> void {
        m_cancelled = true;
    }

private:
    bool m_cancelled;
    Func m_function;
};

/// @brief
///   Deducing guide for @c Defer.
template <class T>
Defer(T &&func) -> Defer<T>;

} // namespace

namespace {

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
auto ioUringSetup(unsigned entries, io_uring_params *p) noexcept -> int {
#if defined(__x86_64__) || defined(__x86_64)
    std::intptr_t rax;
    __asm__ volatile("syscall"
                     : "=a"(rax)
                     : "a"(__NR_io_uring_setup), "D"(entries), "S"(p)
                     : "rcx", "r11", "memory");
    return static_cast<int>(rax);
#elif defined(__i386__)
    std::intptr_t eax;
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
auto ioUringEnter(int fd, unsigned to_submit, unsigned min_complete, unsigned flags,
                  sigset_t *sig) noexcept -> int {
#if defined(__x86_64__) || defined(__x86_64)
    std::intptr_t rax;
    std::intptr_t nsig      = _NSIG / 8;
    std::intptr_t flags_ptr = flags;

    __asm__ volatile("movq %[sig], %%r8\n\t"
                     "movq %[nsig], %%r9\n\t"
                     "movq %[flags], %%r10\n\t"
                     "syscall"
                     : "=a"(rax)
                     : [sig] "m"(sig), [nsig] "m"(nsig), [flags] "m"(flags_ptr),
                       "a"(__NR_io_uring_enter), "D"(fd), "S"(to_submit), "d"(min_complete)
                     : "rcx", "r8", "r9", "r10", "r11", "memory");

    return static_cast<int>(rax);
#elif defined(__i386__)
    std::intptr_t eax  = __NR_io_uring_enter;
    std::intptr_t arg6 = _NSIG / 8;

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

/// @brief
///   Create an unsigned int that represents a version number.
/// @param major
///   Major linux kernel version number.
/// @param minor
///   Minor linux kernel version number.
/// @param patch
///   Patch linux kernel version number.
[[nodiscard]]
constexpr auto makeVersion(std::uint8_t major, std::uint8_t minor, std::uint8_t patch) noexcept
    -> std::uint32_t {
    return (static_cast<std::uint32_t>(major) << 16) | (static_cast<std::uint32_t>(minor) << 8) |
           patch;
}

/// @brief
///   Get current linux kernel version. This is used to check if certain @c io_uring features are
///   supported.
/// @return
///   An unsigned integer that represents current linux kernel version. This is created via function
///   @c makeVersion.
[[nodiscard]]
auto kernelVersion() noexcept -> std::uint32_t {
    std::uint8_t versions[3]{};

    struct utsname name;
    if (::uname(&name) != 0)
        return 0;

    std::string_view s = name.release;
    std::uint8_t *v    = versions;

    for (char c : s) {
        if (c >= '0' && c <= '9')
            *v = *v * 10 + static_cast<std::uint8_t>(c) - '0';
        else if (c == '.')
            ++v;
        else
            break;

        if (v >= versions + std::size(versions)) [[unlikely]]
            break;
    }

    return makeVersion(versions[0], versions[1], versions[2]);
}

/// @brief
///   Get available @c io_uring setup flags according to current kernel version.
/// @return
///   Available @c io_uring setup flags.
[[nodiscard]]
auto ioUringAvailableSetupFlags() noexcept -> std::uint32_t {
    std::uint32_t flags = 0;

    const std::uint32_t v = kernelVersion();

    // kernel version 5.11.0
    flags |= IORING_SETUP_SQPOLL;

    // kernel version 6.0.0
    flags |= IORING_SETUP_SINGLE_ISSUER;

    if (v >= makeVersion(6, 6, 0))
        flags |= IORING_SETUP_NO_SQARRAY;

    return flags;
}

/// @brief
///   Get available @c io_uring feature flags according to current kernel version.
/// @return
///   Available @c io_uring feature flags.
[[nodiscard]]
auto ioUringAvailableFeatureFlags() noexcept -> std::uint32_t {
    std::uint32_t features = 0;

    // kernel version 5.4.0
    features |= IORING_FEAT_SINGLE_MMAP;

    // kernel version 5.6.0
    features |= IORING_FEAT_RW_CUR_POS;

    // kernel version 5.7.0
    features |= IORING_FEAT_FAST_POLL;

    // kernel version 5.11.0
    features |= IORING_FEAT_SQPOLL_NONFIXED;

    // kernel version 5.19.0
    features |= IORING_FEAT_NODROP;

    return features;
}

} // namespace

IoContextWorker::IoContextWorker()
    : m_isRunning(false), m_shouldStop(false), m_ring(-1), m_flags(ioUringAvailableSetupFlags()),
      m_features(ioUringAvailableFeatureFlags()), m_sq(), m_cq() {
    // Create io_uring.
    io_uring_params params{
        .sq_entries     = 0,
        .cq_entries     = 0,
        .flags          = m_flags,
        .sq_thread_cpu  = 0,
        .sq_thread_idle = 0,
        .features       = m_features,
        .wq_fd          = 0,
        .resv           = {},
        .sq_off         = {},
        .cq_off         = {},
    };

    m_ring = ioUringSetup(4096, &params);
    if (m_ring < 0)
        throw std::system_error(-m_ring, std::system_category(), "io_uring_setup");

    auto ringGuard = Defer([this] { ::close(m_ring); });

    // map memory.
    m_sq.mappedSize = params.sq_off.array + params.sq_entries * sizeof(unsigned);
    m_cq.mappedSize = params.cq_off.cqes + params.cq_entries * sizeof(io_uring_cqe);

    // Use single mmap.
    m_sq.mappedSize = std::max(m_sq.mappedSize, m_cq.mappedSize);
    m_cq.mappedSize = m_sq.mappedSize;

    { // Map memory for submission queue and completion queue.
        void *data = ::mmap(nullptr, m_sq.mappedSize, PROT_READ | PROT_WRITE,
                            MAP_SHARED | MAP_POPULATE, m_ring, IORING_OFF_SQ_RING);
        if (data == MAP_FAILED) [[unlikely]]
            throw std::system_error(errno, std::system_category(), "mmap");

        m_sq.mappedData = data;
        m_cq.mappedData = data;
    }

    auto sqDataGuard = Defer([this] { ::munmap(m_sq.mappedData, m_sq.mappedSize); });

    { // Map memory for submission queue entries.
        m_sq.sqeMappedSize = params.sq_entries * sizeof(io_uring_sqe);

        void *data = ::mmap(nullptr, m_sq.sqeMappedSize, PROT_READ | PROT_WRITE,
                            MAP_SHARED | MAP_POPULATE, m_ring, IORING_OFF_SQES);
        if (data == MAP_FAILED) [[unlikely]]
            throw std::system_error(errno, std::system_category(), "mmap");

        m_sq.sqes = static_cast<io_uring_sqe *>(data);
    }

    { // Setup pointers for submission queue.
        auto *base = static_cast<std::uint8_t *>(m_sq.mappedData);
        auto &sq   = m_sq;

        sq.head  = reinterpret_cast<std::atomic<unsigned> *>(base + params.sq_off.head);
        sq.tail  = reinterpret_cast<std::atomic<unsigned> *>(base + params.sq_off.tail);
        sq.flags = reinterpret_cast<std::atomic<unsigned> *>(base + params.sq_off.flags);

        sq.mask     = *reinterpret_cast<unsigned *>(base + params.sq_off.ring_mask);
        sq.sqeCount = *reinterpret_cast<unsigned *>(base + params.sq_off.ring_entries);
    }

    { // Setup pointers for completion queue.
        auto *base = static_cast<std::uint8_t *>(m_sq.mappedData);
        auto &cq   = m_cq;

        cq.head = reinterpret_cast<std::atomic<unsigned> *>(base + params.cq_off.head);
        cq.tail = reinterpret_cast<std::atomic<unsigned> *>(base + params.cq_off.tail);
        cq.cqes = reinterpret_cast<io_uring_cqe *>(base + params.cq_off.cqes);
        if (params.cq_off.flags != 0)
            cq.flags = reinterpret_cast<std::atomic<unsigned> *>(base + params.cq_off.flags);

        cq.mask     = *reinterpret_cast<unsigned *>(base + params.cq_off.ring_mask);
        cq.cqeCount = *reinterpret_cast<unsigned *>(base + params.cq_off.ring_entries);
    }

    // Succeeded.
    sqDataGuard.cancel();
    ringGuard.cancel();
}

IoContextWorker::~IoContextWorker() {
    assert(!m_isRunning.load(std::memory_order_relaxed));
    ::close(m_ring);
    ::munmap(m_sq.mappedData, m_sq.mappedSize);
    ::munmap(m_sq.sqes, m_sq.sqeMappedSize);
}

auto IoContextWorker::run() noexcept -> void {
    if (m_isRunning.exchange(true, std::memory_order_relaxed)) [[unlikely]]
        return;

    auto nextCompletionQueueEntry = [this]() noexcept -> io_uring_cqe * {
        while (!m_shouldStop.load(std::memory_order_relaxed)) {
            if (auto *cqe = pollCompletionQueueEntry(); cqe != nullptr)
                return cqe;

            unsigned flags = IORING_ENTER_GETEVENTS;
            if (m_sq.flags->load(std::memory_order_relaxed) | IORING_SQ_NEED_WAKEUP)
                flags |= IORING_ENTER_SQ_WAKEUP;

            ioUringEnter(m_ring, 0, 1, flags, nullptr);
        }

        return nullptr;
    };

    // It is possible that other threads have scheduled some tasks before start.
    m_shouldStop.store(false, std::memory_order_seq_cst);
    while (true) {
        submit();

        auto *cqe = nextCompletionQueueEntry();
        if (cqe == nullptr) [[unlikely]]
            break;

        // cqe is not null here.
        do {
            auto *p = reinterpret_cast<PromiseBase *>(static_cast<std::uintptr_t>(cqe->user_data));

            // p may be null for linked timeout event.
            if (p == nullptr) [[unlikely]] {
                consumeCompletionQueueEntries(1);
                cqe = pollCompletionQueueEntry();
                continue;
            }

            p->ioFlags  = cqe->flags;
            p->ioResult = cqe->res;

            // It is safe to mark current cqe as consumed.
            consumeCompletionQueueEntries(1);

            if (!p->isCancelled()) [[likely]] {
                auto &stack = p->stackBottomPromise();
                p->coroutine().resume();
                if (stack.coroutine().done())
                    stack.release();
            }

            // Try to handle next completion event.
            cqe = pollCompletionQueueEntry();
        } while (cqe != nullptr);
    }

    m_isRunning.store(false, std::memory_order_relaxed);
}

auto IoContextWorker::submit() noexcept -> void {
    flushSubmissionQueue();

    unsigned pendingSqes = m_sq.sqeTail - m_sq.head->load(std::memory_order_relaxed);
    unsigned flags       = 0;

    unsigned sqFlags  = m_sq.flags->load(std::memory_order_relaxed);
    bool cqNeedsEnter = (sqFlags & (IORING_SQ_CQ_OVERFLOW | IORING_SQ_TASKRUN)) != 0;

    if (cqNeedsEnter)
        flags |= IORING_ENTER_GETEVENTS;

    bool sqNeedsEnter = false;

    do {
        if (pendingSqes == 0)
            break;

        std::atomic_thread_fence(std::memory_order_seq_cst);
        if (m_sq.flags->load(std::memory_order_relaxed) & IORING_SQ_NEED_WAKEUP) {
            flags        |= IORING_ENTER_SQ_WAKEUP;
            sqNeedsEnter  = true;
            break;
        }
    } while (false);

    if (sqNeedsEnter || cqNeedsEnter)
        ioUringEnter(m_ring, pendingSqes, 0, flags, nullptr);
}

IoContext::IoContext() : m_numWorkers(), m_next(), m_workers(), m_threads() {
    m_numWorkers = std::thread::hardware_concurrency() / 2;
    if (m_numWorkers == 0) [[unlikely]]
        m_numWorkers = 1;

    m_workers = std::make_unique<IoContextWorker[]>(static_cast<std::uint32_t>(m_numWorkers));
    m_threads = std::make_unique<std::jthread[]>(static_cast<std::uint32_t>(m_numWorkers));
}

IoContext::IoContext(std::size_t count) : m_numWorkers(), m_next(), m_workers(), m_threads() {
    if (count == 0)
        count = std::thread::hardware_concurrency() / 2;

    if (count == 0) [[unlikely]]
        count = 1;

    m_numWorkers = count;
    m_workers    = std::make_unique<IoContextWorker[]>(static_cast<std::uint32_t>(count));
    m_threads    = std::make_unique<std::jthread[]>(static_cast<std::uint32_t>(count));
}

IoContext::~IoContext() {
    stop();
}
