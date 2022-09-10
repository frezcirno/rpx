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
    // If assert failed, please check if the connector has been stopped
    assert(!_channel);
  }

  void setNewConnectionCallback(NewConnectionCallback cb)
  {
    _newConnectionCallback = std::move(cb);
  }

  /**
   * start() - start the connector
   *
   * Can be called in any thread.
   */
  void start()
  {
    _running = true;
    _loop->queueInLoop([&] {
      if (_running)
        connect();
    });
  }

  /**
   * stop() - stop the connector
   *
   * Can be called in any thread.
   */
  void stop()
  {
    _running = false;
    _loop->queueInLoop([that = shared_from_this()] {
      if (that->_state == CONNECTING) {
        int sockfd = that->unregisterChannel();
        ::close(sockfd);
      }
      if (that->_state == CONNECTING || that->_state == RETRYING)
        that->_channel.reset();
    });
  }

private:
  enum States
  {
    DISCONNECTED = 0,
    CONNECTING,
    RETRYING,
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

  /**
   * restart() - restart the connector
   *
   * Only can be called in io thread.
   */
  void restart()
  {
    assert(_loop->isInEventLoop());
    _retryDelayMs = kInitRetryDelayMs;
    _running = true;
    connect();
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

    _state = CONNECTING;
    _channel.reset(new Channel(_loop, sockfd));
    _channel->setWriteCallback([&] { handleWrite(); });
    _channel->setErrorCallback([&] { handleError(); });
    _channel->setWriteInterest();
  }

  void handleWrite()
  {
    if (_state == CONNECTING) {
      // the work is ended now
      int sockfd = unregisterChannel();
      int err = ::getSocketError(sockfd);
      if (err || ::isSelfConnect(sockfd)) {
        _state = RETRYING;
        retry(sockfd);
      } else {
        _state = CONNECTED;
        // avoid reset in the middle of a loop
        _loop->queueInLoop([&] { _channel.reset(); });
        if (_running)
          _newConnectionCallback(sockfd);
        else
          ::close(sockfd);
      }
    }
  }

  void handleError()
  {
    if (_state == CONNECTING) {
      perror("connect error");
      int sockfd = unregisterChannel();
      int err = ::getSocketError(sockfd);
      _state = RETRYING;
      retry(sockfd);
    }
  }

  int unregisterChannel()
  {
    _channel->unsetAllInterest();
    _channel->remove();
    return _channel->fd();
  }

  void retry(int sockfd)
  {
    ::close(sockfd);
    if (_running) {
      _loop->runAfter(_retryDelayMs / 1000.0, [that = shared_from_this()] {
        if (that->_running)
          that->connect();
      });
      _retryDelayMs = std::min(_retryDelayMs * 2, kMaxRetryDelayMs);
    }
  }
};

#endif
