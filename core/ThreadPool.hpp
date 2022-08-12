#ifndef __THREAD_POOL_HPP__
#define __THREAD_POOL_HPP__

#include <assert.h>
#include <functional>
#include <pthread.h>
#include <queue>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "Utils.hpp"
#include "Condition.hpp"
#include "MutexLock.hpp"

typedef std::function<void()> ThreadFunc;
static __thread int t_cachedTid;

static inline int tid()
{
  if (t_cachedTid == 0)
    t_cachedTid = static_cast<int>(::syscall(SYS_gettid));
  return t_cachedTid;
}

class Thread
{
public:
  Thread(ThreadFunc func)
    : func(func)
  {}
  ~Thread();

  void start()
  {
    pthread_create(&thread, NULL, bootstrap, this);
  }
  void join()
  {
    pthread_join(thread, NULL);
  }

private:
  pthread_t thread;
  ThreadFunc func;

  void runInThread()
  {
    func();
  }
  static void* bootstrap(void* arg)
  {
    Thread* thread = static_cast<Thread*>(arg);
    thread->runInThread();
    return NULL;
  }
};

class ThreadPool : noncopyable
{
public:
  ThreadPool(int numThreads, int maxQueueSize = 0)
    : notEmpty(qMutex)
    , notFull(qMutex)
    , isEmpty(qMutex)
    , running(true)
    , maxQueueSize(maxQueueSize)
    , numThreads(numThreads)
  {
    threads.reserve(numThreads);
    for (int i = 0; i < numThreads; i++) {
      Thread* th = new Thread(std::bind(&ThreadPool::runInThread, this));
      threads.push_back(th);
      th->start();
    }
  }
  ~ThreadPool()
  {
    stop();
  }
  void addTask(ThreadFunc func)
  {
    MutexGuard lock(qMutex);
    while (isFullLocked() && running)
      notFull.wait();
    if (!running)
      return;
    taskQueue.push(std::move(func));
    notEmpty.notify();
  }
  void stop()
  {
    {
      MutexGuard lock(qMutex);
      if (running) {
        running = false;
        notEmpty.notifyAll();
        notFull.notifyAll();
      }
    }
    for (auto& thread : threads)
      thread->join();
  }
  void waitTaskClear()
  {
    MutexGuard lock(qMutex);
    if (!taskQueue.empty())
      isEmpty.wait();
  }

private:
  std::vector<Thread*> threads;
  std::queue<ThreadFunc> taskQueue;
  MutexLock qMutex;
  Condition notEmpty, notFull, isEmpty;
  bool running;
  int maxQueueSize;
  int numThreads;

  void runInThread()
  {
    while (running) {
      ThreadFunc task(take());
      if (task)
        task();
    }
  }
  ThreadFunc take()
  {
    MutexGuard lock(qMutex);
    while (taskQueue.empty() && running)
      notEmpty.wait();
    ThreadFunc task;
    if (!taskQueue.empty()) {
      task = taskQueue.front();
      taskQueue.pop();
      if (maxQueueSize > 0)
        notFull.notify();
      if (taskQueue.empty())
        isEmpty.notifyAll();
    }
    return task;
  }
  bool isFullLocked()
  {
    return maxQueueSize > 0 && (int)taskQueue.size() >= maxQueueSize;
  }
};

#endif   // __THREAD_POOL_HPP__