#pragma once

#include <atomic>
#include <coroutine>
#include <exception>
#include <memory>
#include <optional>
#include <thread>
#include <tuple>

#include <linux/io_uring.h>
#include <sys/socket.h>

namespace nyaio {

/// @class Task
/// @tparam T
///   Return type of this coroutine.
/// @brief
///   @c Task is a wrapper of coroutine that can be awaited.
template <class T = void>
class Task;

/// @class IoContextWorker
/// @brief
///   Worker for handling IO events.
class IoContextWorker {
public:
    using Self = IoContextWorker;

    /// @brief
    ///   Create a new worker and initialize @c io_uring. Linux kernel version 6.0 and above is
    ///   required.
    /// @throws std::system_error
    ///   Thrown if failed to initialize @c io_uring.
    NYAIO_API IoContextWorker();

    /// @brief
    ///   @c IoContextWorker is not copyable.
    IoContextWorker(const Self &other) = delete;

    /// @brief
    ///   @c IoContextWorker is not movable.
    IoContextWorker(Self &&other) = delete;

    /// @brief
    ///   Destroy this worker and @c io_uring. Current worker must be stopped before destruction.
    NYAIO_API ~IoContextWorker();

    /// @brief
    ///   @c IoContextWorker is not copyable.
    auto operator=(const Self &other) = delete;

    /// @brief
    ///   @c IoContextWorker is not copyable.
    auto operator=(Self &&other) = delete;

    /// @brief
    ///   Start this worker and handle IO requests. This method will block this thread. This method
    ///   should not be called in multiple threads at the same time.
    NYAIO_API auto run() noexcept -> void;

    /// @brief
    ///   Request this worker to stop. This method only sets a flag and does not block current
    ///   thread. This worker may take some time to stop.
    auto stop() noexcept -> void {
        if (!m_isRunning.load(std::memory_order_relaxed)) [[unlikely]]
            return;
        m_shouldStop.store(true, std::memory_order_relaxed);
    }

    /// @brief
    ///   Checks if this worker is running.
    /// @retval true
    ///   This worker is running.
    /// @retval false
    ///   This worker is not running.
    [[nodiscard]]
    auto isRunning() const noexcept -> bool {
        return m_isRunning.load(std::memory_order_relaxed);
    }

    /// @brief
    ///   Schedule a new task in this worker.
    /// @note
    ///   This method is not concurrent safe, but it is safe to schedule tasks before starting this
    ///   worker. Only limited number of tasks can be scheduled before starting worker, but usually
    ///   you do not need to worry about this.
    /// @tparam T
    ///   Return type of the task to be scheduled.
    /// @param task
    ///   The task to be scheduled. This worker will take the ownership of the scheduled task @p
    ///   task.
    template <class T>
    auto schedule(Task<T> task) noexcept -> void;

    /// @brief
    ///   Try to acquire a new @c io_uring submission queue entry.
    /// @return
    ///   Pointer to a submission queue entry if succeeded. All fields of the submission queue entry
    ///   will be initialized to 0 if succeeded to acquire one. Return @c nullptr if there is no
    ///   available submission queue entry currently.
    [[nodiscard]]
    auto pollSubmissionQueueEntry() noexcept -> io_uring_sqe * {
        auto &sq = m_sq;

        unsigned head = sq.head->load(std::memory_order_acquire);
        unsigned next = sq.sqeTail + 1;

        if (next - head <= sq.sqeCount) [[likely]] {
            auto *sqe  = sq.sqes + (sq.sqeTail & sq.mask);
            sq.sqeTail = next;

            __builtin_memset(sqe, 0, sizeof(io_uring_sqe));
            return sqe;
        }

        return nullptr;
    }

    /// @brief
    ///   Flush the submission queue so that @c io_uring can see new submission queue entries. This
    ///   method does not trigger system call.
    auto flushSubmissionQueue() noexcept -> void {
        auto &sq = m_sq;
        if (sq.sqeHead != sq.sqeTail) [[likely]] {
            sq.sqeHead = sq.sqeTail;
            sq.tail->store(sq.sqeTail, std::memory_order_release);
        }
    }

    /// @brief
    ///   Try to get a completion queue entry from the completion queue immediately.
    /// @return
    ///   Pointer to a new completion queue entry if succeeded. Return @c nullptr if there is no
    ///   completion queue entry available.
    [[nodiscard]]
    auto pollCompletionQueueEntry() noexcept -> io_uring_cqe * {
        auto &cq = m_cq;

        unsigned tail = cq.tail->load(std::memory_order_acquire);
        unsigned head = cq.head->load(std::memory_order_relaxed);

        if (head == tail)
            return nullptr;

        auto *cqe = cq.cqes + (head & cq.mask);
        return cqe;
    }

    /// @brief
    ///   Flush the submission queue and tell the kernel to handle the new submission entries.
    NYAIO_API auto submit() noexcept -> void;

    /// @brief
    ///   Marks that @p count @c io_uring_cqes are consumed and could be overridden.
    /// @param count
    ///   Number of completion queue entries to be consumed.
    auto consumeCompletionQueueEntries(unsigned count) noexcept -> void {
        m_cq.head->fetch_add(count, std::memory_order_release);
    }

private:
    std::atomic_bool m_isRunning;
    std::atomic_bool m_shouldStop;

    int m_ring;
    std::uint32_t m_flags;
    std::uint32_t m_features;

    struct {
        void *mappedData;
        std::size_t mappedSize;

        io_uring_sqe *sqes;
        std::size_t sqeMappedSize;

        std::uint32_t sqeHead;
        std::uint32_t sqeTail;
        std::uint32_t sqeCount;
        std::uint32_t mask;

        std::atomic<unsigned> *head;
        std::atomic<unsigned> *tail;
        std::atomic<unsigned> *flags;
    } m_sq;

    struct {
        void *mappedData;
        std::size_t mappedSize;

        io_uring_cqe *cqes;

        std::uint32_t cqeCount;
        std::uint32_t mask;

        std::atomic<unsigned> *head;
        std::atomic<unsigned> *tail;
        std::atomic<unsigned> *flags;
    } m_cq;
};

/// @class IoContext
/// @brief
///   Context for asynchronous IO operations. A static thread pool is used.
class IoContext {
public:
    using Self = IoContext;

    /// @brief
    ///   Create a new IO context and initialize workers. Number of workers will be automatically
    ///   set due to number of CPU virtual cores.
    /// @throws std::system_error
    ///   Thrown if failed to initialize @c io_uring for workers.
    NYAIO_API IoContext();

    /// @brief
    ///   Create a new IO context and initialize workers.
    /// @param count
    ///   Expected number of workers to be created. Pass 0 to detect number of workers
    ///   automatically.
    /// @throws std::system_error
    ///   Thrown if failed to initialize @c io_uring for workers.
    NYAIO_API explicit IoContext(std::size_t count);

