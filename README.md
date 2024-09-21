# nyaio

![license](https://img.shields.io/github/license/longhao-li/nyaio)
![tests](https://img.shields.io/github/actions/workflow/status/longhao-li/nyaio/unit-test.yml?label=tests)
![codecov](https://img.shields.io/codecov/c/github/longhao-li/nyaio/main)

`nyaio` is a general purpose asynchronous IO framework based on `io_uring` and C++20 coroutine, which aims to provide a flexible and efficient way to handle asynchronous file IO, network communications and more.

- Easy-to-use: `nyaio` provides easy-to-use and intuitive async/blocking IO APIs.
- No dependencies: `nyaio` does not rely on third-party library.
- No macro: `nyaio` does not use any macro, except `NYAIO_API` which is used to export shared library symbols.
- Completely lock free: `nyaio` does not use any mutex.
- Minimal system calls: Use `io_uring` SQ poll and very little use of system calls.
- No global state: `nyaio` does not use global variables.

## Requirement

- Linux kernel version >= 6.6
- GCC >= 12 or Clang >= 18
- CMake >= 3.16

Tested on Debian Trixie (currently testing) and Ubuntu 24.04.

## Getting Started

See [CMakeLists.txt](./CMakeLists.txt) for build options.

### Basic Usage

`nyaio::Task<T>` is used for general-purpose coroutine. Coroutines or generators should always return `nyaio::Task<T>` to support async operations. Here is a minimal example:

```cpp
// Task<> is the same as Task<void>
auto hello() noexcept -> Task<> {
    std::cout << "Hello, world!\n";
    co_return;
}

// This method simply returns a integer.
auto returnInt() noexcept -> Task<int> {
    co_return 0;
}

auto main() -> int {
    // Coroutines need a context to be scheduled.
    // IoContext works as a static thread pool.
    IoContext ctx;

    ctx.schedule(hello());
    ctx.schedule(returnInt());

    // Block and wait. Usually, this method could be considered as a noreturn function.
    ctx.run();
}
```

Use `schedule()` to schedule a new task to run concurrently with current task. The scheduled task is actually running in the same worker and therefore mutex is not necessary:

```cpp
auto foo() -> Task<> {
    // do something.
}

auto bar() -> Task<> {
    // schedule never suspends current coroutine and there is no mutex lock operation.
    co_await schedule(foo());
}
```

Usually you can `co_await` tasks one by one to wait for a series of tasks to be completed. But if you really need the tasks working concurrently, you can also use `waitAll()`. Please note that compared to `co_await` operation, `waitAll()` has greater overhead:

```cpp
auto task1() -> Task<std::string> {
    // do something
}

auto task2() -> Task<std::errc> {
    // do something
}

auto task3() -> Task<int> {
    // do something
}

auto root() -> Task<> {
    // waitAll is not implemented via calling co_await one by one. The following 3 tasks work concurrently in current worker.
    auto [str, error, integer] = co_await waitAll(task1(), task2(), task3());
}
```

Currently `nyaio` does not use generator internally, but it is still supported:

```cpp
auto generator() -> Task<int> {
    int value = 0;
    while (true) {
        co_yield value++;
    }
}

auto consumer() -> Task<> {
    auto numbers = generator();
    while (true)
        std::cout << co_await numbers << '\n';
}
```

### Asynchronous IO

Use async file IO:

```cpp
auto fileIo() noexcept -> Task<> {
    File file;
    file.open("test.txt", FileFlag::Read | FileFlag::Write);

    constexpr std::string_view str = "Hello, world!";

    // This method will suspend current coroutine until io_uring returns result of the write operation.
    // Most of the async IO API returns an awaitable object instead of Task to avoid heap memory allocation.
    auto [written, error] = co_await file.writeAsync(str.data(), str.size());

    assert(error == std::errc{});
    assert(written == str.size());

    // Async read from file at offset 0. See code commends for more details.
    char buffer[1024];
    auto result = co_await file.readAsync(buffer, str.size(), 0);

    assert(result.bytes == str.size());
    assert(result.error == std::errc{});
}
```

For TCP and UDP IO examples, please see [example](./example) for details.

## License

BSD 3-Clause License. See [LICENSE](./LICENSE) for details.
