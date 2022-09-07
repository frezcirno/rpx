#ifndef __ACCEPTOR_HPP__
#define __ACCEPTOR_HPP__

#include <sys/socket.h>
#include <sys/uio.h>
#include <fcntl.h>

#include "EventLoop.hpp"
#include "Socket.hpp"

class Acceptor
{
public:
  typedef std::function<void(int sockfd, const InetAddress&)> NewConnectionCallback;

  Acceptor(EventLoop* loop, const InetAddress& addr, bool reusePort)
    : _loop(loop)
    , socket(addr.family())
    , serverChannel(loop, socket.fd())
    , _idleFd(::open("/dev/null", O_RDONLY | O_CLOEXEC))
  {
    socket.setReuseAddr(true);
    socket.setReuseAddr(reusePort);
    socket.bind(addr);
    serverChannel.setReadCallback([&] { handleRead(); });
  }
  ~Acceptor()
  {
    serverChannel.unsetAllInterest();
    serverChannel.remove();
    ::close(_idleFd);
  }

  void setNewConnectionCallback(NewConnectionCallback cb)
  {
    _newConnectionCallback = std::move(cb);
  }

  void listen()
  {
    assert(_loop->isInEventLoop());
    socket.listen();
    serverChannel.setReadInterest();
  }

private:
  EventLoop* _loop;
  Socket socket;
  Channel serverChannel;
  NewConnectionCallback _newConnectionCallback;
  int _idleFd;

  void handleRead()
  {
    assert(_loop->isInEventLoop());

    InetAddress peerAddr;
    while (true) {
      int connfd = ::accept(socket.fd(), peerAddr);
      if (connfd < 0) {
        if (errno == EMFILE) {
          ::close(_idleFd);
          _idleFd = ::accept(socket.fd(), NULL, NULL);
          ::close(_idleFd);
          _idleFd = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
        }
        return;
      }

      if (_newConnectionCallback)
        _newConnectionCallback(connfd, peerAddr);
      else
        ::close(connfd);
    }
  }
};


#endif