    /// @brief
    ///   @c IoContext is not copyable.
    IoContext(const Self &other) = delete;

    /// @brief
    ///   @c IoContext is not movable.
    IoContext(Self &&other) = delete;

    /// @brief
    ///   Stop all workers and destroy this context.
    /// @warning
    ///   There may be memory leaks if there are pending I/O tasks scheduled in asynchronize I/O
    ///   multiplexer when stopping. This is due to internal implementation of the worker class.
    NYAIO_API ~IoContext();

    /// @brief
    ///   @c IoContext is not copyable.
    auto operator=(const Self &other) = delete;

    /// @brief
    ///   @c IoContext is not movable.
    auto operator=(Self &&other) = delete;

    /// @brief
    ///   Start all worker threads. This method returns immediately once workers are started.
    auto start() noexcept -> void {
        for (std::size_t i = 0; i < m_numWorkers; ++i)
            m_threads[i] = std::jthread([i, this]() { m_workers[i].run(); });
    }

    /// @brief
    ///   Start all worker threads. This method blocks current thread until all worker threads are
    ///   completed.
    auto run() noexcept -> void {
        start();
        for (std::size_t i = 0; i < m_numWorkers; ++i)
            m_threads[i].join();
    }

    /// @brief
    ///   Stop all workers. This method will send a stop request and return immediately. Workers may
    ///   not be stopped immediately.
    auto stop() noexcept -> void {
        for (std::size_t i = 0; i < m_numWorkers; ++i)
            m_workers[i].stop();
    }

    /// @brief
    ///   Get number of workers in this context.
    /// @return
    ///   Number of workers in this context.
    [[nodiscard]]
    auto size() const noexcept -> std::size_t {
        return m_numWorkers;
    }

    /// @brief
    ///   Schedule a new task in this context. Round-Robin is used here to choose which worker to be
    ///   used.
    /// @tparam T
    ///   Return type of the task to be executed.
    /// @param task
    ///   The task to be scheduled.
    template <class T>
    auto schedule(Task<T> task) noexcept -> void;

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
    auto dispatch(Func &&func, Args &&...args) -> void;

private:
    std::size_t m_numWorkers;
    mutable std::atomic_size_t m_next;
    std::unique_ptr<IoContextWorker[]> m_workers;
    std::unique_ptr<std::jthread[]> m_threads;
};

} // namespace nyaio

namespace nyaio {
namespace detail {

/// @class PromiseBase
/// @brief
///   Base class of coroutine promises.
class PromiseBase {
public:
    using Self = PromiseBase;

    /// @struct FinalAwaitable
    /// @brief
    ///   For internal usage. Final awaitable object for tasks. See C++20 coroutine documents for
    ///   details.
    struct FinalAwaitable {
        using Self = FinalAwaitable;

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
        /// @tparam T
        ///   Promise type of the coroutine to be finalized.
        /// @param coro
        ///   The coroutine to be finalized.
        /// @return
        ///   The caller coroutine to be resumed. A noop coroutine is returned if the finalized
        ///   coroutine is the coroutine stack bottom.
        template <class T>
            requires(std::is_base_of_v<PromiseBase, T>)
        static auto await_suspend(std::coroutine_handle<T> coro) noexcept
            -> std::coroutine_handle<> {
            auto &p = static_cast<PromiseBase &>(coro.promise());
            if (p.m_callerPromise == nullptr)
                return std::noop_coroutine();

            // The caller coroutine should be resumed.
            if (p.m_callerPromise->notify() == 1)
                return p.m_callerPromise->m_coroutine;

            return std::noop_coroutine();
        }

        /// @brief
        ///   For internal usage. C++20 coroutine API. Do nothing.
        static constexpr auto await_resume() noexcept -> void {}
    };

public:
    PromiseBase() noexcept
        : ioFlags(), ioResult(), worker(), m_isCancelled(), m_referenceCount(1), m_waitingTasks(),
          m_coroutine(), m_callerPromise(), m_stackBottomPromise(), m_exception() {}

    /// @brief
    ///   Promise is not copyable.
    PromiseBase(const PromiseBase &other) = delete;

    /// @brief
    ///   Promise is not movable.
    PromiseBase(PromiseBase &&other) = delete;

    /// @brief
    ///   Destroy this promise and release resources.
    ~PromiseBase() = default;

    /// @brief
    ///   Promise is not copyable.
    auto operator=(const PromiseBase &other) = delete;

    /// @brief
    ///   Promise is not movable.
    auto operator=(PromiseBase &&other) = delete;

    /// @brief
    ///   For internal usage. C++20 coroutine API. Tasks should not execute immediately on created.
    /// @return
    ///   Returns a @c std::suspend_always object.
    [[nodiscard]]
    static constexpr auto initial_suspend() noexcept -> std::suspend_always {
        return {};
    }

    /// @brief
    ///   For internal usage. C++20 coroutine API. Return @c FinalAwaitable to maintain the
    ///   coroutine stack and context. See @c FinalAwaitable for more details.
    /// @return
    ///   Returns a @c FinalAwaitable object.
    [[nodiscard]]
    static constexpr auto final_suspend() noexcept -> FinalAwaitable {
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
        m_referenceCount.fetch_add(1, std::memory_order_relaxed);
    }

    /// @brief
    ///   Decrease reference count for this coroutine and destroy it if necessary.
    auto release() noexcept -> void {
        int count = m_referenceCount.fetch_sub(1, std::memory_order_relaxed);
        if (count == 1)
            m_coroutine.destroy();
    }

    /// @brief
    ///   Get current reference counting for this coroutine.
    /// @return
    ///   Current reference counting for this coroutine.
    [[nodiscard]]
    auto referenceCount() const noexcept -> std::size_t {
        return static_cast<std::size_t>(m_referenceCount.load(std::memory_order_relaxed));
    }

    /// @brief
    ///   Marks that this coroutine should wait for a new task to be done.
    /// @param count
    ///   Number of tasks to be waited.
    auto wait(std::size_t count = 1) noexcept -> void {
        m_waitingTasks.fetch_add(static_cast<int>(count), std::memory_order_relaxed);
    }

    /// @brief
    ///   Marks that a task that this coroutine is waiting for has been completed.
    /// @return
    ///   Number of waiting tasks before notify. That is, the return value is 1 if this coroutine
    ///   should be resumed.
    auto notify() noexcept -> std::size_t {
        return static_cast<std::size_t>(m_waitingTasks.fetch_sub(1, std::memory_order_relaxed));
    }

    /// @brief
    ///   Get number of tasks that this coroutine is waiting for.
    /// @return
    ///   Number of tasks that this coroutine is waiting for.
    [[nodiscard]]
    auto waitingTasks() const noexcept -> std::size_t {
        return static_cast<std::size_t>(m_waitingTasks.load(std::memory_order_relaxed));
    }

