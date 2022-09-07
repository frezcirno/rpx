#ifndef __EVENTLOOP_HPP__
#define __EVENTLOOP_HPP__

#include <errno.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <unordered_map>
#include <unordered_set>
#include <atomic>
#include <vector>
#include <memory>
#include <map>
#include <functional>
#include <mutex>
#include <condition_variable>
#include "Utils.hpp"
#include "Time.hpp"
#include "ThreadPool.hpp"

#define CHAN_UNSET -1
#define CHAN_SET 1
#define CHAN_DELET 2

class EventLoop;

class Channel : noncopyable
{
public:
  typedef std::function<void()> EventCallback;

public:
  Channel(EventLoop* loop, int fd)
    : _fd(fd)
    , _loop(loop)
    , _tied(false)
    , _interests(0)
    , _events(0)
  {
    poller_cb.state = CHAN_UNSET;   // FIXME: hard-coded
  }
  ~Channel() {}

  int fd() const
  {
    return _fd;
  }

  int interests() const
  {
    return _interests;
  }

  bool hasNoneInterest() const
  {
    return _interests == 0;
  }

  bool hasReadInterest() const
  {
    return _interests & (EPOLLIN | EPOLLPRI);
  }

  bool hasWriteInterest() const
  {
    return _interests & (EPOLLOUT);
  }

  bool hasReadEvent() const
  {
    return _events & (EPOLLIN | EPOLLPRI);
  }

  bool hasWriteEvent() const
  {
    return _events & (EPOLLOUT);
  }

  void setReadInterest()
  {
    _interests |= (EPOLLIN | EPOLLPRI);
    applyInterest();
  }

  void unsetReadInterest()
  {
    _interests &= ~(EPOLLIN | EPOLLPRI);
    applyInterest();
  }

  void setWriteInterest()
  {
    _interests |= EPOLLOUT;
    applyInterest();
  }

  void unsetWriteInterest()
  {
    _interests &= ~EPOLLOUT;
    applyInterest();
  }

  void unsetAllInterest()
  {
    _interests = 0;
    applyInterest();
  }

  void applyInterest();

  void setReadCallback(EventCallback cb)
  {
    readCb = std::move(cb);
  }
  void setWriteCallback(EventCallback cb)
  {
    writeCb = std::move(cb);
  }
  void setCloseCallback(EventCallback cb)
  {
    closeCb = std::move(cb);
  }
  void setErrorCallback(EventCallback cb)
  {
    errorCb = std::move(cb);
  }

  void setEvents(int events)
  {
    _events = events;
  }

  int events() const
  {
    return _events;
  }

  void tie(const std::shared_ptr<void>& obj)
  {
    _tie = obj;
    _tied = true;
  }

  void handleEvent()
  {
    if (_tied) {
      if (_tie.lock())
        handleEventGuarded();
    } else {
      handleEventGuarded();
    }
  }

  void handleEventGuarded()
  {
    if ((_events & EPOLLHUP) && !(_events & EPOLLIN)) {
      if (closeCb)
        closeCb();
    }

    if (_events & EPOLLERR) {
      if (errorCb)
        errorCb();
    }

    if (_events & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)) {
      if (readCb)
        readCb();
    }

    if (_events & EPOLLOUT) {
      if (writeCb)
        writeCb();
    }
  }

  void remove();

  static std::string eventsToString(int events)
  {
    std::string evt;
    if (events & EPOLLIN)
      evt += "EPOLLIN ";
    if (events & EPOLLPRI)
      evt += "EPOLLPRI ";
    if (events & EPOLLOUT)
      evt += "EPOLLOUT ";
    if (events & EPOLLHUP)
      evt += "EPOLLHUP ";
    if (events & EPOLLRDHUP)
      evt += "EPOLLRDHUP ";
    if (events & EPOLLERR)
      evt += "EPOLLERR ";
    if (events & EPOLLONESHOT)
      evt += "EPOLLONESHOT ";
    if (events & EPOLLET)
      evt += "EPOLLET ";
    return evt;
  }

