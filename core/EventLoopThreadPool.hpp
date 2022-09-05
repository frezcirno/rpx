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
  EventLoopThreadPool(EventLoop* baseLoop, int numThreads,
                      ThreadInitCallback cb = ThreadInitCallback())
    : _baseLoop(baseLoop)
    , _pool(numThreads)
    , _cnt(0)
  {
    _loops.resize(numThreads);
    for (int i = 0; i < numThreads; i++) {
      _ready.store(0);
      // one eventloop per thread
      _pool.addTask([&, i, cb] { eventloopTask(&_loops[i], cb); });
      // wait until the eventloop is ready
      while (!_ready.load())
        continue;
    }
  }
  ~EventLoopThreadPool() {}

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
  std::atomic<int> _ready;
  mutable int _cnt;

  void eventloopTask(EventLoop** ret, ThreadInitCallback cb)
  {
    EventLoop loop;
    *ret = &loop;
    _ready.store(1);
    if (cb)
      cb(&loop);
    loop.loop();
  }
};