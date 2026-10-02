// Helpers for the few unit tests that need a real desktop window (T-031).
//
// A window needs the desktop to deliver paints and vblank callbacks. Both stop
// when the display is asleep, the session is locked, or the compositor is
// starved, so every wait here has a deadline (hard cap 10 s) and a test checks
// the environment first and SKIPs when it does not deliver.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <thread>

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
#endif

namespace investigate::guitest
{
/** Longest any single wait may last. */
constexpr int maxWaitMs = 10000;

/** Drains this thread's Win32 message queue for `ms` (JUCE_MODAL_LOOPS_PERMITTED is off). */
inline void pump (int ms)
{
    const auto end = juce::Time::getMillisecondCounterHiRes() + (double) std::min (ms, maxWaitMs);

    while (juce::Time::getMillisecondCounterHiRes() < end)
    {
#if JUCE_WINDOWS
        MSG msg;

        while (PeekMessageW (&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage (&msg);
            DispatchMessageW (&msg);
        }
#endif
        juce::Thread::sleep (1);
    }
}

/** Pumps messages until `done()` is true or `timeoutMs` (capped at 10 s) has passed. Returns done(). */
template <typename Pred>
bool pumpUntil (Pred done, int timeoutMs)
{
    const auto end = juce::Time::getMillisecondCounterHiRes() + (double) std::min (timeoutMs, maxWaitMs);

    for (;;)
    {
        pump (1);

        if (done())
            return true;

        if (juce::Time::getMillisecondCounterHiRes() >= end)
            return false;
    }
}

/** Last resort for a native call that never returns (for example a swap-chain
    present against a sleeping display): ends the process with a message after
    `limitMs` instead of hanging until ctest's timeout. */
class Watchdog
{
public:
    explicit Watchdog (const char* testName, int limitMs = 60000)
        : thread ([this, testName, limitMs]
          {
              std::unique_lock<std::mutex> lock (mutex);

              if (! cv.wait_for (lock, std::chrono::milliseconds (limitMs), [this] { return finished; }))
              {
                  std::fprintf (stderr, "WATCHDOG: window test '%s' blocked for %d ms (no desktop paints/vblanks?); aborting.\n",
                                testName, limitMs);
                  std::fflush (stderr);
                  std::_Exit (3);
              }
          })
    {
    }

    ~Watchdog()
    {
        {
            std::lock_guard<std::mutex> lock (mutex);
            finished = true;
        }
        cv.notify_all();
        thread.join();
    }

private:
    std::mutex mutex;
    std::condition_variable cv;
    bool finished = false;
    std::thread thread;
};

/** Counts vblank callbacks for a component on the desktop. */
struct VBlankCounter
{
    explicit VBlankCounter (juce::Component& c) : attachment (&c, [this] { ++count; }) {}

    int count = 0;
    juce::VBlankAttachment attachment;
};
}
