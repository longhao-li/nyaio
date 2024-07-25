#pragma once

#include <atomic>
#include <coroutine>
#include <exception>
#include <expected>
#include <memory>
#include <optional>
#include <thread>

#include <linux/io_uring.h>
#include <sys/socket.h>

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
            // It is possible to optimize this as 4 movaps instructions, but I am too lazy to do so.
            __builtin_memset(sqe, 0, sizeof(io_uring_sqe));
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
    ///   Marks that @p count @c io_uring_cqes are consumed and could be overridden.
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

namespace nyaio {
namespace detail {

/// @class promise_base
/// @brief
///   Base class of coroutine promises.
class promise_base {
public:
    /// @struct final_awaitable
    /// @brief
    ///   For internal usage. Final awaitable object for tasks. See C++20 coroutine documents for
    ///   details.
    struct final_awaitable {
        /// @brief
        ///   For internal usage. C++20 coroutine API. Coroutines should always maintain coroutine
        ///   stack before finalizing.
        /// @return
        ///   This method always returns @c false.
        static constexpr auto await_ready() noexcept -> bool {
            return false;
        }

        /// @brief
        ///   For internal usage. C++20 coroutine API. Maintain the coroutine promise status and do
        ///   coroutine context switch.
        /// @tparam Promise
        ///   Promise type of the coroutine to be finalized.
        /// @param coro
        ///   The coroutine to be finalized.
        /// @return
        ///   The caller coroutine to be resumed. A noop coroutine is returned if the finalized
        ///   coroutine is the coroutine stack bottom.
        template <class Promise>
            requires(std::is_base_of_v<promise_base, Promise>)
        static auto
        await_suspend(std::coroutine_handle<Promise> coro) noexcept -> std::coroutine_handle<> {
            auto &p = static_cast<promise_base &>(coro.promise());
            if (p.m_caller == nullptr)
                return std::noop_coroutine();

            // The caller coroutine should be resumed.
            if (p.notify() == 1)
                return p.m_caller;

            return std::noop_coroutine();
        }

        /// @brief
        ///   For internal usage. C++20 coroutine API. Do nothing.
        static constexpr auto await_resume() noexcept -> void {}
    };

public:
    /// @brief
    ///   Initialize this promise object.
    promise_base() noexcept
        : m_is_cancelled(false), m_reference_count(1), m_waiting_tasks(0), m_coroutine(),
          m_caller(), m_caller_promise(), m_stack_bottom_promise(), m_io_uring_flags(),
          m_io_uring_result(), m_worker(), m_exception() {}

    /// @brief
    ///   Promise is not copyable.
    promise_base(const promise_base &other) = delete;

    /// @brief
    ///   Promise is not movable.
    promise_base(promise_base &&other) = delete;

    /// @brief
    ///   Destroy this promise and release resources.
    ~promise_base() = default;

    /// @brief
    ///   Promise is not copyable.
    auto operator=(const promise_base &other) = delete;

    /// @brief
    ///   Promise is not movable.
    auto operator=(promise_base &&other) = delete;

    /// @brief
    ///   For internal usage. C++20 coroutine API. Tasks should not execute immediately on created.
    /// @return
    ///   Returns a @c std::suspend_always object.
    [[nodiscard]]
    static constexpr auto initial_suspend() noexcept -> std::suspend_always {
        return {};
    }

    /// @brief
    ///   For internal usage. C++20 coroutine API. Return @c final_awaitable to maintain the
    ///   coroutine stack and context. See @c final_awaitable for more details.
    /// @return
    ///   Returns a @c final_awaitable object.
    [[nodiscard]]
    static constexpr auto final_suspend() noexcept -> final_awaitable {
        return {};
    }

    /// @brief
    ///   For internal usage. C++20 coroutine API. Catch and save unhandled exception for this
    ///   coroutine.
    auto unhandled_exception() noexcept -> void {
        m_exception = std::current_exception();
    }

    /// @brief
    ///   Get coroutine handle for this promise.
    /// @return
    ///   Coroutine handle for this promise.
    [[nodiscard]]
    auto coroutine() const noexcept -> std::coroutine_handle<> {
        return m_coroutine;
    }

    /// @brief
    ///   Increase reference count for this coroutine.
    auto acquire() noexcept -> void {
        m_reference_count.fetch_add(1, std::memory_order_relaxed);
    }

    /// @brief
    ///   Decrease reference count for this coroutine and destroy it if necessary.
    auto release() noexcept -> void {
        int count = m_reference_count.fetch_sub(1, std::memory_order_relaxed);
        if (count == 1)
            m_coroutine.destroy();
    }

    /// @brief
    ///   Get current reference counting for this coroutine.
    /// @return
    ///   Current reference counting for this coroutine.
    [[nodiscard]]
    auto reference_count() const noexcept -> size_t {
        return static_cast<size_t>(m_reference_count.load(std::memory_order_relaxed));
    }

    /// @brief
    ///   Marks that this coroutine should wait for a new task to be done.
    /// @param count
    ///   Number of tasks to be waited.
    auto wait(size_t count = 1) noexcept -> void {
        m_waiting_tasks.fetch_add(static_cast<int>(count), std::memory_order_relaxed);
    }

    /// @brief
    ///   Marks that a task that this coroutine is waiting for has been completed.
    /// @return
    ///   Number of waiting tasks before notify. That is, the return value is 1 if this coroutine
    ///   should be resumed.
    auto notify() noexcept -> size_t {
        return static_cast<size_t>(m_waiting_tasks.fetch_sub(1, std::memory_order_relaxed));
    }

    /// @brief
    ///   Get number of tasks that this coroutine is waiting for.
    /// @return
    ///   Number of tasks that this coroutine is waiting for.
    [[nodiscard]]
    auto waiting_tasks() const noexcept -> size_t {
        return static_cast<size_t>(m_waiting_tasks.load(std::memory_order_relaxed));
    }

    /// @brief
    ///   Marks that this coroutine should be cancelled. Cancelled tasks will not be resumed by
    ///   executors.
    auto cancel() noexcept -> void {
        m_is_cancelled.store(true, std::memory_order_relaxed);
    }

