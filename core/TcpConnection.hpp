#ifndef __TCPCONNECTION_HPP__
#define __TCPCONNECTION_HPP__

#include "assert.h"
#include "Utils.hpp"
#include "Socket.hpp"
#include "StreamBuffer.hpp"
#include "EventLoop.hpp"


class TcpConnection;
typedef std::shared_ptr<TcpConnection> TcpConnectionPtr;
typedef std::function<void(const TcpConnectionPtr&)> ConnectionCallback;
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
  void write(const char* data, size_t len)
  {
    assert(_loop->isOwner());
    _writeBuffer.append(data, len);
    if (!_channel->hasWriteInterest()) {
      _channel->setWriteInterest();
    }
  }
  void write(const std::string& data)
  {
    write(data.data(), data.size());
  }

private:
  EventLoop* _loop;
  std::unique_ptr<Channel> _channel;
  InetAddress _peerAddr;
  std::unique_ptr<Socket> _socket;

  ConnectionCallback _connectionCallback;
  MessageCallback _messageCallback;
  CloseCallback _closeCallback;
  WriteCompleteCallback _writeCompleteCallback;

  StreamBuffer _readBuffer;
  StreamBuffer _writeBuffer;

  EventLoop* getLoop() const
  {
    return _loop;
  }
  void setConnectionCallback(const ConnectionCallback& cb)
  {
    _connectionCallback = cb;
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
    assert(_loop->isOwner());
    std::cout << "connect established" << std::endl;
    _channel->tie(shared_from_this());
    _channel->setReadInterest();

    _connectionCallback(shared_from_this());
  }

  void connectDestroyed()
  {
    assert(_loop->isOwner());
    std::cout << "connect destroyed" << std::endl;
    _channel->unsetAllInterest();
    _connectionCallback(shared_from_this());
    _channel->remove();
  }

  void handleRead()
  {
    assert(_loop->isOwner());
    int rv = _readBuffer.readFd(_channel->fd());
    if (rv < 0) {
      if (errno == EAGAIN)
        return;
      handleError();
    } else if (rv == 0) {
      handleClose();
    } else {
      _messageCallback(shared_from_this(), &_readBuffer);
    }
  }

  void handleWrite()
  {
    assert(_loop->isOwner());
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
    assert(_loop->isOwner());
    _channel->unsetAllInterest();

    TcpConnectionPtr guardThis(shared_from_this());
    _connectionCallback(guardThis);
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