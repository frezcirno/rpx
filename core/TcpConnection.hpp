#ifndef __TCPCONNECTION_HPP__
#define __TCPCONNECTION_HPP__

#include <assert.h>
#include <any>
#include "Utils.hpp"
#include "Socket.hpp"
#include "StreamBuffer.hpp"
#include "EventLoop.hpp"

class TcpConnection;
typedef std::shared_ptr<TcpConnection> TcpConnectionPtr;
typedef std::function<void(const TcpConnectionPtr&)> TcpCallback;
typedef std::function<void(const TcpConnectionPtr&, StreamBuffer*)> TcpMessageCallback;

class TcpConnection : noncopyable, public std::enable_shared_from_this<TcpConnection>
{
  friend class TcpServer;
  friend class TcpClient;

public:
  TcpConnection(EventLoop* loop, int sockfd, const InetAddress& peerAddr)
    : _loop(loop)
    , _channel(new Channel(loop, sockfd))
    , _peerAddr(peerAddr)
    , _socket(sockfd)
    , _readBuffer(1024)
    , _writeBuffer(1024, 20)
    , _state(CONNECTING)
    , _zc(zlog_get_category("TcpConnection"))
  {
    _channel->setReadCallback([&] { handleRead(); });
    _channel->setWriteCallback([&] { handleWrite(); });
    _channel->setCloseCallback([&] { handleClose(); });
    _channel->setErrorCallback([&] { handleError(); });
    _socket.setKeepAlive(true);
  }
  ~TcpConnection()
  {
    assert(_state == DISCONNECTED);
  }

  EventLoop* getLoop() const
  {
    return _loop;
  }
  int fd()
  {
    return _channel->fd();
  }
  const InetAddress& getPeerAddr() const
  {
    return _peerAddr;
  }

  void setMessageCallback(TcpMessageCallback cb)
  {
    _messageCallback = std::move(cb);
  }
  void setWriteCompleteCallback(TcpCallback cb)
  {
    _writeCompleteCallback = std::move(cb);
  }
  void setCloseCallback(TcpCallback cb)
  {
    _closeCallback = std::move(cb);
  }
  void setErrorCallback(TcpCallback cb)
  {
    _errorCallback = std::move(cb);
  }

  int write(const char* data, size_t len)
  {
    assert(_loop->isInEventLoop());
    ssize_t written;
    size_t remaining = len;
    if (!_channel->hasWriteInterest() && _writeBuffer.empty()) {
      written = ::write(_channel->fd(), data, len);
      if (written < 0) {
        if (errno != EWOULDBLOCK) {
          char _errbuf[100];
          zlog_error(_zc, "write: %s", strerror_r(errno, _errbuf, sizeof(_errbuf)));
          return written;
        }
        written = 0;
      } else {
        remaining -= written;
        if (remaining == 0) {
          if (_writeCompleteCallback)
            _loop->queueInLoop([&] {
              if (_writeCompleteCallback)
                _writeCompleteCallback(shared_from_this());
            });
          return len;
        }
      }
    }

    if (remaining > 0) {
      _writeBuffer.append(data + written, remaining);
      bool close;
      {
        std::lock_guard lock(_stateLock);
        close = (_state == DISCONNECTING || _state == DISCONNECTED);
      }
      // CHECKME: is it atomic?
      if (!close && !_channel->hasWriteInterest())
        _channel->setWriteInterest();
    }
    return len;
  }
  int write(const std::string& data)
  {
    return write(data.data(), data.size());
  }

  /**
   * shutdown() - shutdown write end of the connection
   */
  void shutdown()
  {
    _loop->runInLoop([&] { _socket.shutdownWrite(); });
  }

  /**
   * forceClose() - active close the connection
   */
  void forceClose()
  {
    if (compareExchange(ESTABLISHED, DISCONNECTING)) {
      // will call _closeCallback() in the next loop
      _loop->queueInLoop([that = shared_from_this()] { that->handleClose(); });
    }
  }