    /// @brief
    ///   Checks if this coroutine should be cancelled.
    /// @retval true
    ///   This coroutine is marked as cancelled.
    /// @retval false
    ///   This coroutine is not marked as cancelled.
    [[nodiscard]]
    auto is_cancelled() const noexcept -> bool {
        return m_is_cancelled.load(std::memory_order_relaxed);
    }

    /// @brief
    ///   Get promise for the coroutine stack bottom coroutine.
    /// @return
    ///   Reference to the coroutine call stack bottom coroutine.
    [[nodiscard]]
    auto stack_bottom_promise() const noexcept -> promise_base & {
        return *m_stack_bottom_promise;
    }

    /// @brief
    ///   For internal usage. Set flags returned by @c io_uring_cqe.
    /// @param flags
    ///   Flags returned by @c io_uring_cqe.
    auto set_io_uring_flags(uint32_t flags) noexcept -> void {
        m_io_uring_flags = flags;
    }

    /// @brief
    ///   Get flags returned by @c io_uring_cqe.
    /// @return
    ///   Flags returned by @c io_uring_cqe.
    [[nodiscard]]
    auto io_uring_flags() const noexcept -> uint32_t {
        return m_io_uring_flags;
    }

    /// @brief
    ///   Set result returned by @c io_uring_cqe.
    /// @param result
    ///   Result returned by @c io_uring_cqe.
    auto set_io_uring_result(int result) noexcept -> void {
        m_io_uring_result = result;
    }

    /// @brief
    ///   Get result returned by @c io_uring_cqe.
    /// @return
    ///   Result returned by @c io_uring_cqe.
    [[nodiscard]]
    auto io_uring_result() const noexcept -> int {
        return m_io_uring_result;
    }

    /// @brief
    ///   Get worker of this coroutine. Worker will be automatically passed to subcoroutines.
    ///   Generally, this could be any user-sepcified pointer.
    /// @return
    ///   Pointer to current worker.
    [[nodiscard]]
    auto worker() const noexcept -> void * {
        return m_worker;
    }

    /// @brief
    ///   Set worker for this coroutine.
    /// @param[in] w
    ///   The worker to be set. Worker will be automatically passed to subcoroutines. Generally,
    ///   this could be any user-sepcified pointer.
    auto set_worker(void *w) noexcept -> void {
        m_worker = w;
    }

    template <class>
    friend class task_awaitable;

protected:
    std::atomic_bool m_is_cancelled;
    std::atomic_int m_reference_count;
    std::atomic_int m_waiting_tasks;

    std::coroutine_handle<> m_coroutine;
    std::coroutine_handle<> m_caller;
    promise_base *m_caller_promise;
    promise_base *m_stack_bottom_promise;

    uint32_t m_io_uring_flags;
    int m_io_uring_result;

    void *m_worker;
    std::exception_ptr m_exception;
};

} // namespace detail

/// @class promise
/// @tparam T
///   Type of return value of the owner task.
/// @brief
///   Promise type for corresponding task.
template <class T>
class promise;

namespace detail {

/// @class task_awaitable
/// @tparam T
///   Return type of the coroutine.
/// @brief
///   Awaitable class for @c task.
template <class T>
class task_awaitable {
public:
    using value_type       = T;
    using promise_type     = promise<value_type>;
    using coroutine_handle = std::coroutine_handle<promise_type>;

    /// @brief
    ///   For internal usage. Create a @c task_awaitable object for the specified @c task to be
    ///   awaited.
    explicit task_awaitable(coroutine_handle coro) noexcept : m_coroutine(coro) {}

    /// @brief
    ///   For internal usage. C++20 coroutine standard API. Checks if this coroutine should be
    ///   suspended. Empty coroutine and completed coroutine should not be suspended.
    /// @retval true
    ///   Current coroutine should be suspended.
    /// @retval false
    ///   Current coroutine should not be suspended.
    [[nodiscard]]
    auto await_ready() const noexcept -> bool {
        return m_coroutine == nullptr || m_coroutine.done();
    }

    /// @brief
    ///   For internal usage. C++20 coroutine API. Do some preparation before suspending the caller
    ///   coroutine.
    template <class Promise>
        requires(std::is_base_of_v<promise_base, Promise>)
    auto await_suspend(std::coroutine_handle<Promise> caller) noexcept -> std::coroutine_handle<> {
        auto &p  = static_cast<promise_base &>(m_coroutine.promise());
        auto &cp = static_cast<promise_base &>(caller.promise());

        p.m_caller               = caller;
        p.m_caller_promise       = &cp;
        p.m_stack_bottom_promise = cp.m_stack_bottom_promise;
        p.m_worker               = cp.m_worker;

        // The caller should wait for this coroutine completes.
        cp.wait();

        return m_coroutine;
    }

    /// @brief
    ///   For internal usage. C++20 coroutine API. Get result of the task.
    /// @return
    ///   Result of the task.
    auto await_resume() const -> value_type {
        return std::move(m_coroutine.promise()).result();
    }

private:
    coroutine_handle m_coroutine;
};

} // namespace detail

/// @class task
/// @tparam T
///   Return type of this coroutine.
/// @brief
///   @c Task is a wrapper of coroutine that can be awaited.
template <class T>
class task {
public:
    using value_type       = T;
    using promise_type     = nyaio::promise<value_type>;
    using coroutine_handle = std::coroutine_handle<promise_type>;
    using awaitable_type   = detail::task_awaitable<value_type>;

    /// @brief
    ///   Create an empty task. Empty task cannot be executed.
    task() noexcept : m_coroutine(), m_promise() {}

    /// @brief
    ///   Wrap the specified coroutine as a @c task object.
    /// @param coro
    ///   Coroutine handle of this task.
    /// @param[in] p
    ///   Promise of this task.
    task(coroutine_handle coro, promise_type &p) noexcept : m_coroutine(coro), m_promise(&p) {}

    /// @brief
    ///   Copy constructor of @c task. Reference counting is used.
    /// @param other
    ///   The @c task to be copied.
    task(const task &other) noexcept : m_coroutine(), m_promise(other.m_promise) {
        // Add reference count.
        if (m_coroutine != nullptr)
            m_promise->acquire();
    }

