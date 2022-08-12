#ifndef __COUNTDOWNLATCH_HPP__
#define __COUNTDOWNLATCH_HPP__

#include "Condition.hpp"
#include "MutexLock.hpp"
#include "Utils.hpp"
#include <assert.h>

class CountDownLatch : noncopyable
{
public:
  explicit CountDownLatch(int count)
    : count(count)
    , cond(mutex)
  {
    assert(count > 0);
  }
  ~CountDownLatch() {}
  void wait()
  {
    MutexGuard lock(mutex);
    while (count > 0)
      cond.wait();
  }
  void countDown()
  {
    MutexGuard lock(mutex);
    --count;
    if (count == 0) {
      cond.notifyAll();
    }
  }
  int getCount()
  {
    MutexGuard lock(mutex);
    return count;
  }

private:
  int count;
  MutexLock mutex;
  Condition cond;
};

#endif