  std::any& getUserData()
  {
    return _userData;
  }
  void setUserData(const std::any& userData)
  {
    _userData = userData;
  }

public:
  /**
   * enum StateE - Connection state
   *
   * CONNECTING: Initial state.
   * ESTABLISHED: The connection has been established.
   * DISCONNECTING: Active closing.
   * DISCONNECTED: Closed.
   */
  enum StateE
  {
    CONNECTING = 0,
    ESTABLISHED,
    DISCONNECTING,
    DISCONNECTED,
  };

private:
  EventLoop* _loop;
  std::unique_ptr<Channel> _channel;
  InetAddress _peerAddr;
  Socket _socket;
  StateE _state;
  std::mutex _stateLock;

  TcpMessageCallback _messageCallback;
  TcpCallback _closeCallback;
  TcpCallback _writeCompleteCallback;
  TcpCallback _errorCallback;

  StreamBuffer _readBuffer;
  StreamBuffer _writeBuffer;

  std::any _userData;

  zlog_category_t* _zc;

  bool compareExchange(StateE compare, StateE exchange)
  {
    std::lock_guard lock(_stateLock);
    if (_state == compare) {
      _state = exchange;
      return true;
    }
    return false;
  }

  bool compare2Exchange(StateE compare1, StateE compare2, StateE exchange)
  {
    std::lock_guard lock(_stateLock);
    if (_state == compare1 || _state == compare2) {
      _state = exchange;
      return true;
    }
    return false;
  }

  void connectEstablished()
  {
    assert(_loop->isInEventLoop());
    {
      std::lock_guard lock(_stateLock);
      assert(_state == CONNECTING);
      _state = ESTABLISHED;
    }
    // hold a weak ref to this, in case any callback would close the connection
    _channel->tie(shared_from_this());
    _channel->setReadInterest();
  }

  /**
   * connectDestroyed()
   *
   * TcpServer and TcpClient will call this in their closeCallbacks.
   */
  void connectDestroyed()
  {
    assert(_loop->isInEventLoop());

    // sometimes, this function can be called without handleClose()
    if (compareExchange(ESTABLISHED, DISCONNECTED))
      _channel->unsetAllInterest();

    _channel->remove();
  }

  // Wrappers for the callbacks to be called from the event loop
  void handleRead()
  {
    assert(_loop->isInEventLoop());
    int rv = _readBuffer.readFd(_channel->fd());
    if (rv < 0) {
      if (errno == EAGAIN)
        return;
      handleError();
    } else if (rv == 0) {
      // the peer has nothing more to send us
      // we can safely close the connection
      handleClose();
    } else if (_messageCallback) {
      _messageCallback(shared_from_this(), &_readBuffer);
    }
  }

  void handleWrite()
  {
    assert(_loop->isInEventLoop());
    if (_channel->hasWriteEvent()) {
      ssize_t n = ::write(_channel->fd(), _writeBuffer.data(), _writeBuffer.size());
      if (n < 0) {
        if (errno == EAGAIN)
          return;
        handleError();
      } else {
        _writeBuffer.popFront(n);
        if (_writeBuffer.empty()) {
          _channel->unsetWriteInterest();
          if (_writeCompleteCallback)
            _loop->queueInLoop([&] {
              if (_writeCompleteCallback)
                _writeCompleteCallback(shared_from_this());
            });
        }
      }
    }
  }

  /**
   * handleClose() - called when EPOLLHUP is set
   *
   * Will be called in ioLoop after polling
   */
  void handleClose()
  {
    assert(_loop->isInEventLoop());

    // The handleClose() may be called multiple times
    if (!compare2Exchange(ESTABLISHED, DISCONNECTING, DISCONNECTED))
      return;

    _channel->unsetAllInterest();

    _messageCallback = nullptr;
    _writeCompleteCallback = nullptr;
    _errorCallback = nullptr;
    if (_closeCallback) {
      // cannot inline, because we will change _closeCallback afterward
      TcpConnectionPtr conn = shared_from_this();
      _closeCallback(conn);
      _closeCallback = nullptr;
    }
  }

  void handleError()
  {
    if (_errorCallback)
      _errorCallback(shared_from_this());
  }
};

#endif