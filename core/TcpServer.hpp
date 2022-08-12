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
  TcpServer(EventLoop* baseLoop, const InetAddress& listenAddr, bool reusePort)
    : _baseLoop(baseLoop)
    , _addr(listenAddr)
    , _acceptor(new Acceptor(baseLoop, listenAddr, reusePort))
    , _pool(baseLoop, 12)
  {
    _acceptor->setNewConnectionCallback(std::bind(
      &TcpServer::handleNewConnection, this, std::placeholders::_1, std::placeholders::_2));
  }
  ~TcpServer() {}

  void start()
  {
    // make acceptor start listening
    _baseLoop->runInLoop(std::bind(&Acceptor::listen, _acceptor.get()));
  }

  void setConnectionCallback(const ConnectionCallback& cb)
  {
    _connectionCallback = cb;
  }
  void setMessageCallback(const MessageCallback& cb)
  {
    _messageCallback = cb;
  }
  void setWriteCompleteCallback(const WriteCompleteCallback& cb)
  {
    _writeCompleteCallback = cb;
  }

private:
  EventLoop* _baseLoop;
  InetAddress _addr;
  std::unique_ptr<Acceptor> _acceptor;
  EventLoopThreadPool _pool;
  std::vector<TcpConnectionPtr> _connections;

  // default callbacks for created connections
  ConnectionCallback _connectionCallback;
  MessageCallback _messageCallback;
  WriteCompleteCallback _writeCompleteCallback;

  void handleNewConnection(int sockfd, const InetAddress& peerAddr)
  {
    assert(_baseLoop->isOwner());
    std::cout << "TcpServer::handleNewConnection [" << _addr.ip() << ":" << _addr.port()
              << "] - new connection [" << peerAddr.ip() << ":" << peerAddr.port() << "]"
              << std::endl;
    EventLoop* ioLoop = _pool.getNextLoop();
    TcpConnectionPtr conn(new TcpConnection(ioLoop, sockfd, peerAddr));
    _connections.push_back(conn);
    // will be called soon
    conn->setConnectionCallback(_connectionCallback);
    conn->setMessageCallback(_messageCallback);
    conn->setWriteCompleteCallback(_writeCompleteCallback);
    conn->setCloseCallback(std::bind(&TcpServer::handleClose, this, std::placeholders::_1));
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
  }

  void handleClose(const TcpConnectionPtr& conn)
  {
    _baseLoop->runInLoop(std::bind(&TcpServer::handleCloseInLoop, this, conn));
  }

  void handleCloseInLoop(const TcpConnectionPtr& conn)
  {
    assert(_baseLoop->isOwner());

    EventLoop* connLoop = conn->getLoop();
    connLoop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
  }
};

#endif