    /// @brief
    ///   Move constructor of @c task.
    /// @param other
    ///   The @c task to be moved. @p other will be empty after this movement.
    task(task &&other) noexcept : m_coroutine(other.m_coroutine), m_promise(other.m_promise) {
        other.m_coroutine = nullptr;
        other.m_promise   = nullptr;
    }

    /// @brief
    ///   Decrease the reference count and release resources.
    ~task() {
        if (m_coroutine != nullptr)
            m_promise->release();
    }

    /// @brief
    ///   Copy assignment of @c task. Reference counting is used.
    /// @param other
    ///   The @c task to be copied from.
    /// @return
    ///   Reference to this @c task object.
    auto operator=(const task &other) noexcept -> task & {
        if (this == &other) [[unlikely]]
            return *this;

        if (m_coroutine == other.m_coroutine)
            return *this;

        if (m_coroutine != nullptr)
            m_promise->release();

        m_coroutine = other.m_coroutine;
        m_promise   = other.m_promise;

        if (m_coroutine != nullptr)
            m_promise->acquire();

        return *this;
    }

    /// @brief
    ///   Move assignment of @c task.
    /// @param other
    ///   The @c task to be moved. @p other will be empty after this movement.
    /// @return
    ///   Reference to this @c task object.
    auto operator=(task &&other) noexcept -> task & {
        if (this == &other) [[unlikely]]
            return *this;

        if (m_coroutine != nullptr)
            m_promise->release();

        m_coroutine = other.m_coroutine;
        m_promise   = other.m_promise;

        other.m_coroutine = nullptr;
        other.m_promise   = nullptr;

        return *this;
    }

    /// @brief
    ///   Checks if this task is null.
    /// @retval true
    ///   This task is null.
    /// @retval false
    ///   This task is not null.
    [[nodiscard]]
    auto empty() const noexcept -> bool {
        return m_coroutine == nullptr;
    }

    /// @brief
    ///   Checks if this @c task is completed.
    /// @retval true
    ///   This task is completed.
    /// @retval false
    ///   This task is not completed.
    [[nodiscard]]
    auto done() const noexcept -> bool {
        return m_coroutine.done();
    }

    /// @brief
    ///   Resume this task.
    auto resume() -> void {
        m_coroutine.resume();
    }

    /// @brief
    ///   Get promise of this task.
    /// @return
    ///   Reference to promise of this task.
    [[nodiscard]]
    auto promise() const noexcept -> promise_type & {
        return *m_promise;
    }

    /// @brief
    ///   Detach the coroutine from this task. This task will be empty once detached. Lifetime of
    ///   the detached coroutine should be manually managed.
    /// @return
    ///   Detached coroutine handle of this task.
    [[nodiscard]]
    auto detach() noexcept -> coroutine_handle {
        coroutine_handle result = m_coroutine;
        m_coroutine             = nullptr;
        m_promise               = nullptr;
        return result;
    }

    /// @brief
    ///   C++20 coroutine awaitable API. Suspend the caller coroutine and start this one.
    auto operator co_await() const noexcept -> awaitable_type {
        return awaitable_type(m_coroutine);
    }

private:
    coroutine_handle m_coroutine;
    promise_type *m_promise;
};

/// @class promise
/// @tparam T
///   Type of return value of the owner task.
/// @brief
///   Promise type for corresponding task.
template <class T>
class promise final : public detail::promise_base {
public:
    /// @brief
    ///   Create an empty @c promise object.
    promise() noexcept : detail::promise_base(), m_value() {}

    /// @brief
    ///   @c promise is not copyable.
    promise(const promise &other) = delete;

    /// @brief
    ///   @c promise is not movable.
    promise(promise &&other) = delete;

    /// @brief
    ///   Destroy this promise and release resources.
    ~promise() = default;

    /// @brief
    ///   @c promise is not copyable.
    auto operator=(const promise &other) = delete;

    /// @brief
    ///   @c promise is not movable.
    auto operator=(promise &&other) = delete;

    /// @brief
    ///   For internal usage. C++20 coroutine API. Acquire a new @c task object for this promise.
    [[nodiscard]]
    auto get_return_object() noexcept -> task<T> {
        auto coro              = std::coroutine_handle<promise>::from_promise(*this);
        m_coroutine            = coro;
        m_stack_bottom_promise = this;
        return {coro, *this};
    }

    /// @brief
    ///   For internal usage. C++20 coroutine API. Store the coroutine return value temporarily.
    /// @tparam Arg
    ///   Type of return value of the coroutine. The return type should be able to construct from
    ///   this type.
    /// @param arg
    ///   Reference to the actual return value of the coroutine.
    template <class Arg = T>
        requires(std::is_constructible_v<T, Arg &&>)
    auto return_value(Arg &&arg) noexcept(std::is_nothrow_constructible_v<T, Arg &&>) -> void {
        m_value.emplace(std::forward<Arg>(arg));
    }

    /// @brief
    ///   For internal usage. C++20 coroutine API. Store the coroutine yielded value temporarily.
    /// @tparam Arg
    ///   Type of the return value of the coroutine. The return type should be able to construct
    ///   from this type.
    /// @param arg
    ///   Reference to the actual return value of the coroutine.
    /// @return
    ///   The coroutine should always be suspended after yielding.
    template <class Arg = T>
        requires(std::is_constructible_v<T, Arg &&>)
    auto yield_value(Arg &&arg) noexcept(std::is_nothrow_constructible_v<T, Arg &&>)
        -> detail::promise_base::final_awaitable {
        m_value.emplace(std::forward<Arg>(arg));
        return {};
    }

    /// @brief
    ///   Get result of this coroutine. Exceptions may be thrown if the coroutine is not completed.
    /// @return
    ///   Reference to the return value of this coroutine.
    [[nodiscard]]
    auto result() & -> T & {
        if (m_exception != nullptr) [[unlikely]]
            std::rethrow_exception(m_exception);
        return m_value.value();
    }

