#ifndef __EVENTLOOP_HPP__
#define __EVENTLOOP_HPP__

#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <poll.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unordered_map>
#include <vector>
#include <memory>
#include <functional>
#include "Utils.hpp"
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
    poller_cb.state = CHAN_UNSET;   // hard-coded
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
        if (rv == static_cast<int>(_events.size()))
          _events.resize(2 * rv);
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
    : _poller(new Poller(this))
    , running(true)
    , _ownerThreadId(::tid())
    , _wakeupFd(::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC))
    , _wakeupChannel(new Channel(this, _wakeupFd))
  {
    _wakeupChannel->setReadCallback(std::bind(&EventLoop::wakeupRead, this));
    _wakeupChannel->setReadInterest();
  }
  ~EventLoop()
  {
    _wakeupChannel->unsetAllInterest();
    _wakeupChannel->remove();
    ::close(_wakeupFd);
  }

  bool isOwner()
  {
    return _ownerThreadId == ::gettid();
  }

  void loop()
  {
    while (running) {
      _activeChannels.clear();
      _poller->poll(&_activeChannels);
      std::cout << _activeChannels.size() << " events polled" << std::endl;
      for (auto ch : _activeChannels) {
        ch->handleEvent();
      }
      std::vector<Task> tasks;
      _callingPendingTasks = true;
      {
        MutexGuard lock(_mutex);
        tasks.swap(_pendingTask);
      }
      std::cout << "run " << tasks.size() << " pending tasks" << std::endl;
      for (auto task : tasks) {
        task();
      }
      _callingPendingTasks = true;
    }
  }

  void quit()
  {
    running = false;
  }

  void addOrUpdateChannel(Channel* ch)
  {
    _poller->addOrUpdateChannel(ch);
  }

  void removeChannel(Channel* ch)
  {
    _poller->removeChannel(ch);
  }

  void queueInLoop(Task cb)
  {
    {
      MutexGuard lock(_mutex);
      _pendingTask.push_back(std::move(cb));
    }

    if (!isOwner() || _callingPendingTasks) {
      wakeupWakeup();   // raise a new event
    }
  }

  void runInLoop(const Task& cb)
  {
    if (isOwner()) {
      cb();
    } else {
      queueInLoop(std::move(cb));
    }
  }

private:
  std::vector<Channel*> _activeChannels;
  std::unique_ptr<Poller> _poller;
  bool running;
  const pid_t _ownerThreadId;

  int _wakeupFd;
  std::unique_ptr<Channel> _wakeupChannel;

  mutable MutexLock _mutex;
  std::vector<Task> _pendingTask;
  bool _callingPendingTasks;

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

#endif