    /// @brief
    ///   Marks that this coroutine should be cancelled. Cancelled tasks will not be resumed by
    ///   executors.
    auto cancel() noexcept -> void {
        m_isCancelled.store(true, std::memory_order_relaxed);
    }

    /// @brief
    ///   Checks if this coroutine should be cancelled.
    /// @retval true
    ///   This coroutine is marked as cancelled.
    /// @retval false
    ///   This coroutine is not marked as cancelled.
    [[nodiscard]]
    auto isCancelled() const noexcept -> bool {
        return m_isCancelled.load(std::memory_order_relaxed);
    }

    /// @brief
    ///   Get promise for the coroutine stack bottom coroutine.
    /// @return
    ///   Reference to the coroutine call stack bottom coroutine.
    [[nodiscard]]
    auto stackBottomPromise() const noexcept -> PromiseBase & {
        return *m_stackBottomPromise;
    }

    template <class>
    friend class TaskAwaitable;

public:
    /// @brief
    ///   For internal usage. Used to store @c io_uring result.
    std::uint32_t ioFlags;

    /// @brief
    ///   For internal usage. Used to store @c io_uring result.
    int ioResult;

    /// @brief
    ///   For internal usage. Worker for handling IO events.
    IoContextWorker *worker;

protected:
    std::atomic_bool m_isCancelled;
    std::atomic_int m_referenceCount;
    std::atomic_int m_waitingTasks;

    std::coroutine_handle<> m_coroutine;
    PromiseBase *m_callerPromise;
    PromiseBase *m_stackBottomPromise;

    std::exception_ptr m_exception;
};

/// @class Promise
/// @tparam T
///   Type of return value of the owner task.
/// @brief
///   Promise type for corresponding task.
template <class T>
class Promise;

/// @class TaskAwaitable
/// @tparam T
///   Return type of the coroutine.
/// @brief
///   Awaitable class for @c Task.
template <class T>
class TaskAwaitable {
public:
    using Self = TaskAwaitable;

    using value_type       = T;
    using promise_type     = Promise<value_type>;
    using coroutine_handle = std::coroutine_handle<promise_type>;