    /// @brief
    ///   Get result of this coroutine. Exceptions may be thrown if the coroutine is not completed.
    /// @return
    ///   Reference to the return value of this coroutine.
    [[nodiscard]]
    auto result() const & -> const T & {
        if (m_exception != nullptr) [[unlikely]]
            std::rethrow_exception(m_exception);
        return m_value.value();
    }

    /// @brief
    ///   Get result of this coroutine. Exceptions may be thrown if the coroutine is not completed.
    /// @return
    ///   Rvalue reference to the return value of this coroutine.
    [[nodiscard]]
    auto result() && -> T && {
        if (m_exception != nullptr) [[unlikely]]
            std::rethrow_exception(m_exception);
        return std::move(m_value).value();
    }

    /// @brief
    ///   Get result of this coroutine. Exceptions may be thrown if the coroutine is not completed.
    /// @return
    ///   Rvalue reference to the return value of this coroutine.
    [[nodiscard]]
    auto result() const && -> const T && {
        if (m_exception != nullptr) [[unlikely]]
            std::rethrow_exception(m_exception);
        return std::move(m_value).value();
    }

private:
    std::optional<T> m_value;
};

/// @class promise
/// @brief
///   Promise type for corresponding task.
template <>
class promise<void> final : public detail::promise_base {
public:
    /// @brief
    ///   Create an empty @c promise object.
    promise() noexcept : detail::promise_base() {}

    /// @brief
    ///   @c promise is not copyable.
    promise(const promise &other) = delete;

    /// @brief
    ///   @c promise is not movable.
    promise(promise &&other) = delete;

    /// @brief
    ///   Destroy this promise and release resources.
    ~promise() = default;

    /// @brief
    ///   @c promise is not copyable.
    auto operator=(const promise &other) = delete;

    /// @brief
    ///   @c promise is not movable.
    auto operator=(promise &&other) = delete;

    /// @brief
    ///   For internal usage. C++20 coroutine API. Acquire a new @c task object for this promise.
    [[nodiscard]]
    auto get_return_object() noexcept -> task<void> {
        auto coro              = std::coroutine_handle<promise>::from_promise(*this);
        m_coroutine            = coro;
        m_stack_bottom_promise = this;
        return {coro, *this};
    }

    /// @brief
    ///   For internal usage. C++20 coroutine API. Marks that this coroutine has no return value.
    static constexpr auto return_void() noexcept -> void {}

    /// @brief
    ///   For internal usage. C++20 coroutine API. Allows void tasks to be yielded.
    /// @return
    ///   Yielded coroutines should always be suspended.
    static constexpr auto yield_value() noexcept -> detail::promise_base::final_awaitable {
        return {};
    }

    /// @brief
    ///   Get result of this coroutine. Exceptions may be thrown if the coroutine is not completed.
    auto result() const -> void {
        if (m_exception != nullptr) [[unlikely]]
            std::rethrow_exception(m_exception);
    }
};

/// @class promise
/// @brief
///   Promise type for corresponding task.
template <class T>
class promise<T &> final : public detail::promise_base {
public:
    /// @brief
    ///   Create an empty @c promise object.
    promise() noexcept : detail::promise_base(), m_value() {}

    /// @brief
    ///   @c promise is not copyable.
    promise(const promise &other) = delete;

    /// @brief
    ///   @c promise is not movable.
    promise(promise &&other) = delete;

    /// @brief
    ///   Destroy this promise and release resources.
    ~promise() = default;

    /// @brief
    ///   @c promise is not copyable.
    auto operator=(const promise &other) = delete;

    /// @brief
    ///   @c promise is not movable.
    auto operator=(promise &&other) = delete;

    /// @brief
    ///   For internal usage. C++20 coroutine API. Acquire a new @c task object for this promise.
    [[nodiscard]]
    auto get_return_object() noexcept -> task<T &> {
        auto coro              = std::coroutine_handle<promise>::from_promise(*this);
        m_coroutine            = coro;
        m_stack_bottom_promise = this;
        return {coro, *this};
    }

    /// @brief
    ///   For internal usage. C++20 coroutine standard API. Cache return value of this coroutine.
    /// @param[in] value
    ///   The value returned by coroutine.
    auto return_value(T &value) noexcept -> void {
        m_value = std::addressof(value);
    }

    /// @brief
    ///   For internal usage. C++20 coroutine standard API. Cache yield value of this coroutine.
    /// @param[in] value
    ///   The return yielded by coroutine.
    /// @return
    ///   Yielded coroutines should always be suspended.
    auto yield_value(T &value) noexcept -> detail::promise_base::final_awaitable {
        m_value = std::addressof(value);
        return {};
    }

    /// @brief
    ///   Get result ofthis coroutine. Exceptions may be thrown if the coroutine is not completed.
    /// @return
    ///   The reference returned by coroutine.
    [[nodiscard]]
    auto result() const -> T & {
        if (m_exception != nullptr) [[unlikely]]
            std::rethrow_exception(m_exception);
        return *m_value;
    }

private:
    T *m_value;
};

} // namespace nyaio

namespace nyaio {

/// @class io_context_worker
/// @brief
///   Worker for handling IO events.
class io_context_worker {
public:
    /// @brief
    ///   Create a new worker and initialize @c io_uring.
    /// @throws std::system_error
    ///   Thrown if failed to initialize @c io_uring.
    NYAIO_API io_context_worker();

    /// @brief
    ///   @c io_context_worker is not copyable.
    io_context_worker(const io_context_worker &other) = delete;

    /// @brief
    ///   @c io_context_worker is not movable.
    io_context_worker(io_context_worker &&other) = delete;

    /// @brief
    ///   Destroy the @c io_uring. Worker must be manually stopped before destroying.
    NYAIO_API ~io_context_worker();

    /// @brief
    ///   @c io_context_worker is not copyable.
    auto operator=(const io_context_worker &other) = delete;

    /// @brief
    ///   @c io_context_worker is not movable.
    auto operator=(io_context_worker &&other) = delete;

    /// @brief
    ///   Start this worker and handle IO requests. This method will block this thread. This method
    ///   should not be called in multiple threads at the same time.
    NYAIO_API auto run() noexcept -> void;

