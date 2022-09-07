#ifndef __TCPSERVER_HPP__
#define __TCPSERVER_HPP__

#include <signal.h>
#include <iostream>
#include <unordered_map>
#include <zlog.h>
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
            const ThreadInitCallback& init = nullptr)
    : _baseLoop(baseLoop)
    , _addr(listenAddr)
    , _acceptor(new Acceptor(baseLoop, listenAddr, reusePort))
    , _zc(zlog_get_category("TcpServer"))
    , _pool(baseLoop, threadCount, init)
  {
    // Ignore SIGPIPE
    static auto _ = signal(SIGPIPE, SIG_IGN);

    _acceptor->setNewConnectionCallback(
      [this](int sockfd, const InetAddress& peerAddr) { handleNewConnection(sockfd, peerAddr); });
  }
  ~TcpServer()
  {
    assert(_baseLoop->isInEventLoop());
    for (auto& [fd, _conn] : _connections) {
      TcpConnectionPtr conn(_conn);
      _conn.reset();
      conn->getLoop()->runInLoop([conn] { conn->connectDestroyed(); });
    }
  }

  EventLoop* getBaseLoop() const
  {
    return _baseLoop;
  }

  void start()
  {
    // make acceptor start listening
    zlog_info(_zc, "listening on %s", _addr.toIpPort().c_str());
    _baseLoop->runInLoop([&] { _acceptor->listen(); });
  }

  void setConnectCallback(TcpCallback cb)
  {
    _userConnectCallback = std::move(cb);
  }
  void setMessageCallback(TcpMessageCallback cb)
  {
    _userMessageCallback = std::move(cb);
  }
  void setWriteCompleteCallback(TcpCallback cb)
  {
    _userWriteCompleteCallback = std::move(cb);
  }
  void setCloseCallback(TcpCallback cb)
  {
    _userCloseCallback = std::move(cb);
  }
  void setErrorCallback(TcpCallback cb)
  {
    _userErrorCallback = std::move(cb);
  }

private:
  EventLoop* _baseLoop;
  InetAddress _addr;
  std::unique_ptr<Acceptor> _acceptor;
  EventLoopThreadPool _pool;
  std::unordered_map<int, TcpConnectionPtr> _connections;

  // default callbacks for created connections
  TcpCallback _userConnectCallback;
  TcpMessageCallback _userMessageCallback;
  TcpCallback _userWriteCompleteCallback;
  TcpCallback _userCloseCallback;
  TcpCallback _userErrorCallback;

  zlog_category_t* _zc;

  void handleNewConnection(int sockfd, const InetAddress& peerAddr)
  {
    assert(_baseLoop->isInEventLoop());
    EventLoop* ioLoop = _pool.getNextLoop();
    auto conn = std::make_shared<TcpConnection>(ioLoop, sockfd, peerAddr);
    _connections.insert({sockfd, conn});
    conn->setMessageCallback(_userMessageCallback);
    conn->setWriteCompleteCallback(_userWriteCompleteCallback);
    conn->setCloseCallback([&](const TcpConnectionPtr& conn) { handleClose(conn); });
    conn->setErrorCallback(_userErrorCallback);
    // we are in the base loop, so we cannot call connectEstablished directly
    ioLoop->queueInLoop([&, conn] {
      conn->connectEstablished();
      if (_userConnectCallback)
        _userConnectCallback(conn);
    });
  }

  // user close callback wrapper
  void handleClose(const TcpConnectionPtr& conn)
  {
    // CHECKME: capture conn by value or reference?
    _baseLoop->runInLoop([&, conn] { handleCloseInLoop(conn); });
  }

  void handleCloseInLoop(const TcpConnectionPtr& conn)
  {
    assert(_baseLoop->isInEventLoop());
    _connections.erase(conn->fd());

    EventLoop* connLoop = conn->getLoop();
    // CHECKME: capture conn by value or reference?
    connLoop->queueInLoop([&, conn] {
      conn->connectDestroyed();
      if (_userCloseCallback)
        _userCloseCallback(conn);
    });
  }
};

#endif
