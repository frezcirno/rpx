#ifndef __CONDITION_HPP__
#define __CONDITION_HPP__

#include "MutexLock.hpp"
#include "Utils.hpp"
#include <pthread.h>

class Condition : noncopyable
{
public:
  Condition(MutexLock& mutex)
    : mutex(mutex)
  {
    pthread_cond_init(&cond, NULL);
  }
  ~Condition()
  {
    pthread_cond_destroy(&cond);
  }
  void wait()
  {
    pthread_cond_wait(&cond, mutex.getMutex());
  }
  void notify()
  {
    pthread_cond_signal(&cond);
  }
  void notifyAll()
  {
    pthread_cond_broadcast(&cond);
  }

private:
  pthread_cond_t cond;
  MutexLock& mutex;
};

#endif