    /// @brief
    ///   Send a stop request to this worker. This method does not block current thread.
    NYAIO_API auto stop() noexcept -> void;

    /// @brief
    ///   Checks if this worker is running.
    /// @retval true
    ///   This worker is running.
    /// @retval false
    ///   This worker is not running.
    [[nodiscard]]
    auto is_running() const noexcept -> bool {
        return m_is_running.load(std::memory_order_relaxed);
    }

    /// @brief
    ///   For internal usage. Get poller for this worker.
    /// @return
    ///   Reference to the @c io_uring.
    [[nodiscard]]
    auto poller() const noexcept -> detail::io_uring & {
        return m_ring;
    }

    /// @brief
    ///   Schedule a new task in this worker.
    /// @tparam T
    ///   Return type of the task to be scheduled.
    /// @param t
    ///   The task to be scheduled. This worker will take the ownership of the scheduled task @p t.
    template <class T>
    auto schedule(task<T> t) noexcept -> void {
        auto c  = t.detach();
        auto &p = static_cast<detail::promise_base &>(c.promise());
        p.set_worker(this);

        io_uring_sqe *sqe = m_ring.poll_sqe();
        while (sqe == nullptr) [[unlikely]] {
            m_ring.submit();
            sqe = m_ring.poll_sqe();
        }

        sqe->opcode    = IORING_OP_NOP;
        sqe->fd        = -1;
        sqe->user_data = reinterpret_cast<uintptr_t>(&p);

        m_ring.flush_sq();
    }

private:
    std::atomic_bool m_is_running;
    mutable detail::io_uring m_ring;

    // Pad worker to be aligned up with cache line.
    [[maybe_unused]] char m_padding[256 - sizeof(void *) - sizeof(m_ring)];
};

/// @class io_context
/// @brief
///   Context for asynchronous IO operations. A static thread pool is used.
class io_context {
public:
    /// @brief
    ///   Create a new IO context and initialize workers. Number of workers will be automatically
    ///   set due to number of CPU virtual cores.
    /// @throws std::system_error
    ///   Thrown if failed to initialize @c io_uring for workers.
    NYAIO_API io_context();

    /// @brief
    ///   Create a new IO context and initialize workers.
    /// @param count
    ///   Expected number of workers to be created. Pass 0 to detect number of workers
    ///   automatically.
    /// @throws std::system_error
    ///   Thrown if failed to initialize @c io_uring for workers.
    NYAIO_API explicit io_context(size_t count);

    /// @brief
    ///   @c io_context is not copyable.
    io_context(const io_context &other) = delete;

    /// @brief
    ///   @c io_context is not movable.
    io_context(io_context &&other) = delete;

    /// @brief
    ///   Stop all workers and destroy this context.
    /// @warning
    ///   There may be memory leaks if there are pending I/O tasks scheduled in asynchronize I/O
    ///   multiplexer when stopping. This is due to internal implementation of the worker class.
    ///   This may be fixed in the future.
    NYAIO_API ~io_context();

    /// @brief
    ///   @c io_context is not copyable.
    auto operator=(const io_context &other) = delete;

    /// @brief
    ///   @c io_context is not movable.
    auto operator=(io_context &&other) = delete;

    /// @brief
    ///   Start all worker threads. This method returns immediately once workers are started.
    NYAIO_API auto start() noexcept -> void;

    /// @brief
    ///   Start all worker threads. This method blocks current thread until all worker threads are
    ///   completed.
    NYAIO_API auto run() noexcept -> void;

    /// @brief
    ///   Stop all workers. This method will send a stop request and return immediately. Workers may
    ///   not be stopped immediately.
    NYAIO_API auto stop() noexcept -> void;

    /// @brief
    ///   Get number of workers in this context.
    /// @return
    ///   Number of workers in this context.
    [[nodiscard]]
    auto size() const noexcept -> size_t {
        return m_size;
    }

    /// @brief
    ///   Schedule a new task in this context. Round-Robin is used here to choose which worker to be
    ///   used.
    /// @tparam T
    ///   Return type of the task to be executed.
    /// @param t
    ///   The task to be scheduled.
    template <class T>
    auto schedule(task<T> t) const noexcept -> void {
        auto next = m_next.fetch_add(1, std::memory_order_relaxed) % m_size;
        m_workers[next].schedule<T>(std::move(t));
    }

    /// @brief
    ///   Dispatch tasks to all workers.
    /// @tparam Func
    ///   Type of the dispatcher function. The function should return a task.
    /// @tparam Args
    ///   Types of arguments to be passed to the dispatcher function.
    /// @param func
    ///   The dispatcher function to generate tasks. This may be called multiple times.
    /// @param args
    ///   Arguments to be passed to the dispatcher function. Please notice that the dispatcher
    ///   function may be called multiple times. So be very careful to pass rvalue-reference
    ///   arguments.
    template <class Func, class... Args>
        requires(std::is_invocable_v<Func, Args && ...>)
    auto dispatch(Func &&func, Args &&...args) const -> void {
        for (size_t i = 0; i < m_size; ++i)
            m_workers[i].schedule(func(std::forward<Args>(args)...));
    }

private:
    size_t m_size;
    std::unique_ptr<io_context_worker[]> m_workers;
    std::unique_ptr<std::jthread[]> m_threads;
    mutable std::atomic_size_t m_next;
};

} // namespace nyaio

namespace nyaio {

/// @class yield_awaitable
/// @brief
///   Awaitable object for yielding current coroutine.
class [[nodiscard]] yield_awaitable {
public:
    /// @brief
    ///   Create a new awaitable object for yielding current coroutine.
    constexpr yield_awaitable() noexcept = default;

    /// @brief
    ///   C++20 coroutine API method. Always execute @c await_suspend().
    /// @return
    ///   This function always returns @c false.
    [[nodiscard]]
    static constexpr auto await_ready() noexcept -> bool {
        return false;
    }

