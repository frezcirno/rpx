#ifndef __TCPSERVER_HPP__
#define __TCPSERVER_HPP__

#include <signal.h>
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
  TcpServer(EventLoop* baseLoop, const InetAddress& listenAddr, bool reusePort, int threadCount,
            ThreadInitCallback cb = ThreadInitCallback())
    : _baseLoop(baseLoop)
    , _addr(listenAddr)
    , _acceptor(new Acceptor(baseLoop, listenAddr, reusePort))
    , _pool(baseLoop, threadCount, cb)
  {
    // Ignore SIGPIPE
    static auto init = signal(SIGPIPE, SIG_IGN);

    _acceptor->setNewConnectionCallback(
      [&](int sockfd, const InetAddress& peerAddr) { handleNewConnection(sockfd, peerAddr); });
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
    _baseLoop->runInLoop([&] { _acceptor->listen(); });
  }

  void setConnectCallback(ConnectCallback cb)
  {
    _userConnectCallback = std::move(cb);
  }
  void setMessageCallback(MessageCallback cb)
  {
    _userMessageCallback = std::move(cb);
  }
  void setWriteCompleteCallback(WriteCompleteCallback cb)
  {
    _userWriteCompleteCallback = std::move(cb);
  }
  void setCloseCallback(CloseCallback cb)
  {
    _userCloseCallback = std::move(cb);
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
    auto conn = std::make_shared<TcpConnection>(ioLoop, sockfd, peerAddr);
    _connections.insert({sockfd, conn});
    conn->setConnectCallback(_userConnectCallback);
    conn->setMessageCallback(_userMessageCallback);
    conn->setWriteCompleteCallback(_userWriteCompleteCallback);
    conn->setCloseCallback([&](const TcpConnectionPtr& conn) { handleClose(conn); });
    ioLoop->runInLoop([conn] { conn->connectEstablished(); });
  }

  // user close callback wrapper
  void handleClose(const TcpConnectionPtr& conn)
  {
    // CHECKME: capture conn by value or reference?
    _baseLoop->runInLoop([this, conn] { handleCloseInLoop(conn); });
  }

  void handleCloseInLoop(const TcpConnectionPtr& conn)
  {
    assert(_baseLoop->isInEventLoop());
    _connections.erase(conn->fd());

    EventLoop* connLoop = conn->getLoop();
    // CHECKME: capture conn by value or reference?
    connLoop->queueInLoop([this, conn] { conn->connectDestroyed(_userCloseCallback); });
  }
};

#endif
