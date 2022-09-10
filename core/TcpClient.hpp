#ifndef __TCPCLIENT_HPP__
#define __TCPCLIENT_HPP__

#include "Socket.hpp"
#include "EventLoop.hpp"
#include "Connector.hpp"
#include "TcpConnection.hpp"

class TcpClient : noncopyable, public std::enable_shared_from_this<TcpClient>
{
public:
  TcpClient(EventLoop* loop, const InetAddress& serverAddr)
    : _loop(loop)
    , _serverAddr(serverAddr)
    , _connector(new Connector(loop, serverAddr))
    , _running(false)
    , _reconnect(false)
    , _zc(zlog_get_category("TcpClient"))
  {
    _connector->setNewConnectionCallback([&](int sockfd) { newConnection(sockfd); });
  }
  ~TcpClient()
  {
    TcpConnectionPtr conn;
    bool unique = false;
    {
      std::lock_guard lock(_mutex);
      unique = _connection.unique();
      conn = _connection;
    }
    if (conn) {
      assert(conn->getLoop() == _loop);
      if (unique)
        conn->forceClose();
    } else {
      _connector->stop();
    }
  }

  EventLoop* getLoop() const
  {
    return _loop;
  }

  void enableReconnect()
  {
    _reconnect = true;
  }

  /**
   * start(): Start connecting
   */
  void start()
  {
    _running = true;
    _connector->start();
  }

  /**
   * stopConnect(): Interrupt the connecting process
   */
  void stopConnect()
  {
    _running = false;
    _connector->stop();
  }

  /**
   * shutdown(): Shutdown the connection if exists.
   */
  void shutdown()
  {
    _running = false;
    std::lock_guard lock(_mutex);
    if (_connection)
      _connection->shutdown();
  }

  /**
   * forceClose(): close the connection if exists.
   */
  void forceClose()
  {
    _running = false;
    std::lock_guard lock(_mutex);
    if (_connection)
      _connection->forceClose();
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

private:
  EventLoop* _loop;
  InetAddress _serverAddr;
  std::shared_ptr<Connector> _connector;
  bool _running;
  bool _reconnect;
  std::mutex _mutex;
  TcpConnectionPtr _connection;

  // default callbacks for created connections
  TcpCallback _userConnectCallback;
  TcpMessageCallback _userMessageCallback;
  TcpCallback _userWriteCompleteCallback;
  TcpCallback _userCloseCallback;

  zlog_category_t* _zc;

  void newConnection(int sockfd)
  {
    assert(_loop->isInEventLoop());
    InetAddress peerAddr;
    getPeerAddr(sockfd, peerAddr);
    // the TcpConnection is in the same loop as TcpClient
    auto conn = std::make_shared<TcpConnection>(_loop, sockfd, peerAddr);
    conn->setMessageCallback(_userMessageCallback);
    conn->setWriteCompleteCallback(_userWriteCompleteCallback);
    conn->setCloseCallback([&](const TcpConnectionPtr& conn) { handleClose(conn); });
    {
      std::lock_guard lock(_mutex);
      _connection = conn;
    }
    conn->connectEstablished();
    if (_userConnectCallback)
      _userConnectCallback(conn);
  }

  // user close callback wrapper
  void handleClose(const TcpConnectionPtr& conn)
  {
    assert(_loop->isInEventLoop());
    assert(_loop == conn->getLoop());
    {
      std::lock_guard lock(_mutex);
      assert(_connection == conn);
      _connection.reset();
    }

    // queueInLoop here, or the connection may be destructed in the middle of a loop
    _loop->queueInLoop([&, conn] {
      conn->connectDestroyed();
      if (_userCloseCallback)
        _userCloseCallback(conn);
    });

    // FIXME: reconnect
    if (_reconnect && _running)
      _connector->restart();
  }
};

#endif