    /// @brief
    ///   Prepare for yielding and suspend this coroutine.
    /// @tparam Promise
    ///   Type of promise of the coroutine to be suspended.
    /// @param coro
    ///   Coroutine handle of the coroutine to be yielded.
    template <class Promise>
        requires(std::is_base_of_v<detail::promise_base, Promise>)
    auto await_suspend(std::coroutine_handle<Promise> coro) noexcept -> void {
        auto &p      = static_cast<detail::promise_base &>(coro.promise());
        auto *worker = static_cast<io_context_worker *>(p.worker());
        auto &ring   = worker->poller();

        io_uring_sqe *sqe = ring.poll_sqe();
        while (sqe == nullptr) [[unlikely]] {
            ring.submit();
            sqe = ring.poll_sqe();
        }

        sqe->opcode    = IORING_OP_NOP;
        sqe->fd        = -1;
        sqe->user_data = reinterpret_cast<uintptr_t>(&p);

        ring.flush_sq();
    }

    /// @brief
    ///   Resume this coroutine from yielding. Do nothing.
    static constexpr auto await_resume() noexcept -> void {}
};

/// @class schedule_awaitable
/// @brief
///   Awaitable object for scheduling a new task in current worker.
template <class T>
class [[nodiscard]] schedule_awaitable {
public:
    /// @brief
    ///   Create a new awaitable for scheduling a new task.
    /// @param t
    ///   The task to be scheduled.
    schedule_awaitable(task<T> &&t) noexcept : m_task(std::move(t)) {}

    /// @brief
    ///   C++20 coroutine API method. Always execute @c await_suspend().
    /// @return
    ///   This function always returns @c false.
    [[nodiscard]]
    static constexpr auto await_ready() noexcept -> bool {
        return false;
    }

    /// @brief
    ///   Get worker of this coroutine and schedule the new task.
    /// @tparam Promise
    ///   Type of promise of this coroutine.
    /// @param coro
    ///   Coroutine handle of this coroutine.
    /// @return
    ///   This method always returns @c false.
    template <class Promise>
        requires(std::is_base_of_v<detail::promise_base, Promise>)
    auto await_suspend(std::coroutine_handle<Promise> coro) noexcept -> bool {
        auto &p      = static_cast<detail::promise_base &>(coro.promise());
        auto *worker = static_cast<io_context_worker *>(p.worker());

        worker->schedule(std::move(m_task));
        return false;
    }

    /// @brief
    ///   Resume this coroutine from timeout. Do nothing.
    static constexpr auto await_resume() noexcept -> void {}

private:
    task<T> m_task;
};

/// @class timeout_awaitable
/// @brief
///   Awaitable object for timeout event. This awaitable suspends current coroutine for the
///   specified time.
class [[nodiscard]] timeout_awaitable {
public:
    /// @brief
    ///   Create a new awaitable object for timeout event.
    /// @tparam Rep
    ///   Type representation of duration type. See @c std::chrono::duration for details.
    /// @tparam Period
    ///   Ratio type that is used to measure how to do conversion between different duration types.
    ///   See @c std::chrono::duration for details.
    /// @param duration
    ///   Timeout duration. Ratios less than nanoseconds are not allowed.
    template <class Rep, class Period>
        requires(std::ratio_greater_equal_v<std::nano, Period>)
    explicit timeout_awaitable(std::chrono::duration<Rep, Period> duration) noexcept : m_time() {
        auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
        m_time.tv_sec    = nanoseconds / 1000000000ULL;
        m_time.tv_nsec   = nanoseconds % 1000000000ULL;
    }

    /// @brief
    ///   C++20 coroutine API method. Always execute @c await_suspend().
    /// @return
    ///   This function always returns @c false.
    [[nodiscard]]
    static constexpr auto await_ready() noexcept -> bool {
        return false;
    }

    /// @brief
    ///   Prepare for timeout and suspend this coroutine.
    /// @tparam Promise
    ///   Type of promise of the coroutine to be suspended.
    /// @param coro
    ///   Coroutine handle of the coroutine to be suspended.
    template <class Promise>
        requires(std::is_base_of_v<detail::promise_base, Promise>)
    auto await_suspend(std::coroutine_handle<Promise> coro) noexcept -> void {
        auto &p      = static_cast<detail::promise_base &>(coro.promise());
        auto *worker = static_cast<io_context_worker *>(p.worker());
        auto &ring   = worker->poller();

        io_uring_sqe *sqe = ring.poll_sqe();
        while (sqe == nullptr) [[unlikely]] {
            ring.submit();
            sqe = ring.poll_sqe();
        }

        sqe->opcode    = IORING_OP_TIMEOUT;
        sqe->fd        = -1;
        sqe->addr      = reinterpret_cast<uintptr_t>(&m_time);
        sqe->len       = sizeof(m_time);
        sqe->user_data = reinterpret_cast<uintptr_t>(&p);

        ring.flush_sq();
    }

    /// @brief
    ///   Resume this coroutine from timeout. Do nothing.
    static constexpr auto await_resume() noexcept -> void {}

private:
    __kernel_timespec m_time;
};

/// @class read_awaitable
/// @brief
///   Awaitable object for async read operation.
class [[nodiscard]] read_awaitable {
public:
    /// @brief
    ///   Create a new @c read_awaitable for async read operation.
    /// @param file
    ///   File descriptor to be read from.
    /// @param[out] buffer
    ///   Pointer to start of the buffer to store the read data.
    /// @param size
    ///   Expected size in byte of data to be read.
    /// @param offset
    ///   Offset in byte of the file to start reading. Pass -1 to read from current file pointer.
    ///   For file descriptors that do not support random access, this value should be 0.
    read_awaitable(int file, void *buffer, uint32_t size, uint64_t offset) noexcept
        : m_promise(), m_file(file), m_size(size), m_buffer(buffer), m_offset(offset) {}

    /// @brief
    ///   C++20 coroutine API method. Always execute @c await_suspend().
    /// @return
    ///   This function always returns @c false.
    [[nodiscard]]
    static constexpr auto await_ready() noexcept -> bool {
        return false;
    }

