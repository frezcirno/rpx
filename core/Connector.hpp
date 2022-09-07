#ifndef __CONNECTOR_HPP__
#define __CONNECTOR_HPP__

#include "EventLoop.hpp"
#include "Socket.hpp"

class Connector : noncopyable, public std::enable_shared_from_this<Connector>
{
  friend class TcpClient;

public:
  typedef std::function<void(int sockfd)> NewConnectionCallback;

  Connector(EventLoop* loop, const InetAddress& peerAddr)
    : _loop(loop)
    , _peerAddr(peerAddr)
    , _running(false)
    , _retryDelayMs(kInitRetryDelayMs)
    , _state(DISCONNECTED)
  {}
  ~Connector()
  {
    if (_channel) {
      abort();
    }
  }

  void setNewConnectionCallback(NewConnectionCallback cb)
  {
    _newConnectionCallback = std::move(cb);
  }

  void start()
  {
    _running = true;
    _loop->runInLoop([&] {
      assert(_state == DISCONNECTED);
      if (_running)
        connect();
    });
  }

  void stop()
  {
    _running = false;
    // CHECKME: queue or run?
    _loop->runInLoop([&, that = shared_from_this()] {
      if (_state == CONNECTING) {
        setState(DISCONNECTED);
        int sockfd = destroyChannel();
        ::close(sockfd);
      }
    });
  }

private:
  enum States
  {
    DISCONNECTED = 0,
    CONNECTING,
    CONNECTED
  };
  static constexpr int kInitRetryDelayMs = 500;
  static constexpr int kMaxRetryDelayMs = 30 * 1000;

  EventLoop* _loop;
  InetAddress _peerAddr;
  std::unique_ptr<Channel> _channel;
  bool _running;
  NewConnectionCallback _newConnectionCallback;
  int _retryDelayMs;
  States _state;

  void setState(States state)
  {
    _state = state;
  }

  void restart()
  {
    assert(_loop->isInEventLoop());
    _retryDelayMs = kInitRetryDelayMs;
    setState(DISCONNECTED);
    start();
  }

  void connect()
  {
    int sockfd = ::socket(_peerAddr.family());
    int ret = ::connect(sockfd, _peerAddr);
    if (ret < 0) {
      switch (errno) {
        case 0:
        case EINPROGRESS:
        case EINTR:
        case EISCONN:
          break;

        case EAGAIN:
        case EADDRINUSE:
        case EADDRNOTAVAIL:
        case ECONNREFUSED:
        case ENETUNREACH:
          retry(sockfd);
          return;

        case EACCES:
        case EPERM:
        case EAFNOSUPPORT:
        case EALREADY:
        case EBADF:
        case EFAULT:
        case ENOTSOCK:
        default:
          perror("connect");
          ::close(sockfd);
          return;
      }
    }

    setState(CONNECTING);
    _channel.reset(new Channel(_loop, sockfd));
    _channel->setWriteCallback([&] { handleWrite(); });
    _channel->setErrorCallback([&] { handleError(); });
    _channel->setWriteInterest();
  }

  void handleWrite()
  {
    if (_state == CONNECTING) {
      // the work is ended now
      int sockfd = destroyChannel();
      int err = ::getSocketError(sockfd);
      if (err || ::isSelfConnect(sockfd)) {
        retry(sockfd);
        return;
      }

      setState(CONNECTED);
      if (_running) {
        _newConnectionCallback(sockfd);
      } else {
        ::close(sockfd);
      }
    }
  }

  void handleError()
  {
    if (_state == CONNECTING) {
      int sockfd = destroyChannel();
      int err = ::getSocketError(sockfd);
      perror("connect error");
      retry(sockfd);
    }
  }

  int destroyChannel()
  {
    _channel->unsetAllInterest();
    _channel->remove();
    int sockfd = _channel->fd();
    // Can't reset _channel here as we are in Channel::handleEvent()
    _loop->queueInLoop([&] { _channel.reset(); });
    return sockfd;
  }

  void retry(int sockfd)
  {
    ::close(sockfd);
    setState(DISCONNECTED);
    if (_running) {
      _loop->runAfter(_retryDelayMs / 1000, [that = shared_from_this()] { that->connect(); });
      _retryDelayMs = std::min(_retryDelayMs * 2, kMaxRetryDelayMs);
    }
  }
};

#endif