private:
  int _fd;
  int _interests;
  int _events;
  EventLoop* _loop;
  EventCallback readCb, writeCb, closeCb, errorCb;

  std::weak_ptr<void> _tie;
  bool _tied;

public:
  struct
  {
    int state;
  } poller_cb;
};

typedef std::function<void()> TimerCallback;

class TimerQueue
{
  friend EventLoop;
  class Timer
  {
  public:
    Timer(TimerCallback cb, Time when, double interval)
      : _cb(std::move(cb))
      , _when(when)
      , _interval(interval)
      , _repeated(interval > 0.0)
      , _sequence(_id.fetch_add(1) + 1)
    {}
    ~Timer() {}

    void run() const
    {
      _cb();
    }
    Time when() const
    {
      return _when;
    }
    bool isRepeat() const
    {
      return _interval > 0;
    }
    int64_t sequence() const
    {
      return _sequence;
    }
    void restart(Time now)
    {
      assert(_repeated);
      _when = now.offsetBy(_interval);
    }

  private:
    const TimerCallback _cb;
    Time _when;
    const double _interval;   // unit: second
    const bool _repeated;
    const int64_t _sequence;

    static std::atomic<int64_t> _id;
  };

public:
  TimerQueue(EventLoop* loop)
    : _loop(loop)
    , _timerfd(timerfd_create())
    , _timerfdChannel(loop, _timerfd)
    , _callingExpiredTimers(false)
  {
    _timerfdChannel.setReadCallback([&] { handleRead(); });
    _timerfdChannel.setReadInterest();
  }
  ~TimerQueue() {}

  Timer* addTimer(TimerCallback cb, Time when, double interval);

  void addTimerInLoop(Timer* timer);

  bool insert(Timer* timer);

  void cancel(Timer* timerId);

  void cancelInLoop(Timer* timerId);

  typedef std::unordered_set<Timer*> TimerSet;

private:
  EventLoop* _loop;
  int _timerfd;
  Channel _timerfdChannel;
  std::map<Time, TimerSet> _timers;
  TimerSet _activeTimers;
  bool _callingExpiredTimers;
  TimerSet _cancelingTimers;

  void handleRead();

  void resetTimerfd(Time when)
  {
    struct itimerspec newValue;
    struct itimerspec oldValue;
    memset(&newValue, 0, sizeof(newValue));
    memset(&oldValue, 0, sizeof(oldValue));
    Time period = std::max<Time>(100, when - Time::now());
    newValue.it_value = (struct timespec)period;
    int rv = ::timerfd_settime(_timerfd, 0, &newValue, &oldValue);
    if (rv < 0) {
      perror("timerfd_settime");
      abort();
    }
  }

  void consumeTimerfd()
  {
    int64_t howmany;
    ssize_t n = ::read(_timerfd, &howmany, sizeof(howmany));
    if (n != sizeof(howmany)) {
      perror("TimerQueue::handleRead() reads");
      abort();
    }
  }

  static int timerfd_create()
  {
    int rv = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (rv < 0) {
      perror("timerfd_create");
      abort();
    }
    return rv;
  }
};

std::atomic<int64_t> TimerQueue::Timer::_id;

class EventLoop : noncopyable
{
public:
  typedef std::function<void()> Task;

private:
  class Poller : noncopyable
  {
  public:
    Poller(EventLoop* loop)
      : epollfd(::epoll_create(1))
      , _loop(loop)
      , _events(16)   // 16 slots for events
    {
      if (epollfd < 0) {
        perror("epoll_create");
        abort();
      }
    }
    ~Poller()
    {
      ::close(epollfd);
    }