    /// @brief
    ///   Prepare for async read operation and suspend the coroutine.
    /// @tparam Promise
    ///   Promise type of the coroutine to be suspended.
    /// @param coro
    ///   Coroutine handle of the coroutine to be suspended.
    template <class Promise>
        requires(std::is_base_of_v<detail::promise_base, Promise>)
    auto await_suspend(std::coroutine_handle<Promise> coro) noexcept -> void {
        m_promise    = std::addressof(coro.promise());
        auto *worker = static_cast<io_context_worker *>(m_promise->worker());
        auto &ring   = worker->poller();

        io_uring_sqe *sqe = ring.poll_sqe();
        while (sqe == nullptr) [[unlikely]] {
            ring.submit();
            sqe = ring.poll_sqe();
        }

        sqe->opcode    = IORING_OP_READ;
        sqe->fd        = m_file;
        sqe->off       = m_offset;
        sqe->addr      = reinterpret_cast<uintptr_t>(m_buffer);
        sqe->len       = m_size;
        sqe->user_data = reinterpret_cast<uintptr_t>(m_promise);

        ring.flush_sq();
    }

    /// @brief
    ///   Resume the coroutine and get result of the async read operation.
    /// @return
    ///   An @c std::expected object that contains actual bytes read from the file. @c std::errc is
    ///   returned as error code if the read operation failed.
    [[nodiscard]]
    auto await_resume() const noexcept -> std::expected<uint32_t, std::errc> {
        int ret = m_promise->io_uring_result();
        if (ret < 0) [[unlikely]]
            return std::unexpected(static_cast<std::errc>(-ret));
        return static_cast<uint32_t>(ret);
    }

private:
    detail::promise_base *m_promise;
    int m_file;
    uint32_t m_size;
    void *m_buffer;
    uint64_t m_offset;
};

/// @class write_awaitable
/// @brief
///   Awaitable object for async write operation.
class [[nodiscard]] write_awaitable {
public:
    /// @brief
    ///   Create a new @c write_awaitable for async write operation.
    /// @param file
    ///   File descriptor to write to.
    /// @param data
    ///   Pointer to start of the data to be written.
    /// @param size
    ///   Expected size in byte of data to be written.
    /// @param offset
    ///   Offset in byte of the file to start writing. Pass @c std::numeric_limit<uint64_t>::max()
    ///   to write to current file pointer. For files does not support random access, this value
    ///   should be 0.
    write_awaitable(int file, const void *data, uint32_t size, uint64_t offset) noexcept
        : m_promise(), m_file(file), m_size(size), m_data(data), m_offset(offset) {}

    /// @brief
    ///   C++20 coroutine API method. Always execute @c await_suspend().
    /// @return
    ///   This function always returns @c false.
    [[nodiscard]]
    static constexpr auto await_ready() noexcept -> bool {
        return false;
    }

    /// @brief
    ///   Prepare for async write operation and suspend the coroutine.
    /// @tparam Promise
    ///   Promise type of the coroutine to be suspended.
    /// @param coro
    ///   Coroutine handle of the coroutine to be suspended.
    template <class Promise>
        requires(std::is_base_of_v<detail::promise_base, Promise>)
    auto await_suspend(std::coroutine_handle<Promise> coro) noexcept -> void {
        m_promise    = std::addressof(coro.promise());
        auto *worker = static_cast<io_context_worker *>(m_promise->worker());
        auto &ring   = worker->poller();

        io_uring_sqe *sqe = ring.poll_sqe();
        while (sqe == nullptr) [[unlikely]] {
            ring.submit();
            sqe = ring.poll_sqe();
        }

        sqe->opcode    = IORING_OP_WRITE;
        sqe->fd        = m_file;
        sqe->off       = m_offset;
        sqe->addr      = reinterpret_cast<uintptr_t>(m_data);
        sqe->len       = m_size;
        sqe->user_data = reinterpret_cast<uintptr_t>(m_promise);

        ring.flush_sq();
    }

    /// @brief
    ///   Resume the coroutine and get result of the async write operation.
    /// @return
    ///   An @c std::expected object that contains actual bytes written to the file. @c std::errc is
    ///   returned as error code if the write operation failed.
    [[nodiscard]]
    auto await_resume() const noexcept -> std::expected<uint32_t, std::errc> {
        int ret = m_promise->io_uring_result();
        if (ret < 0) [[unlikely]]
            return std::unexpected(static_cast<std::errc>(-ret));
        return static_cast<uint32_t>(ret);
    }

private:
    detail::promise_base *m_promise;
    int m_file;
    uint32_t m_size;
    const void *m_data;
    uint64_t m_offset;
};

/// @class recv_awaitable
/// @brief
///   Awaitable object for async recv operation.
class [[nodiscard]] recv_awaitable {
public:
    /// @brief
    ///   Create a new @c recv_awaitable for async recv operation.
    /// @param socket
    ///   The socket to receive data from.
    /// @param[out] buffer
    ///   Pointer to start of buffer to store data received from the socket.
    /// @param size
    ///   Maximum available size in byte of @c buffer.
    /// @param flags
    ///   Flags for this async recv operation. See linux manual for @c recv for details.
    recv_awaitable(int socket, void *buffer, uint32_t size, int flags) noexcept
        : m_promise(), m_socket(socket), m_size(size), m_buffer(buffer), m_flags(flags) {}

    /// @brief
    ///   C++20 coroutine API method. Always execute @c await_suspend().
    /// @return
    ///   This function always returns @c false.
    [[nodiscard]]
    static constexpr auto await_ready() noexcept -> bool {
        return false;
    }

    /// @brief
    ///   Prepare for async recv operation and suspend the coroutine.
    /// @tparam Promise
    ///   Promise type of the coroutine to be suspended.
    /// @param coro
    ///   Coroutine handle of the coroutine to be suspended.
    template <class Promise>
        requires(std::is_base_of_v<detail::promise_base, Promise>)
    auto await_suspend(std::coroutine_handle<Promise> coro) noexcept -> void {
        m_promise    = std::addressof(coro.promise());
        auto *worker = static_cast<io_context_worker *>(m_promise->worker());
        auto &ring   = worker->poller();

        io_uring_sqe *sqe = ring.poll_sqe();
        while (sqe == nullptr) [[unlikely]] {
            ring.submit();
            sqe = ring.poll_sqe();
        }

        sqe->opcode    = IORING_OP_RECV;
        sqe->fd        = m_socket;
        sqe->addr      = reinterpret_cast<uintptr_t>(m_buffer);
        sqe->len       = m_size;
        sqe->msg_flags = static_cast<uint32_t>(m_flags);
        sqe->user_data = reinterpret_cast<uintptr_t>(m_promise);

        ring.flush_sq();
    }