    /// @brief
    ///   For internal usage. Create a @c task_awaitable object for the specified @c task to be
    ///   awaited.
    explicit TaskAwaitable(coroutine_handle coro) noexcept : m_coroutine(coro) {}

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
    /// @tparam U
    ///   Return type of the caller coroutine.
    /// @param caller
    ///   Caller coroutine to be suspended.
    /// @return
    ///   Coroutine handle of the coroutine to be resumed.
    template <class U>
        requires(std::is_base_of_v<PromiseBase, U>)
    auto await_suspend(std::coroutine_handle<U> caller) noexcept -> std::coroutine_handle<> {
        auto &p  = static_cast<PromiseBase &>(m_coroutine.promise());
        auto &cp = static_cast<PromiseBase &>(caller.promise());

        p.m_callerPromise      = &cp;
        p.m_stackBottomPromise = cp.m_stackBottomPromise;
        p.worker               = cp.worker;

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

/// @class Task
/// @tparam T
///   Return type of this coroutine.
/// @brief
///   @c Task is a wrapper of coroutine that can be awaited.
template <class T>
class Task {
public:
    using Self = Task;

    using value_type       = T;
    using promise_type     = detail::Promise<value_type>;
    using coroutine_handle = std::coroutine_handle<promise_type>;
    using awaitable_type   = detail::TaskAwaitable<value_type>;

    /// @brief
    ///   Create an empty task. Empty task cannot be executed.
    Task() noexcept : m_coroutine() {}

    /// @brief
    ///   Wrap the specified coroutine as a @c Task object.
    /// @param coro
    ///   Coroutine handle of this task.
    explicit Task(coroutine_handle coro) noexcept : m_coroutine(coro) {}

    /// @brief
    ///   Copy constructor of @c task. Reference counting is used.
    /// @param other
    ///   The @c Task to be copied.
    Task(const Self &other) noexcept : m_coroutine(other.m_coroutine) {
        if (m_coroutine != nullptr)
            m_coroutine.promise().acquire();
    }

    /// @brief
    ///   Move constructor of @c task.
    /// @param other
    ///   The @c task to be moved. @p other will be empty after this movement.
    Task(Self &&other) noexcept : m_coroutine(other.m_coroutine) {
        other.m_coroutine = nullptr;
    }

    /// @brief
    ///   Decrease the reference count and release resources if necessary.
    ~Task() {
        if (m_coroutine != nullptr)
            m_coroutine.promise().release();
    }

    /// @brief
    ///   Copy assignment of @c Task. Reference counting is used.
    /// @param other
    ///   The @c Task to be copied from.
    /// @return
    ///   Reference to this @c Task object.
    auto operator=(const Self &other) noexcept -> Self & {
        if (m_coroutine == other.m_coroutine) [[unlikely]]
            return *this;

        if (m_coroutine != nullptr)
            m_coroutine.promise().release();

        m_coroutine = other.m_coroutine;
        if (m_coroutine != nullptr)
            m_coroutine.promise().acquire();

        return *this;
    }

    /// @brief
    ///   Move assignment of @c Task.
    /// @param other
    ///   The @c Task to be moved. @p other will be empty after this movement.
    /// @return
    ///   Reference to this @c Task object.
    auto operator=(Self &&other) noexcept -> Self & {
        if (this == &other) [[unlikely]]
            return *this;

        if (m_coroutine != nullptr)
            m_coroutine.promise().release();

        m_coroutine       = other.m_coroutine;
        other.m_coroutine = nullptr;

        return *this;
    }

    /// @brief
    ///   Checks if this @c Task is null.
    /// @retval true
    ///   This @c Task is null.
    /// @retval false
    ///   This @c Task is not null.
    [[nodiscard]]
    auto isNull() const noexcept -> bool {
        return m_coroutine == nullptr;
    }

    /// @brief
    ///   Checks if this @c Task is completed.
    /// @retval true
    ///   This @c Task is completed.
    /// @retval false
    ///   This @c Task is not completed.
    [[nodiscard]]
    auto isCompleted() const noexcept -> bool {
        return m_coroutine != nullptr && m_coroutine.done();
    }

    /// @brief
    ///   Resume this task. Make sure that this task is not completed and is not null.
    auto resume() noexcept -> void {
        m_coroutine.resume();
    }

    /// @brief
    ///   Detach the coroutine from this task. This task will be empty once detached. Lifetime of
    ///   the detached coroutine should be manually managed.
    /// @return
    ///   Detached coroutine handle of this task.
    [[nodiscard]]
    auto detach() noexcept -> coroutine_handle {
        auto ret    = m_coroutine;
        m_coroutine = nullptr;
        return ret;
    }

    /// @brief
    ///   Get promise object of this task.
    /// @return
    ///   Reference to the promise object of this task.
    [[nodiscard]]
    auto promise() noexcept -> promise_type & {
        return m_coroutine.promise();
    }

    /// @brief
    ///   C++20 coroutine awaitable API. Suspend the caller coroutine and start this one.
    auto operator co_await() const noexcept -> awaitable_type {
        return awaitable_type(m_coroutine);
    }

private:
    coroutine_handle m_coroutine;
};

namespace detail {

/// @class Promise
/// @tparam T
///   Type of return value of the owner task.
/// @brief
///   Promise type for corresponding task.
template <class T>
class Promise final : public PromiseBase {
public:
    using Super = PromiseBase;
    using Self  = Promise;

    /// @brief
    ///   Create an empty @c Promise object.
    Promise() noexcept : Super(), m_value() {}

    /// @brief
    ///   For internal usage. C++20 coroutine API. Acquire @c Task object for this promise.
    /// @return
    ///   @c Task object for this promise.
    [[nodiscard]]
    auto get_return_object() noexcept -> Task<T> {
        auto coro            = std::coroutine_handle<Self>::from_promise(*this);
        m_coroutine          = coro;
        m_stackBottomPromise = this;
        return Task<T>(coro);
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
        -> FinalAwaitable {
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
        return *m_value;
    }

    /// @brief
    ///   Get result of this coroutine. Exceptions may be thrown if the coroutine is not completed.
    /// @return
    ///   Reference to the return value of this coroutine.
    [[nodiscard]]
    auto result() const & -> const T & {
        if (m_exception != nullptr) [[unlikely]]
            std::rethrow_exception(m_exception);
        return *m_value;
    }

    /// @brief
    ///   Get result of this coroutine. Exceptions may be thrown if the coroutine is not completed.
    /// @return
    ///   Reference to the return value of this coroutine.
    [[nodiscard]]
    auto result() && -> T && {
        if (m_exception != nullptr) [[unlikely]]
            std::rethrow_exception(m_exception);
        return *std::move(m_value);
    }

    /// @brief
    ///   Get result of this coroutine. Exceptions may be thrown if the coroutine is not completed.
    /// @return
    ///   Reference to the return value of this coroutine.
    [[nodiscard]]
    auto result() const && -> const T && {
        if (m_exception != nullptr) [[unlikely]]
            std::rethrow_exception(m_exception);
        return *std::move(m_value);
    }

private:
    std::optional<T> m_value;
};

/// @class Promise
/// @brief
///   Promise type for void task.
template <>
class Promise<void> final : public PromiseBase {
public:
    using Super = PromiseBase;
    using Self  = Promise;

    /// @brief
    ///   Create an empty @c Promise object.
    Promise() noexcept : Super() {}

    /// @brief
    ///   For internal usage. C++20 coroutine API. Acquire @c Task object for this promise.
    /// @return
    ///   @c Task object for this promise.
    [[nodiscard]]
    auto get_return_object() noexcept -> Task<> {
        auto coro            = std::coroutine_handle<Self>::from_promise(*this);
        m_coroutine          = coro;
        m_stackBottomPromise = this;
        return Task<>(coro);
    }

    /// @brief
    ///   For internal usage. C++20 coroutine API. Marks that this coroutine has no return value.
    static constexpr auto return_void() noexcept -> void {}

    /// @brief
    ///   For internal usage. C++20 coroutine API. Allows void tasks to be yielded.
    /// @return
    ///   Yielded coroutines should always be suspended.
    static constexpr auto yield_value() noexcept -> FinalAwaitable {
        return {};
    }

    /// @brief
    ///   Get result of this coroutine. Exceptions may be thrown if the coroutine is not completed.
    auto result() const -> void {
        if (m_exception != nullptr) [[unlikely]]
            std::rethrow_exception(m_exception);
    }
};

/// @class Promise
/// @brief
///   Promise type for corresponding task.
template <class T>
class Promise<T &> final : public PromiseBase {
public:
    using Super = PromiseBase;
    using Self  = Promise;

    /// @brief
    ///   Create an empty @c Promise object.
    Promise() noexcept : Super(), m_value() {}

    /// @brief
    ///   For internal usage. C++20 coroutine API. Acquire @c Task object for this promise.
    /// @return
    ///   @c Task object for this promise.
    [[nodiscard]]
    auto get_return_object() noexcept -> Task<T &> {
        auto coro            = std::coroutine_handle<Self>::from_promise(*this);
        m_coroutine          = coro;
        m_stackBottomPromise = this;
        return Task<T &>(coro);
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
    auto yield_value(T &value) noexcept -> FinalAwaitable {
        m_value = std::addressof(value);
        return {};
    }

    /// @brief
    ///   Get result ofthis coroutine. Exceptions may be thrown if the coroutine is not completed.
    /// @return
    ///   The reference returned by coroutine.
    [[nodiscard]]
    auto result() const noexcept -> T & {
        if (m_exception != nullptr) [[unlikely]]
            std::rethrow_exception(m_exception);
        return *m_value;
    }

private:
    T *m_value;
};

} // namespace detail

/// @brief
///   Schedule a new task in this worker.
/// @note
///   This method is not concurrent safe, but it is safe to schedule tasks before starting this
///   worker. Only limited number of tasks can be scheduled before starting worker, but usually you
///   do not need to worry about this.
/// @tparam T
///   Return type of the task to be scheduled.
/// @param task
///   The task to be scheduled. This worker will take the ownership of the scheduled task @p task.
template <class T>
auto IoContextWorker::schedule(Task<T> task) noexcept -> void {
    auto coro     = task.detach();
    auto &promise = static_cast<detail::PromiseBase &>(coro.promise());

    promise.worker = this;

    io_uring_sqe *sqe = pollSubmissionQueueEntry();
    while (sqe == nullptr) [[unlikely]] {
        submit();
        sqe = pollSubmissionQueueEntry();
    }

    sqe->opcode    = IORING_OP_NOP;
    sqe->fd        = -1;
    sqe->user_data = reinterpret_cast<std::uintptr_t>(&promise);

    flushSubmissionQueue();
}

/// @brief
///   Schedule a new task in this context. Round-Robin is used here to choose which worker to be
///   used.
/// @tparam T
///   Return type of the task to be executed.
/// @param task
///   The task to be scheduled.
template <class T>
auto IoContext::schedule(Task<T> task) noexcept -> void {
    std::size_t next = m_next.fetch_add(1, std::memory_order_relaxed) % size();
    m_workers[next].schedule(std::move(task));
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
///   Arguments to be passed to the dispatcher function. Please notice that the dispatcher function
///   may be called multiple times. So be very careful to pass rvalue-reference arguments.
template <class Func, class... Args>
    requires(std::is_invocable_v<Func, Args && ...>)
auto IoContext::dispatch(Func &&func, Args &&...args) -> void {
    for (std::size_t i = 0; i < size(); ++i)
        m_workers[i].schedule(func(std::forward<Args>(args)...));
}

} // namespace nyaio

namespace nyaio {

/// @struct SystemIoResult
/// @brief
///   Result of system IO operations.
struct SystemIoResult {
    /// @brief
    ///   Number of bytes that is transferred.
    std::uint32_t bytes;

    /// @brief
    ///   Error code of the operation. The error code is @c std::errc{} if succeeded.
    std::errc error;
};

/// @class YieldAwaitable
/// @brief
///   Awaitable object for yielding current coroutine.
class YieldAwaitable {
public:
    using Self = YieldAwaitable;

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
    /// @tparam T
    ///   Type of promise of the coroutine to be suspended.
    /// @param coro
    ///   Coroutine handle of the coroutine to be yielded.
    template <class T>
        requires(std::is_base_of_v<detail::PromiseBase, T>)
    static auto await_suspend(std::coroutine_handle<T> coro) noexcept -> void {
        auto &promise = static_cast<detail::PromiseBase &>(coro.promise());
        auto *worker  = promise.worker;

        io_uring_sqe *sqe = worker->pollSubmissionQueueEntry();
        while (sqe == nullptr) [[unlikely]] {
            worker->submit();
            sqe = worker->pollSubmissionQueueEntry();
        }

        sqe->opcode    = IORING_OP_NOP;
        sqe->fd        = -1;
        sqe->user_data = reinterpret_cast<std::uintptr_t>(&promise);

        worker->flushSubmissionQueue();
    }

    /// @brief
    ///   Resume this coroutine from yielding. Do nothing.
    static constexpr auto await_resume() noexcept -> void {}
};

/// @class TimeoutAwaitable
/// @brief
///   Awaitable object for timeout event. This awaitable suspends current coroutine for the
///   specified time.
class TimeoutAwaitable {
public:
    using Self = TimeoutAwaitable;

    /// @brief
    ///   Create a new awaitable object for timeout event.
    /// @tparam Rep
    ///   Type representation of duration type. See @c std::chrono::duration for details.
    /// @tparam Period
    ///   Ratio type that is used to measure how to do conversion between different duration types.
    ///   See @c std::chrono::duration for details.
    /// @param duration
    ///   Timeout duration. Ratios greater than nanoseconds are not allowed.
    template <class Rep, class Period>
        requires(std::ratio_less_equal_v<std::nano, Period>)
    TimeoutAwaitable(std::chrono::duration<Rep, Period> duration) noexcept : m_timeout() {
        auto nano = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
        if (nano <= 0)
            return;

        m_timeout.tv_sec  = static_cast<std::uint64_t>(nano) / 1000000000ULL;
        m_timeout.tv_nsec = static_cast<std::uint64_t>(nano) % 1000000000ULL;
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
    ///   Prepare for yielding and suspend this coroutine.
    /// @tparam T
    ///   Type of promise of the coroutine to be suspended.
    /// @param coro
    ///   Coroutine handle of the coroutine to be yielded.
    template <class T>
        requires(std::is_base_of_v<detail::PromiseBase, T>)
    auto await_suspend(std::coroutine_handle<T> coro) noexcept -> void {
        auto &promise = static_cast<detail::PromiseBase &>(coro.promise());
        auto *worker  = promise.worker;

        io_uring_sqe *sqe = worker->pollSubmissionQueueEntry();
        while (sqe == nullptr) [[unlikely]] {
            worker->submit();
            sqe = worker->pollSubmissionQueueEntry();
        }

        sqe->opcode        = IORING_OP_TIMEOUT;
        sqe->fd            = -1;
        sqe->addr          = reinterpret_cast<std::uintptr_t>(&m_timeout);
        sqe->len           = 1;
        sqe->user_data     = reinterpret_cast<std::uintptr_t>(&promise);
        sqe->timeout_flags = IORING_TIMEOUT_ETIME_SUCCESS;

        worker->flushSubmissionQueue();
    }

    /// @brief
    ///   Resume this coroutine from yielding. Do nothing.
    static constexpr auto await_resume() noexcept -> void {}

private:
    __kernel_timespec m_timeout;
};

/// @class ScheduleAwaitable
/// @brief
///   Awaitable object for scheduling a new task in current worker.
template <class T>
class ScheduleAwaitable {
public:
    using Self = ScheduleAwaitable;

    /// @brief
    ///   Create a new awaitable for scheduling a new task.
    /// @param t
    ///   The task to be scheduled.
    ScheduleAwaitable(Task<T> task) noexcept : m_task(std::move(task)) {}

    /// @brief
    ///   C++20 coroutine API method. Always execute @c await_suspend().
    /// @return
    ///   This function always returns @c false.
    [[nodiscard]]
    static constexpr auto await_ready() noexcept -> bool {
        return false;
    }

    /// @brief
    ///   Prepare for scheduling the given task.
    /// @tparam U
    ///   Type of promise of current coroutine.
    /// @param coro
    ///   Current coroutine handle.
    /// @return
    ///   This method always returns @c false. Scheduling new task does not require suspending.
    template <class U>
        requires(std::is_base_of_v<detail::PromiseBase, U>)
    auto await_suspend(std::coroutine_handle<U> coro) noexcept -> bool {
        auto &promise = static_cast<detail::PromiseBase &>(coro.promise());
        auto *worker  = promise.worker;
        worker->schedule(std::move(m_task));
        return false;
    }

    /// @brief
    ///   Resume this coroutine. Do nothing.
    static constexpr auto await_resume() noexcept -> void {}

private:
    Task<T> m_task;
};

/// @brief
///   Deduction guide for @c ScheduleAwaitable.
template <class T>
ScheduleAwaitable(Task<T>) -> ScheduleAwaitable<T>;

/// @class WaitAllAwaitable
/// @tparam Ts
///   Return types of the awaiting tasks. @c void is not allowed.
/// @brief
///   Awaitable object for waiting an array of tasks.
template <class... Ts>
    requires(sizeof...(Ts) > 0 && !std::disjunction_v<std::is_void<Ts>...>)
class WaitAllAwaitable {
public:
    using Self = WaitAllAwaitable;

    /// @brief
    ///   Create a new awaitable for waiting an array of tasks.
    /// @param tasks
    ///   Tasks to be waited for.
    WaitAllAwaitable(Task<Ts>... tasks) noexcept : m_tasks(std::make_tuple(std::move(tasks)...)) {}

    /// @brief
    ///   C++20 coroutine API method. Always execute @c await_suspend().
    /// @return
    ///   This function always returns @c false.
    [[nodiscard]]
    static constexpr auto await_ready() noexcept -> bool {
        return false;
    }

    /// @brief
    ///   Prepare for waiting for the given tasks.
    /// @tparam U
    ///   Type of promise of current coroutine.
    /// @param coro
    ///   Current coroutine handle.
    template <class U>
        requires(std::is_base_of_v<detail::PromiseBase, U>)
    auto await_suspend(std::coroutine_handle<U> coro) noexcept -> void {
        std::apply(
            [this, coro](auto &...tasks) {
                (tasks.operator co_await().await_suspend(coro), ...);

                // Tasks should not be moved, it will be used by await_resume later.
                (ScheduleAwaitable(tasks).await_suspend(coro), ...);

                // ScheduleAwaitable detached all coroutine handles and therefore the reference
                // count should be decreased. Maybe use some better way to handle this in the
                // future.
                (tasks.promise().release(), ...);
            },
            m_tasks);
    }

    /// @brief
    ///   Resume this coroutine and get results of the awaited tasks.
    /// @return
    ///   A tuple that contains results of the awaited tasks.
    auto await_resume() const -> std::tuple<Ts...> {
        return std::apply(
            [this](auto &...tasks) {
                return std::make_tuple(tasks.operator co_await().await_resume()...);
            },
            m_tasks);
    }

private:
    std::tuple<Task<Ts>...> m_tasks;
};

/// @brief
///   Suspend current task and wait for all of the given tasks to be completed.
/// @tparam T
///   First return type of the tasks to be waited. @c void is not allowed.
/// @tparam Ts
///   Return types of the rest of tasks to be waited. @c void is not allowed.
/// @return
///   An awaitable object that can be used to wait for all of the given tasks. Return values of the
///   awaited tasks may be acquired via structured bindings.
template <class T, class... Ts>
    requires(!std::disjunction_v<std::is_void<T>, std::is_void<Ts>...>)
auto waitAll(Task<T> task, Task<Ts>... tasks) noexcept -> WaitAllAwaitable<T, Ts...> {
    return WaitAllAwaitable<T, Ts...>(std::move(task), std::move(tasks)...);
}

/// @class ReadAwaitable
/// @brief
///   Awaitable object for async read operation.
class ReadAwaitable {
public:
    using Self = ReadAwaitable;

    /// @brief
    ///   Create a new @c ReadAwaitable for async read operation.
    /// @param file
    ///   File descriptor to be read from.
    /// @param[out] buffer
    ///   Pointer to start of the buffer to store the read data.
    /// @param size
    ///   Expected size in byte of data to be read.
    /// @param offset
    ///   Offset in byte of the file to start reading. Pass -1 to read from current file pointer.
    ///   For file descriptors that do not support random access, this value should be 0.
    ReadAwaitable(int file, void *buffer, std::uint32_t size, std::uint64_t offset) noexcept
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

        sqe->opcode    = IORING_OP_READ;
        sqe->fd        = m_file;
        sqe->off       = m_offset;
        sqe->addr      = reinterpret_cast<std::uintptr_t>(m_buffer);
        sqe->len       = m_size;
        sqe->user_data = reinterpret_cast<std::uintptr_t>(&promise);

        worker->flushSubmissionQueue();
    }

    /// @brief
    ///   Resume this coroutine and get result of the async read operation.
    /// @return
    ///   A struct that contains number of bytes read and an error code. The error code is
    ///   @c std::errc{} if succeeded and the number of bytes read is valid.
    auto await_resume() const noexcept -> SystemIoResult {
        if (m_promise->ioResult < 0) [[unlikely]]
            return {0, std::errc{-m_promise->ioResult}};
        return {static_cast<std::uint32_t>(m_promise->ioResult), std::errc{}};
    }

private:
    detail::PromiseBase *m_promise;
    int m_file;
    std::uint32_t m_size;
    void *m_buffer;
    std::uint64_t m_offset;
};

/// @class WriteAwaitable
/// @brief
///   Awaitable object for async write operation.
class WriteAwaitable {
public:
    using Self = WriteAwaitable;

    /// @brief
    ///   Create a new @c WriteAwaitable for async write operation.
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
    WriteAwaitable(int file, const void *data, std::uint32_t size, std::uint64_t offset) noexcept
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

        sqe->opcode    = IORING_OP_WRITE;
        sqe->fd        = m_file;
        sqe->off       = m_offset;
        sqe->addr      = reinterpret_cast<std::uintptr_t>(m_data);
        sqe->len       = m_size;
        sqe->user_data = reinterpret_cast<std::uintptr_t>(&promise);

        worker->flushSubmissionQueue();
    }

    /// @brief
    ///   Resume this coroutine and get result of the async write operation.
    /// @return
    ///   A struct that contains number of bytes written and an error code. The error code is
    ///   @c std::errc{} if succeeded and the number of bytes written is valid.
    auto await_resume() const noexcept -> SystemIoResult {
        if (m_promise->ioResult < 0) [[unlikely]]
            return {0, std::errc{-m_promise->ioResult}};
        return {static_cast<std::uint32_t>(m_promise->ioResult), std::errc{}};
    }

private:
    detail::PromiseBase *m_promise;
    int m_file;
    std::uint32_t m_size;
    const void *m_data;
    std::uint64_t m_offset;
};

/// @class FileSyncAwaitable
/// @brief
///   Awaitable object for file synchronization operation.
class FileSyncAwaitable {
public:
    using Self = FileSyncAwaitable;

    /// @brief
    ///   Create a new @c FileSyncAwaitable for file synchronization operation.
    /// @param file
    ///   File descriptor to be synchronized.
    FileSyncAwaitable(int file) noexcept : m_promise(), m_file(file) {}

    /// @brief
    ///   C++20 coroutine API method. Always execute @c await_suspend().
    /// @return
    ///   This function always returns @c false.
    [[nodiscard]]
    static constexpr auto await_ready() noexcept -> bool {
        return false;
    }

    /// @brief
    ///   Prepare for file synchronize operation and suspend the coroutine.
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

        sqe->opcode    = IORING_OP_FSYNC;
        sqe->fd        = m_file;
        sqe->user_data = reinterpret_cast<std::uintptr_t>(&promise);

        worker->flushSubmissionQueue();
    }

    /// @brief
    ///   Resume this coroutine and get result of the file synchronize operation.
    /// @return
    ///   A system error code that indicates the result of the file synchronize operation. The error
    ///   code is @c std::errc{} if succeeded.
    auto await_resume() const noexcept -> std::errc {
        return std::errc{-m_promise->ioResult};
    }

private:
    detail::PromiseBase *m_promise;
    int m_file;
};

/// @class SendAwaitable
/// @brief
///   Awaitable object for async send operation.
class SendAwaitable {
public:
    using Self = SendAwaitable;

    /// @brief
    ///   Create a new @c SendAwaitable for async send operation.
    /// @param socket
    ///   The socket to send data to.
    /// @param data
    ///   Pointer to start of data to be sent.
    /// @param size
    ///   Expected size in byte of data to be sent.
    /// @param flags
    ///   Flags for this async send operation. See linux manual for @c send for details.
    SendAwaitable(int socket, const void *data, std::uint32_t size, int flags) noexcept
        : m_promise(), m_socket(socket), m_data(data), m_size(size), m_flags(flags) {}

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

        sqe->opcode    = IORING_OP_SEND;
        sqe->fd        = m_socket;
        sqe->addr      = reinterpret_cast<std::uintptr_t>(m_data);
        sqe->len       = m_size;
        sqe->msg_flags = static_cast<std::uint32_t>(m_flags);
        sqe->user_data = reinterpret_cast<std::uintptr_t>(&promise);

        worker->flushSubmissionQueue();
    }

    /// @brief
    ///   Resume this coroutine and get result of the async send operation.
    /// @return
    ///   A struct that contains number of bytes sent and an error code. The error code is
    ///   @c std::errc{} if succeeded and the number of bytes sent is valid.
    [[nodiscard]]
    auto await_resume() const noexcept -> SystemIoResult {
        if (m_promise->ioResult < 0) [[unlikely]]
            return {0, std::errc{-m_promise->ioResult}};
        return {static_cast<std::uint32_t>(m_promise->ioResult), std::errc{}};
    }

private:
    detail::PromiseBase *m_promise;
    int m_socket;
    const void *m_data;
    std::uint32_t m_size;
    int m_flags;
};

/// @class ReceiveAwaitable
/// @brief
///   Awaitable object for async recv operation.
class ReceiveAwaitable {
public:
    using Self = ReceiveAwaitable;

    /// @brief
    ///   Create a new @c ReceiveAwaitable for async recv operation.
    /// @param socket
    ///   The socket to receive data from.
    /// @param[out] buffer
    ///   Pointer to start of buffer to store data received from the socket.
    /// @param size
    ///   Maximum available size in byte of @c buffer.
    /// @param flags
    ///   Flags for this async recv operation. See linux manual for @c recv for details.
    ReceiveAwaitable(int socket, void *buffer, std::uint32_t size, int flags) noexcept
        : m_promise(), m_socket(socket), m_buffer(buffer), m_size(size), m_flags(flags) {}

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

        sqe->opcode    = IORING_OP_RECV;
        sqe->fd        = m_socket;
        sqe->addr      = reinterpret_cast<std::uintptr_t>(m_buffer);
        sqe->len       = m_size;
        sqe->msg_flags = static_cast<std::uint32_t>(m_flags);
        sqe->user_data = reinterpret_cast<std::uintptr_t>(&promise);

        worker->flushSubmissionQueue();
    }

    /// @brief
    ///   Resume this coroutine and get result of the async recv operation.
    /// @return
    ///   A struct that contains number of bytes received and an error code. The error code is
    ///   @c std::errc{} if succeeded and the number of bytes received is valid.
    [[nodiscard]]
    auto await_resume() const noexcept -> SystemIoResult {
        if (m_promise->ioResult < 0) [[unlikely]]
            return {0, std::errc{-m_promise->ioResult}};
        return {static_cast<std::uint32_t>(m_promise->ioResult), std::errc{}};
    }

private:
    detail::PromiseBase *m_promise;
    int m_socket;
    void *m_buffer;
    std::uint32_t m_size;
    int m_flags;
};

/// @class SendToAwaitable
/// @brief
///   Awaitable object for async sendto operation.
class SendToAwaitable {
public:
    using Self = SendToAwaitable;