    void poll(std::vector<Channel*>* channels)
    {
      int rv = ::epoll_wait(epollfd, _events.data(), static_cast<int>(_events.size()), -1);
      if (rv < 0) {
        if (errno != EINTR) {
          perror("epoll_wait");
          abort();
        }
      } else if (rv > 0) {
        for (int i = 0; i < rv; ++i) {
          Channel* ch = static_cast<Channel*>(_events[i].data.ptr);
          ch->setEvents(_events[i].events);
          channels->push_back(ch);
        }
        if (rv == static_cast<int>(_events.size())) {
          _events.clear();
          _events.resize(2 * rv);
        }
      }
    }

    void addOrUpdateChannel(Channel* ch)
    {
      int fd = ch->fd();
      const int state = ch->poller_cb.state;
      if (state == CHAN_UNSET || state == CHAN_DELET) {
        if (state == CHAN_UNSET)
          _channels[fd] = ch;
        ch->poller_cb.state = CHAN_SET;
        epollCtl(EPOLL_CTL_ADD, ch);
      } else {
        if (ch->hasNoneInterest()) {
          ch->poller_cb.state = CHAN_DELET;
          epollCtl(EPOLL_CTL_DEL, ch);
        } else
          epollCtl(EPOLL_CTL_MOD, ch);
      }
    }

    void removeChannel(Channel* ch)
    {
      int fd = ch->fd();
      int state = ch->poller_cb.state;
      _channels.erase(fd);
      if (state == CHAN_SET)
        epollCtl(EPOLL_CTL_DEL, ch);
      ch->poller_cb.state = CHAN_UNSET;
    }

  private:
    int epollfd;
    std::unordered_map<int, Channel*> _channels;
    std::vector<struct epoll_event> _events;
    EventLoop* _loop;

    void epollCtl(int operation, Channel* ch)
    {
      struct epoll_event ev;
      ev.events = ch->interests();
      ev.data.ptr = ch;
      int fd = ch->fd();
      int rv = ::epoll_ctl(epollfd, operation, fd, &ev);
      if (rv < 0) {
        perror("epoll_ctl");
        exit(-1);
      }
    }
  };

public:
  EventLoop()
    : _poller(this)
    , running(true)
    , _ownerThreadId(std::this_thread::get_id())
    , _wakeupFd(::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC))
    , _wakeupChannel(this, _wakeupFd)
    , _timerQueue(this)
  {
    _wakeupChannel.setReadCallback([&] { this->wakeupRead(); });
    _wakeupChannel.setReadInterest();
  }
  ~EventLoop()
  {
    _wakeupChannel.unsetAllInterest();
    _wakeupChannel.remove();
    ::close(_wakeupFd);
  }

  bool isInEventLoop()
  {
    return _ownerThreadId == std::this_thread::get_id();
  }

  void loop()
  {
    while (running) {
      _activeChannels.clear();
      _poller.poll(&_activeChannels);
      for (auto ch : _activeChannels)
        ch->handleEvent();
      std::vector<Task> tasks;
      _callingPendingTasks = true;
      {
        std::lock_guard lock(_mutex);
        tasks.swap(_pendingTask);
      }
      for (auto task : tasks)
        task();
      _callingPendingTasks = true;
    }
  }

  void quit()
  {
    running = false;
  }

  void addOrUpdateChannel(Channel* ch)
  {
    _poller.addOrUpdateChannel(ch);
  }

  void removeChannel(Channel* ch)
  {
    _poller.removeChannel(ch);
  }

  void queueInLoop(Task cb)
  {
    {
      std::lock_guard lock(_mutex);
      _pendingTask.push_back(std::move(cb));
    }

    if (!isInEventLoop() || _callingPendingTasks) {
      wakeupWakeup();   // raise a new event
    }
  }

  void runInLoop(Task cb)
  {
    if (isInEventLoop()) {
      // in the loop now
      cb();
    } else {
      queueInLoop(std::move(cb));
    }
  }

  void* runAt(Time time, TimerCallback cb)
  {
    return _timerQueue.addTimer(std::move(cb), time, 0.0);
  }

  void* runAfter(double delayS, TimerCallback cb)
  {
    return runAt(Time::now().offsetBy(delayS), std::move(cb));
  }

  void* runEvery(double intervalS, TimerCallback cb)
  {
    return _timerQueue.addTimer(std::move(cb), Time::now(), intervalS);
  }

  void cancel(void* timerId)
  {
    return _timerQueue.cancel(static_cast<TimerQueue::Timer*>(timerId));
  }

