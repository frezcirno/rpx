#ifndef __COUNTDOWNLATCH_HPP__
#define __COUNTDOWNLATCH_HPP__

#include <assert.h>
#include <mutex>
#include <condition_variable>
#include "Utils.hpp"

class CountDownLatch : noncopyable
{
public:
  explicit CountDownLatch(int count)
    : count(count)
  {
    assert(count > 0);
  }
  ~CountDownLatch() {}
  void wait()
  {
    std::unique_lock lock(mutex);
    while (count > 0)
      cond.wait(lock);
  }
  void countDown()
  {
    std::lock_guard lock(mutex);
    --count;
    if (count == 0) {
      cond.notify_all();
    }
  }
  int getCount()
  {
    std::lock_guard lock(mutex);
    return count;
  }

private:
  int count;
  std::mutex mutex;
  std::condition_variable cond;
};

#endif