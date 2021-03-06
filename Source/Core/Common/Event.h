// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Multithreaded event class. This allows waiting in a thread for an event to
// be triggered in another thread. While waiting, the CPU will be available for
// other tasks.
// * Set(): triggers the event and wakes up the waiting thread.
// * Wait(): waits for the event to be triggered.
// * Reset(): tries to reset the event before the waiting thread sees it was
//            triggered. Usually a bad idea.

#pragma once

#ifdef _WIN32
#include <concrt.h>
#endif

#include <chrono>
#include <condition_variable>
#include <mutex>

#include "Common/Flag.h"

namespace Common
{
// Windows uses a specific implementation because std::condition_variable has
// terrible performance for this kind of workload with MSVC++ 2013.
#if (!defined(_WIN32)) || (defined(_MSC_VER) && _MSC_VER > 1800)
class Event final
{
public:
  void Set()
  {
    if (m_flag.TestAndSet())
    {
      std::lock_guard<std::mutex> lk(m_mutex);
      m_condvar.notify_one();
    }
  }

  void Wait()
  {
    if (m_flag.TestAndClear())
      return;

    std::unique_lock<std::mutex> lk(m_mutex);
    m_condvar.wait(lk, [&] { return m_flag.TestAndClear(); });
  }

  template <class Rep, class Period>
  bool WaitFor(const std::chrono::duration<Rep, Period>& rel_time)
  {
    if (m_flag.TestAndClear())
      return true;

    std::unique_lock<std::mutex> lk(m_mutex);
    bool signaled = m_condvar.wait_for(lk, rel_time, [&] { return m_flag.TestAndClear(); });

    return signaled;
  }

  void Reset()
  {
    // no other action required, since wait loops on
    // the predicate and any lingering signal will get
    // cleared on the first iteration
    m_flag.Clear();
  }

private:
  Flag m_flag;
  std::condition_variable m_condvar;
  std::mutex m_mutex;
};
#else
class Event final
{
public:
  void Set() { m_event.set(); }
  void Wait()
  {
    m_event.wait();
    m_event.reset();
  }

  template <class Rep, class Period>
  bool WaitFor(const std::chrono::duration<Rep, Period>& rel_time)
  {
    bool signaled =
        m_event.wait(
            (u32)std::chrono::duration_cast<std::chrono::milliseconds>(rel_time).count()) == 0;
    m_event.reset();
    return signaled;
  }

  void Reset() { m_event.reset(); }
private:
  concurrency::event m_event;
};
#endif

}  // namespace Common