private:
  std::vector<Channel*> _activeChannels;
  Poller _poller;
  bool running;
  const std::thread::id _ownerThreadId;

  int _wakeupFd;
  Channel _wakeupChannel;

  mutable std::mutex _mutex;
  std::vector<Task> _pendingTask;
  bool _callingPendingTasks;

  TimerQueue _timerQueue;

  void wakeupRead()
  {
    uint64_t one = 1;
    ssize_t n = ::read(_wakeupFd, &one, sizeof one);
    assert(n == sizeof one);
  }

  void wakeupWakeup()
  {
    uint64_t one = 1;
    ssize_t n = ::write(_wakeupFd, &one, sizeof one);
    assert(n == sizeof one);
  }
};

inline void Channel::applyInterest()
{
  _loop->addOrUpdateChannel(this);
}

inline void Channel::remove()
{
  assert(hasNoneInterest());
  _loop->removeChannel(this);
}

inline TimerQueue::Timer* TimerQueue::addTimer(TimerCallback cb, Time when, double interval)
{
  Timer* timer = new Timer(std::move(cb), when, interval);
  _loop->runInLoop([&, timer] { addTimerInLoop(timer); });
  return timer;
}

inline void TimerQueue::addTimerInLoop(Timer* timer)
{
  assert(_loop->isInEventLoop());
  bool earliestChanged = insert(timer);
  if (earliestChanged)
    resetTimerfd(timer->when());
}

inline bool TimerQueue::insert(Timer* timer)
{
  assert(_loop->isInEventLoop());
  bool earliestChanged = false;
  Time when = timer->when();
  auto it = _timers.begin();
  if (it == _timers.end() || when < it->first)
    earliestChanged = true;
  _timers[when].insert(timer);
  _activeTimers.insert(timer);
  return earliestChanged;
}

inline void TimerQueue::cancel(Timer* timerId)
{
  _loop->runInLoop([&] { cancelInLoop(timerId); });
}

inline void TimerQueue::cancelInLoop(Timer* timer)
{
  assert(_loop->isInEventLoop());
  auto it = _activeTimers.find(timer);
  if (it != _activeTimers.end()) {
    auto when = timer->when();
    size_t n = _timers[when].erase(timer);
    if (_timers[when].empty())
      _timers.erase(when);
    delete timer;
    _activeTimers.erase(it);
  } else if (_callingExpiredTimers) {
    _cancelingTimers.insert(timer);
  }
}

inline void TimerQueue::handleRead()
{
  assert(_loop->isInEventLoop());
  consumeTimerfd();

  Time now = Time::now();
  auto it = _timers.begin();
  auto end = _timers.upper_bound(now);
  std::vector<Timer*> expired;
  while (it != end) {
    for (auto timer : it->second) {
      expired.push_back(timer);
      _activeTimers.erase(timer);
    }
    it = _timers.erase(it);
  }

  _callingExpiredTimers = true;
  _cancelingTimers.clear();
  // safe to callback outside critical section
  for (const auto& timer : expired)
    timer->run();
  _callingExpiredTimers = false;

  bool earliestChanged = false;
  for (const auto& timer : expired) {
    if (timer->isRepeat() && !_cancelingTimers.count(timer)) {
      timer->restart(now);
      earliestChanged |= insert(timer);
    } else
      delete timer;
  }
  if (earliestChanged)
    resetTimerfd(_timers.begin()->first);
}

#endif
