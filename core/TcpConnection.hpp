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
typedef std::function<void(const TcpConnectionPtr&)> ConnectCallback;
typedef std::function<void(const TcpConnectionPtr&, StreamBuffer*)> MessageCallback;
typedef std::function<void(const TcpConnectionPtr&)> CloseCallback;
typedef std::function<void(const TcpConnectionPtr&)> WriteCompleteCallback;

class TcpServer;

class TcpConnection : noncopyable, public std::enable_shared_from_this<TcpConnection>
{
  friend TcpServer;

public:
  TcpConnection(EventLoop* loop, int sockfd, const InetAddress& peerAddr)
    : _loop(loop)
    , _channel(new Channel(loop, sockfd))
    , _peerAddr(peerAddr)
    , _socket(new Socket(sockfd))
    , _readBuffer(4096)
    , _writeBuffer(4096, 100)
  {
    _channel->setReadCallback(std::bind(&TcpConnection::handleRead, this));
    _channel->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
    _channel->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
    _channel->setErrorCallback(std::bind(&TcpConnection::handleError, this));
    _socket->setKeepAlive(true);
  }
  ~TcpConnection() {}

  int fd()
  {
    return _channel->fd();
  }
  const InetAddress& getPeerAddr() const
  {
    return _peerAddr;
  }

  void write(const char* data, size_t len)
  {
    assert(_loop->isInEventLoop());
    ssize_t written;
    size_t remaining = len;
    if (!_channel->hasWriteInterest() && _writeBuffer.empty()) {
      written = ::write(_channel->fd(), data, len);
      if (written < 0) {
        if (errno != EWOULDBLOCK) {
          perror("write");
          return;
        }
        written = 0;
      } else {
        remaining -= written;
        if (remaining == 0) {
          _loop->queueInLoop(std::bind(_writeCompleteCallback, shared_from_this()));
          return;
        }
      }
    }

    if (remaining > 0) {
      _writeBuffer.append(data + written, remaining);
      if (!_channel->hasWriteInterest()) {
        _channel->setWriteInterest();
      }
    }
  }
  void write(const std::string& data)
  {
    write(data.data(), data.size());
  }

  std::any getUserData()
  {
    return _userData;
  }
  void setUserData(const std::any& userData)
  {
    _userData = userData;
  }

private:
  EventLoop* _loop;
  std::unique_ptr<Channel> _channel;
  InetAddress _peerAddr;
  std::unique_ptr<Socket> _socket;

  ConnectCallback _connectCallback;
  MessageCallback _messageCallback;
  CloseCallback _closeCallback;
  WriteCompleteCallback _writeCompleteCallback;

  StreamBuffer _readBuffer;
  StreamBuffer _writeBuffer;

  std::any _userData;

  EventLoop* getLoop() const
  {
    return _loop;
  }
  void setConnectCallback(const ConnectCallback& cb)
  {
    _connectCallback = cb;
  }
  void setMessageCallback(const MessageCallback& cb)
  {
    _messageCallback = cb;
  }
  void setCloseCallback(const CloseCallback& cb)
  {
    _closeCallback = cb;
  }
  void setWriteCompleteCallback(const WriteCompleteCallback& cb)
  {
    _writeCompleteCallback = cb;
  }

  void connectEstablished()
  {
    assert(_loop->isInEventLoop());
    _channel->tie(shared_from_this());
    _channel->setReadInterest();

    _connectCallback(shared_from_this());
  }

  void connectDestroyed(const CloseCallback& userCloseCallback)
  {
    assert(_loop->isInEventLoop());
    _channel->unsetAllInterest();
    userCloseCallback(shared_from_this());
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
    } else {
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
            _writeCompleteCallback(shared_from_this());
        } else {
          _channel->setWriteInterest();
        }
      }
    }
  }

  void handleClose()
  {
    assert(_loop->isInEventLoop());
    _channel->unsetAllInterest();

    TcpConnectionPtr guardThis(shared_from_this());
    // must be the last line
    _closeCallback(guardThis);
  }

  void handleError()
  {
    int rv = ::getSocketError(_channel->fd());
    printf("TcpConnection::handleError [%s:%d] - SO_ERROR = %d\n",
           _peerAddr.ip().c_str(),
           _peerAddr.port(),
           rv);
  }
};

#endif