    /// @brief
    ///   Create a new @c SendToAwaitable for async sendto operation.
    /// @param socket
    ///   The socket to send data to.
    /// @param data
    ///   Pointer to start of data to be sent.
    /// @param size
    ///   Expected size in byte of data to be sent.
    /// @param flags
    ///   Flags for this async send operation. See linux manual for @c sendto for details.
    /// @param address
    ///   Pointer to the address of the receiver.
    /// @param addressLength
    ///   Length of the address of the receiver.
    SendToAwaitable(int socket, const void *data, std::uint32_t size, int flags,
                    const struct sockaddr *address, socklen_t addressLength) noexcept
        : m_promise(), m_socket(socket), m_data(data), m_size(size), m_flags(flags),
          m_address(address), m_addressLength(addressLength) {}

    /// @brief
    ///   C++20 coroutine API method. Always execute @c await_suspend().
    /// @return
    ///   This function always returns @c false.
    [[nodiscard]]
    static constexpr auto await_ready() noexcept -> bool {
        return false;
    }

    /// @brief
    ///   Prepare for async sendto operation and suspend the coroutine.
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

        sqe->opcode    = IORING_OP_SEND;
        sqe->fd        = m_socket;
        sqe->addr2     = reinterpret_cast<std::uintptr_t>(m_address);
        sqe->addr      = reinterpret_cast<std::uintptr_t>(m_data);
        sqe->len       = m_size;
        sqe->msg_flags = static_cast<std::uint32_t>(m_flags);
        sqe->addr_len  = m_addressLength;
        sqe->user_data = reinterpret_cast<std::uintptr_t>(&promise);

