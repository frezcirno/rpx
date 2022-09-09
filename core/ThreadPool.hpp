#ifndef __THREAD_POOL_HPP__
#define __THREAD_POOL_HPP__

#include <assert.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <functional>
#include <queue>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include "Utils.hpp"

typedef std::function<void()> ThreadFunc;

class ThreadPool : noncopyable
{
public:
  ThreadPool(int numThreads, int maxQueueSize = 0)
    : running(true)
    , maxQueueSize(maxQueueSize)
    , numThreads(numThreads)
  {
    threads.reserve(numThreads);
    for (int i = 0; i < numThreads; i++) {
      threads.push_back(std::thread(&ThreadPool::runInThread, this));
    }
  }
  ~ThreadPool()
  {
    if (running)
      stop();
  }
  void addTask(ThreadFunc func)
  {
    std::unique_lock lock(qMutex);
    while (isFullLocked() && running)
      notFull.wait(lock);
    if (!running)
      return;
    taskQueue.push(std::move(func));
    notEmpty.notify_one();
  }

  __attribute__((no_sanitize_thread)) void stop()
  {
    {
      std::lock_guard lock(qMutex);
      if (running) {
        running = false;
        notEmpty.notify_all();
        notFull.notify_all();
      }
    }
    for (auto& thread : threads) {
      if (thread.joinable())
        thread.join();
    }
  }
  void waitTaskClear()
  {
    std::unique_lock lock(qMutex);
    if (!taskQueue.empty())
      isEmpty.wait(lock);
  }

private:
  std::vector<std::thread> threads;
  std::queue<ThreadFunc> taskQueue;
  std::mutex qMutex;
  std::condition_variable notEmpty, notFull, isEmpty;
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
    std::unique_lock lock(qMutex);
    while (taskQueue.empty() && running)
      notEmpty.wait(lock);
    ThreadFunc task;
    if (!taskQueue.empty()) {
      task = taskQueue.front();
      taskQueue.pop();
      if (maxQueueSize > 0)
        notFull.notify_one();
      if (taskQueue.empty())
        isEmpty.notify_all();
    }
    return task;
  }
  bool isFullLocked()
  {
    return maxQueueSize > 0 && (int)taskQueue.size() >= maxQueueSize;
  }
};

#endif   // __THREAD_POOL_HPP__