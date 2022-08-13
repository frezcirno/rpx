#ifndef __TCPSERVER_HPP__
#define __TCPSERVER_HPP__

#include <iostream>
#include <unordered_map>
#include "Socket.hpp"
#include "Acceptor.hpp"
#include "EventLoop.hpp"
#include "ThreadPool.hpp"
#include "EventLoopThreadPool.hpp"
#include "TcpConnection.hpp"

class TcpServer
{
public:
  TcpServer(EventLoop* baseLoop, const InetAddress& listenAddr, bool reusePort,
            int threadCount = 12)
    : _baseLoop(baseLoop)
    , _addr(listenAddr)
    , _acceptor(new Acceptor(baseLoop, listenAddr, reusePort))
    , _pool(baseLoop, threadCount)
  {
    _acceptor->setNewConnectionCallback(std::bind(
      &TcpServer::handleNewConnection, this, std::placeholders::_1, std::placeholders::_2));
  }
  ~TcpServer() {}

  EventLoop* getBaseLoop() const
  {
    return _baseLoop;
  }

  void start()
  {
    // make acceptor start listening
    std::cout << "listening on " << _addr.toIpPort() << std::endl;
    _baseLoop->runInLoop(std::bind(&Acceptor::listen, _acceptor.get()));
  }

  void setConnectCallback(const ConnectCallback& cb)
  {
    _userConnectCallback = cb;
  }
  void setMessageCallback(const MessageCallback& cb)
  {
    _userMessageCallback = cb;
  }
  void setWriteCompleteCallback(const WriteCompleteCallback& cb)
  {
    _userWriteCompleteCallback = cb;
  }
  void setCloseCallback(const CloseCallback& cb)
  {
    _userCloseCallback = cb;
  }

private:
  EventLoop* _baseLoop;
  InetAddress _addr;
  std::unique_ptr<Acceptor> _acceptor;
  EventLoopThreadPool _pool;
  std::unordered_map<int, TcpConnectionPtr> _connections;

  // default callbacks for created connections
  ConnectCallback _userConnectCallback;
  MessageCallback _userMessageCallback;
  WriteCompleteCallback _userWriteCompleteCallback;
  CloseCallback _userCloseCallback;

  void handleNewConnection(int sockfd, const InetAddress& peerAddr)
  {
    assert(_baseLoop->isInEventLoop());
    EventLoop* ioLoop = _pool.getNextLoop();
    TcpConnectionPtr conn(new TcpConnection(ioLoop, sockfd, peerAddr));
    _connections.insert({sockfd, conn});
    conn->setConnectCallback(_userConnectCallback);
    conn->setMessageCallback(_userMessageCallback);
    conn->setWriteCompleteCallback(_userWriteCompleteCallback);
    conn->setCloseCallback(std::bind(&TcpServer::handleClose, this, std::placeholders::_1));
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
  }

  // user close callback wrapper
  void handleClose(const TcpConnectionPtr& conn)
  {
    _baseLoop->runInLoop(std::bind(&TcpServer::handleCloseInLoop, this, conn));
  }

  void handleCloseInLoop(const TcpConnectionPtr& conn)
  {
    assert(_baseLoop->isInEventLoop());
    _connections.erase(conn->fd());

    EventLoop* connLoop = conn->getLoop();
    connLoop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn, _userCloseCallback));
  }
};

#endif