        worker->flushSubmissionQueue();
    }

    /// @brief
    ///   Resume this coroutine and get result of the async sendto operation.
    /// @return
    ///   A struct that contains number of bytes sent and an error code. The error code is
    ///   @c std::errc{} if succeeded and the number of bytes sent is valid.
    [[nodiscard]]
    auto await_resume() const noexcept -> SystemIoResult {
        if (m_promise->ioResult < 0) [[unlikely]]
            return {0, std::errc{-m_promise->ioResult}};
        return {static_cast<std::uint32_t>(m_promise->ioResult), std::errc{}};
    }

private:
    detail::PromiseBase *m_promise;
    int m_socket;
    const void *m_data;
    std::uint32_t m_size;
    int m_flags;
    const struct sockaddr *m_address;
    socklen_t m_addressLength;
};

/// @class ReceiveFromAwaitable
/// @brief
///   Awaitable object for async recvfrom operation. @c io_uring does not support @c recvfrom
///   natively. This awaitable is implemented with @c recvmsg.
class ReceiveFromAwaitable {
public:
    using Self = ReceiveFromAwaitable;

    /// @brief
    ///   Create a new @c ReceiveFromAwaitable for async recvfrom operation.
    /// @param socket
    ///   The socket to receive data from.
    /// @param[out] buffer
    ///   Pointer to start of buffer to store data received from the socket.
    /// @param size
    ///   Maximum available size in byte of @p buffer.
    /// @param flags
    ///   Flags for this async recv operation. See linux manual for @c recvfrom for details.
    /// @param[out] address
    ///   Optional. Pointer to the buffer to store the address of the sender. Pass @c nullptr if you
    ///   do not need this information.
    /// @param addressLength
    ///   Optional. Length of the buffer to store the address of the sender. Pass 0 if you do not
    ///   need this information. This value must be 0 if @p address is @c nullptr.
    ReceiveFromAwaitable(int socket, void *buffer, std::uint32_t size, int flags,
                         struct sockaddr *address, socklen_t addressLength) noexcept
        : m_promise(), m_socket(socket), m_flags(flags),
          m_buffer{
              .iov_base = buffer,
              .iov_len  = size,
          },
          m_message{
              .msg_name       = address,
              .msg_namelen    = addressLength,
              .msg_iov        = &m_buffer,
              .msg_iovlen     = 1,
              .msg_control    = nullptr,
              .msg_controllen = 0,
              .msg_flags      = 0,
          } {}