    /// @brief
    ///   Resume the coroutine and get result of the async recv operation.
    /// @return
    ///   An @c std::expected object that contains actual bytes received from the socket.
    ///   @c std::errc is returned as error code if the write operation failed.
    [[nodiscard]]
    auto await_resume() const noexcept -> std::expected<uint32_t, std::errc> {
        int ret = m_promise->io_uring_result();
        if (ret < 0) [[unlikely]]
            return std::unexpected(static_cast<std::errc>(-ret));
        return static_cast<uint32_t>(ret);
    }

private:
    detail::promise_base *m_promise;
    int m_socket;
    uint32_t m_size;
    void *m_buffer;
    int m_flags;
};

/// @class send_awaitable
/// @brief
///   Awaitable object for async send operation.
class [[nodiscard]] send_awaitable {
public:
    /// @brief
    ///   Create a new @c send_awaitable for async send operation.
    /// @param socket
    ///   The socket to send data to.
    /// @param data
    ///   Pointer to start of data to be sent.
    /// @param size
    ///   Expected size in byte of data to be sent.
    /// @param flags
    ///   Flags for this async send operation. See linux manual for @c send for details.
    send_awaitable(int socket, const void *data, uint32_t size, int flags) noexcept
        : m_promise(), m_socket(socket), m_size(size), m_data(data), m_flags(flags) {}

    /// @brief
    ///   C++20 coroutine API method. Always execute @c await_suspend().
    /// @return
    ///   This function always returns @c false.
    [[nodiscard]]
    static constexpr auto await_ready() noexcept -> bool {
        return false;
    }

    /// @brief
    ///   Prepare for async send operation and suspend the coroutine.
    /// @tparam Promise
    ///   Promise type of the coroutine to be suspended.
    /// @param coro
    ///   Coroutine handle of the coroutine to be suspended.
    template <class Promise>
        requires(std::is_base_of_v<detail::promise_base, Promise>)
    auto await_suspend(std::coroutine_handle<Promise> coro) noexcept -> void {
        m_promise    = std::addressof(coro.promise());
        auto *worker = static_cast<io_context_worker *>(m_promise->worker());
        auto &ring   = worker->poller();

        io_uring_sqe *sqe = ring.poll_sqe();
        while (sqe == nullptr) [[unlikely]] {
            ring.submit();
            sqe = ring.poll_sqe();
        }

        sqe->opcode    = IORING_OP_SEND;
        sqe->fd        = m_socket;
        sqe->addr      = reinterpret_cast<uintptr_t>(m_data);
        sqe->len       = m_size;
        sqe->msg_flags = static_cast<uint32_t>(m_flags);
        sqe->user_data = reinterpret_cast<uintptr_t>(m_promise);

        ring.flush_sq();
    }

    /// @brief
    ///   Resume the coroutine and get result of the async send operation.
    /// @return
    ///   An @c std::expected object that contains actual bytes sent to the socket. @c std::errc is
    ///   returned as error code if the send operation failed.
    [[nodiscard]]
    auto await_resume() const noexcept -> std::expected<uint32_t, std::errc> {
        int ret = m_promise->io_uring_result();
        if (ret < 0) [[unlikely]]
            return std::unexpected(static_cast<std::errc>(-ret));
        return static_cast<uint32_t>(ret);
    }

private:
    detail::promise_base *m_promise;
    int m_socket;
    uint32_t m_size;
    const void *m_data;
    int m_flags;
};

/// @class connect_awaitable
/// @brief
///   Awaitable object for async connect operation.
class [[nodiscard]] connect_awaitable {
public:
    /// @brief
    ///   Create a new @c connect_awaitable object for async connect operation.
    /// @param socket
    ///   The socket to connect to peer address.
    /// @param addr
    ///   Pointer to the @c sockaddr object that contains the peer address.
    /// @param addrlen
    ///   Size in byte of the @c sockaddr object.
    connect_awaitable(int socket, const sockaddr *addr, socklen_t addrlen) noexcept
        : m_promise(), m_socket(socket), m_addr(addr), m_addrlen(addrlen) {}

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
    /// @tparam Promise
    ///   Promise type of the coroutine to be suspended.
    /// @param coro
    ///   Coroutine handle of the coroutine to be suspended.
    template <class Promise>
        requires(std::is_base_of_v<detail::promise_base, Promise>)
    auto await_suspend(std::coroutine_handle<Promise> coro) noexcept -> void {
        m_promise    = std::addressof(coro.promise());
        auto *worker = static_cast<io_context_worker *>(m_promise->worker());
        auto &ring   = worker->poller();

        io_uring_sqe *sqe = ring.poll_sqe();
        while (sqe == nullptr) [[unlikely]] {
            ring.submit();
            sqe = ring.poll_sqe();
        }

        sqe->opcode    = IORING_OP_CONNECT;
        sqe->fd        = m_socket;
        sqe->addr      = reinterpret_cast<uintptr_t>(m_addr);
        sqe->len       = m_addrlen;
        sqe->user_data = reinterpret_cast<uintptr_t>(m_promise);

        ring.flush_sq();
    }

    /// @brief
    ///   Resume the coroutine and get result of the async connect operation.
    /// @return
    ///   An empty @c std::expected object if succeeded. @c std::errc is returned as error code if
    ///   the connect operation failed.
    [[nodiscard]]
    auto await_resume() const noexcept -> std::errc {
        int ret = m_promise->io_uring_result();
        if (ret < 0) [[unlikely]]
            return static_cast<std::errc>(-ret);
        return {};
    }

private:
    detail::promise_base *m_promise;
    int m_socket;
    const sockaddr *m_addr;
    socklen_t m_addrlen;
};

} // namespace nyaio
