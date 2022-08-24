#ifndef __TCPCLIENT_HPP__
#define __TCPCLIENT_HPP__

#include "Socket.hpp"
#include "EventLoop.hpp"
#include "Connector.hpp"
#include "TcpConnection.hpp"

typedef std::shared_ptr<Connector> ConnectorPtr;

class TcpClient : noncopyable
{
public:
  TcpClient(EventLoop* loop, const InetAddress& serverAddr)
    : _loop(loop)
    , _serverAddr(serverAddr)
    , _connector(new Connector(loop, serverAddr))
    , _running(false)
    , _reconnect(false)
  {
    _connector->setNewConnectionCallback([&](int sockfd) { newConnection(sockfd); });
  }
  ~TcpClient() {}

  void enableReconnect()
  {
    _reconnect = true;
  }

  void connect()
  {
    _running = true;
    _connector->start();
  }

  void disconnect()
  {
    _running = false;
    std::lock_guard lock(_mutex);
    if (_connection)
      _connection->shutdown();
  }

  void stop()
  {
    _running = false;
    _connector->stop();
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
  EventLoop* _loop;
  InetAddress _serverAddr;
  ConnectorPtr _connector;
  bool _running;
  bool _reconnect;
  std::mutex _mutex;
  TcpConnectionPtr _connection;

  // default callbacks for created connections
  ConnectCallback _userConnectCallback;
  MessageCallback _userMessageCallback;
  WriteCompleteCallback _userWriteCompleteCallback;
  CloseCallback _userCloseCallback;

  void newConnection(int sockfd)
  {
    assert(_loop->isInEventLoop());
    InetAddress peerAddr = ::getPeerAddr(sockfd);
    // the conn is on the same loop
    auto conn = std::make_shared<TcpConnection>(_loop, sockfd, peerAddr);
    conn->setConnectCallback(_userConnectCallback);
    conn->setMessageCallback(_userMessageCallback);
    conn->setWriteCompleteCallback(_userWriteCompleteCallback);
    conn->setCloseCallback([&](const TcpConnectionPtr& conn) { handleClose(conn); });
    {
      std::lock_guard lock(_mutex);
      _connection = conn;
    }
    conn->connectEstablished();
  }

  // user close callback wrapper
  void handleClose(const TcpConnectionPtr& conn)
  {
    assert(_loop->isInEventLoop());
    assert(_loop == conn->getLoop());
    {
      std::lock_guard lock(_mutex);
      _connection.reset();
    }
    // CHECKME: capture conn by value or reference?
    _loop->queueInLoop([&, conn] { conn->connectDestroyed(_userCloseCallback); });

    if (_reconnect && _running)
      _connector->restart();
  }
};

#endif