    /// @brief
    ///   C++20 coroutine API method. Always execute @c await_suspend().
    /// @return
    ///   This function always returns @c false.
    [[nodiscard]]
    static constexpr auto await_ready() noexcept -> bool {
        return false;
    }

    /// @brief
    ///   Prepare for async recvfrom operation and suspend the coroutine.
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

        sqe->opcode    = IORING_OP_RECVMSG;
        sqe->fd        = m_socket;
        sqe->addr      = reinterpret_cast<std::uintptr_t>(&m_message);
        sqe->len       = 1;
        sqe->msg_flags = m_flags;
        sqe->user_data = reinterpret_cast<std::uintptr_t>(&promise);

        worker->flushSubmissionQueue();
    }

    /// @brief
    ///   Resume this coroutine and get result of the async recvfrom operation.
    /// @return
    ///   A struct that contains number of bytes received and an error code. The error code is
    ///   @c std::errc{} if succeeded and the number of bytes received is valid.
    [[nodiscard]]
    auto await_resume() const noexcept -> SystemIoResult {
        if (m_promise->ioResult < 0) [[unlikely]]
            return {0, std::errc{-m_promise->ioResult}};
        return {static_cast<std::uint32_t>(m_promise->ioResult), std::errc{}};
    }

private:
    detail::PromiseBase *m_promise;
    int m_socket;
    int m_flags;
    struct iovec m_buffer;
    struct msghdr m_message;
};

/// @class ConnectAwaitable
/// @brief
///   Awaitable object for async connect operation.
class ConnectAwaitable {
public:
    using Self = ConnectAwaitable;

