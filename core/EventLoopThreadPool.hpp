#include <atomic>
#include <functional>
#include <iostream>
#include <thread>
#include <vector>

#include "CountDownLatch.hpp"
#include "ThreadPool.hpp"
#include "EventLoop.hpp"

typedef std::function<void(EventLoop*)> ThreadInitCallback;

class EventLoopThreadPool
{
public:
  EventLoopThreadPool(EventLoop* baseLoop, int numThreads, const ThreadInitCallback& init = nullptr)
    : _baseLoop(baseLoop)
    , _pool(numThreads)
    , _cnt(0)
  {
    CountDownLatch initLatch(numThreads);
    _loops.resize(numThreads);
    for (int i = 0; i < numThreads; i++)
      _pool.addTask([&, loop = &_loops[i]] { eventloopTask(loop, initLatch, init); });
    initLatch.wait();
  }

  ~EventLoopThreadPool()
  {
    stopAllTask();
  }

  void stopAllTask()
  {
    for (auto& loop : _loops)
      loop->quit();
  }

  EventLoop* getNextLoop() const
  {
    assert(_baseLoop->isInEventLoop());
    EventLoop* loop = _loops[_cnt++];
    if (_cnt == static_cast<int>(_loops.size()))
      _cnt = 0;
    return loop;
  }

private:
  EventLoop* _baseLoop;
  ThreadPool _pool;
  std::vector<EventLoop*> _loops;
  mutable int _cnt;

  void eventloopTask(EventLoop** ret, CountDownLatch& initLatch, const ThreadInitCallback& cb)
  {
    EventLoop loop;
    *ret = &loop;
    if (cb)
      cb(&loop);
    initLatch.countDown();
    loop.loop();
  }
};