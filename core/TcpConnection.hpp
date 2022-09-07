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
  ~TcpConnection() {}

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

  void setConnectCallback(TcpCallback cb)
  {
    _connectCallback = std::move(cb);
  }
  void setMessageCallback(TcpMessageCallback cb)
  {
    _messageCallback = std::move(cb);
  }
  void setCloseCallback(TcpCallback cb)
  {
    _closeCallback = std::move(cb);
  }
  void setWriteCompleteCallback(TcpCallback cb)
  {
    _writeCompleteCallback = std::move(cb);
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
      if (!_channel->hasWriteInterest())
        _channel->setWriteInterest();
    }
    return len;
  }
  int write(const std::string& data)
  {
    return write(data.data(), data.size());
  }

  void shutdown()
  {
    auto state = CONNECTED;
    if (_state.compare_exchange_strong(state, DISCONNECTING))
      _loop->runInLoop([&] { _socket.shutdownWrite(); });
  }

  void forceClose()
  {
    // the DISCONNECTED is the final state, so it should be safe
    if (_state.load() == DISCONNECTED)
      return;

    auto state = _state.exchange(DISCONNECTING);
    if (state == CONNECTED || state == DISCONNECTING) {
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
  enum StateE
  {
    CONNECTING = 0,
    CONNECTED,
    DISCONNECTING,
    DISCONNECTED,
  };

private:
  EventLoop* _loop;
  std::unique_ptr<Channel> _channel;
  InetAddress _peerAddr;
  Socket _socket;
  std::atomic<StateE> _state;

  TcpCallback _connectCallback;
  TcpMessageCallback _messageCallback;
  TcpCallback _closeCallback;
  TcpCallback _writeCompleteCallback;

  StreamBuffer _readBuffer;
  StreamBuffer _writeBuffer;

  std::any _userData;

  zlog_category_t* _zc;

  void connectEstablished()
  {
    assert(_loop->isInEventLoop());
    assert(_state.exchange(CONNECTED) == CONNECTING);
    _channel->tie(shared_from_this());
    _channel->setReadInterest();
    if (_connectCallback)
      _connectCallback(shared_from_this());
  }

  /**
   * connectDestroyed()
   *
   * Cannot be called in handleClose() -> _closeCallback()
   */
  void connectDestroyed(const TcpCallback& userCloseCallback)
  {
    assert(_loop->isInEventLoop());
    auto state = CONNECTED;
    if (_state.compare_exchange_strong(state, DISCONNECTED))
      _channel->unsetAllInterest();
    _channel->remove();
    if (userCloseCallback)
      userCloseCallback(shared_from_this());
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

  void handleClose()
  {
    assert(_loop->isInEventLoop());
    auto oldstate = _state.exchange(DISCONNECTED);
    assert(oldstate == CONNECTED || oldstate == DISCONNECTING);
    _channel->unsetAllInterest();

    _connectCallback = nullptr;
    _messageCallback = nullptr;
    _writeCompleteCallback = nullptr;
    if (_closeCallback) {
      TcpConnectionPtr guardThis(shared_from_this());
      // must be the last line
      _closeCallback(guardThis);
      _closeCallback = nullptr;
    }
  }

  void handleError()
  {
    int rv = ::getSocketError(_channel->fd());
    zlog_error(_zc,
               "TcpConnection::handleError [%s] - SO_ERROR = %d, %s\n",
               _peerAddr.toIpPort().c_str(),
               rv,
               strerror(rv));
  }
};

#endif