    /// @brief
    ///   Create a new @c ConnectAwaitable object for async connect operation.
    /// @param socket
    ///   The socket to connect to peer address.
    /// @param addr
    ///   Pointer to the @c sockaddr object that contains the peer address.
    /// @param addrlen
    ///   Size in byte of the @c sockaddr object.
    ConnectAwaitable(int socket, const struct sockaddr *address, socklen_t addressLength) noexcept
        : m_promise(), m_socket(socket), m_addressLength(addressLength), m_address(address) {}

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
    auto await_suspend(std::coroutine_handle<T> coro) noexcept -> void {
        auto &promise = static_cast<detail::PromiseBase &>(coro.promise());
        m_promise     = &promise;
        auto *worker  = promise.worker;

        io_uring_sqe *sqe = worker->pollSubmissionQueueEntry();
        while (sqe == nullptr) [[unlikely]] {
            worker->submit();
            sqe = worker->pollSubmissionQueueEntry();
        }

        sqe->opcode    = IORING_OP_CONNECT;
        sqe->fd        = m_socket;
        sqe->addr      = reinterpret_cast<std::uintptr_t>(m_address);
        sqe->off       = m_addressLength;
        sqe->user_data = reinterpret_cast<std::uintptr_t>(m_promise);

        worker->flushSubmissionQueue();
    }

    /// @brief
    ///   Resume this coroutine and get result of the async connect operation.
    /// @return
    ///   An error code that indicates the result of the async connect operation. The error code is
    ///   @c std::errc{} if succeeded.
    [[nodiscard]]
    auto await_resume() const noexcept -> std::errc {
        return std::errc{-m_promise->ioResult};
    }

private:
    detail::PromiseBase *m_promise;
    int m_socket;
    socklen_t m_addressLength;
    const struct sockaddr *m_address;
};

/// @class AcceptAwaitable
/// @brief
///   Awaitable object for async accept operation.
class AcceptAwaitable {
public:
    using Self = AcceptAwaitable;

    /// @struct Result
    /// @brief
    ///   Result of the async accept operation.
    struct Result {
        /// @brief
        ///   The socket of the accepted connection. This field is a invalid value if any error
        ///   occurs.
        std::int32_t socket;

        /// @brief
        ///   Error code of the operation. The error code is @c std::errc{} if succeeded.
        std::errc error;
    };

    /// @brief
    ///   Create a new @c AcceptAwaitable for async accept operation.
    /// @param socket
    ///   The socket to accept incoming connections.
    /// @param[out] address
    ///   Optional. Pointer to the buffer to store the address of the sender if succeeded.
    /// @param addressLength
    ///   Optional. Length of the buffer to store the address of the sender. This value must be 0 if
    ///   @p address is
    ///   @c nullptr.
    /// @param flags
    ///   Flags for this async accept operation. See linux manual for @c accept4 for details.
    AcceptAwaitable(int socket, struct sockaddr *address, socklen_t *addressLength,
                    int flags) noexcept
        : m_promise(), m_socket(socket), m_flags(flags), m_address(address),
          m_addressLength(addressLength) {}

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
        sqe->addr         = reinterpret_cast<std::uintptr_t>(m_address);
        sqe->off          = reinterpret_cast<std::uintptr_t>(m_addressLength);
        sqe->accept_flags = static_cast<std::uint32_t>(m_flags);
        sqe->user_data    = reinterpret_cast<std::uintptr_t>(&promise);

        worker->flushSubmissionQueue();
    }

    /// @brief
    ///   Resume this coroutine and get result of the async accept operation.
    /// @return
    ///   A struct that contains the new accepted socket and an error code. The error code is @c
    ///   std::errc{} if succeeded and the new accepted socket is valid.
    [[nodiscard]]
    auto await_resume() const noexcept -> Result {
        if (m_promise->ioResult < 0) [[unlikely]]
            return {0, std::errc{-m_promise->ioResult}};
        return {m_promise->ioResult, std::errc{}};
    }

private:
    detail::PromiseBase *m_promise;
    int m_socket;
    int m_flags;
    struct sockaddr *m_address;
    socklen_t *m_addressLength;
};

} // namespace nyaio
