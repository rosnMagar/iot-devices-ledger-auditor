#pragma once
#include <csignal>
#include <functional>
#include <thread>

namespace ledger {

/// Turns SIGTERM/SIGINT into an ordinary function call on an ordinary thread.
///
/// ## Why not a plain signal handler
/// A handler installed with `signal()`/`sigaction()` may only call
/// async-signal-safe functions. It must not lock a mutex, allocate, or log —
/// which rules out everything our shutdown actually does. `httplib::Server::stop()`
/// is not async-signal-safe either.
///
/// So instead: block the signals in every thread, and have one dedicated thread
/// sit in `sigwait()`. When a signal arrives that thread simply *returns* from
/// sigwait in normal thread context, where locking and logging are fine. The
/// signal never runs any code on an interrupted thread's stack.
///
/// ## Ordering requirement
/// Construct this **before** starting any other thread. Threads inherit the
/// signal mask of the thread that created them, and the block has to be in place
/// everywhere — a signal delivered to any thread that has not blocked it takes
/// the default action and kills the process.
class SignalWaiter {
public:
    /// Blocks SIGTERM and SIGINT process-wide and starts the waiting thread.
    /// `on_signal` runs on that thread, once, when either signal arrives.
    explicit SignalWaiter(std::function<void()> on_signal);

    /// Stops the waiting thread and joins it. Safe whether or not a signal ever
    /// arrived — if none did, the thread is woken with a directed signal.
    ~SignalWaiter();

    SignalWaiter(const SignalWaiter&) = delete;
    SignalWaiter& operator=(const SignalWaiter&) = delete;

private:
    std::function<void()> on_signal_;
    sigset_t mask_{};
    std::thread thread_;

    /// Distinguishes "a real signal arrived" from "the destructor woke us to
    /// shut down", so the callback does not fire on an ordinary exit.
    volatile std::sig_atomic_t winding_down_ = 0;
};

}  // namespace ledger
