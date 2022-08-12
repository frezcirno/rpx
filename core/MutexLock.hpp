#ifndef __MUTEXLOCK_HPP__
#define __MUTEXLOCK_HPP__

#include "Utils.hpp"
#include <pthread.h>

class MutexLock : noncopyable {
public:
  MutexLock() { pthread_mutex_init(&mutex, NULL); }
  ~MutexLock() { pthread_mutex_destroy(&mutex); }
  void lock() { pthread_mutex_lock(&mutex); }
  void unlock() { pthread_mutex_unlock(&mutex); }
  pthread_mutex_t *getMutex() { return &mutex; }

private:
  pthread_mutex_t mutex;
};

class MutexGuard : noncopyable {
private:
  MutexLock &mutex;

public:
  MutexGuard(MutexLock &mutex) : mutex(mutex) { mutex.lock(); }
  ~MutexGuard() { mutex.unlock(); }
};

#endif