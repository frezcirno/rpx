#include <atomic>
#include <functional>
#include <iostream>
#include <thread>
#include <vector>

#include "CountDownLatch.hpp"
#include "ThreadPool.hpp"
#include "EventLoop.hpp"


class EventLoopThreadPool
{
public:
  EventLoopThreadPool(EventLoop* baseLoop, int numThreads)
    : _baseLoop(baseLoop)
    , _pool(numThreads)
    , _cnt(0)
  {
    _loops.resize(numThreads);
    for (int i = 0; i < numThreads; i++) {
      // one eventloop per thread
      _pool.addTask(std::bind(&EventLoopThreadPool::eventloopTask, this, &_loops[i]));
    }
  }
  ~EventLoopThreadPool() {}

  EventLoop* getNextLoop()
  {
    EventLoop* loop = _loops[_cnt++];
    if (_cnt == static_cast<int>(_loops.size()))
      _cnt = 0;
    return loop;
  }


private:
  EventLoop* _baseLoop;
  ThreadPool _pool;
  std::vector<EventLoop*> _loops;
  int _cnt;

  void eventloopTask(EventLoop** ret)
  {
    EventLoop loop;
    *ret = &loop;
    loop.loop();
  }
};