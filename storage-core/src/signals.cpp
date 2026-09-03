#include "signals.hpp"

#include <pthread.h>

#include <string>
#include <utility>

#include <log.hpp>

namespace ledger {

SignalWaiter::SignalWaiter(std::function<void()> on_signal)
    : on_signal_(std::move(on_signal)) {
    sigemptyset(&mask_);
    sigaddset(&mask_, SIGTERM);  // docker stop
    sigaddset(&mask_, SIGINT);   // Ctrl-C in local dev

    // Block in the calling thread. Every thread created after this inherits the
    // mask, which is why this must run before any other thread is started.
    pthread_sigmask(SIG_BLOCK, &mask_, nullptr);

    thread_ = std::thread([this] {
        for (;;) {
            int signal_number = 0;
            const int result = sigwait(&mask_, &signal_number);

            // EINTR is not possible for sigwait, but a defensive retry costs
            // nothing and a busy-loop here would be nasty.
            if (result != 0) continue;

            if (winding_down_ != 0) return;  // woken by the destructor

            // Ordinary thread context: locking and logging are safe here, which
            // is the entire reason for this design.
            log::info("received signal " + std::to_string(signal_number) +
                      ", shutting down");
            if (on_signal_) on_signal_();
            return;
        }
    });
}

SignalWaiter::~SignalWaiter() {
    if (!thread_.joinable()) return;

    winding_down_ = 1;
    // Wake the thread if no signal ever arrived. Directed at that thread
    // specifically, and since SIGTERM is blocked everywhere it can only be
    // consumed by the one sitting in sigwait.
    pthread_kill(thread_.native_handle(), SIGTERM);
    thread_.join();
}

}  // namespace ledger
