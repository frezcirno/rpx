#ifndef __SOCKET_HPP__
#define __SOCKET_HPP__

#include "Utils.hpp"
#include <arpa/inet.h>
#include <assert.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

struct sockaddr* sockaddr_cast(struct sockaddr_in* addr)
{
  return reinterpret_cast<struct sockaddr*>(addr);
}

const struct sockaddr* sockaddr_cast(const struct sockaddr_in* addr)
{
  return reinterpret_cast<const struct sockaddr*>(addr);
}

int socket(sa_family_t family)
{
  int rv = ::socket(family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
  return rv;
}

void bind(int fd, const struct sockaddr_in* addr)
{
  int rv = ::bind(fd, reinterpret_cast<const struct sockaddr*>(addr), sizeof(*addr));
  if (rv < 0) {
    perror("bind");
    abort();
  }
}

void listen(int fd)
{
  int rv = ::listen(fd, SOMAXCONN);
  assert(rv == 0);
}

int accept(int fd, struct sockaddr_in* addr)
{
  socklen_t addrlen = sizeof(*addr);
  int rv = ::accept(fd, reinterpret_cast<struct sockaddr*>(addr), &addrlen);
  if (rv < 0) {
    perror("accept");
    abort();
  }
  return rv;
}

int connect(int sockfd, const struct sockaddr* addr)
{
  return ::connect(sockfd, addr, static_cast<socklen_t>(sizeof(struct sockaddr_in6)));
}

int getSocketError(int sockfd)
{
  int optval;
  socklen_t optlen = static_cast<socklen_t>(sizeof optval);

  if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0) {
    return errno;
  } else {
    return optval;
  }
}

class InetAddress
{
public:
  InetAddress(const char* ip, uint16_t port)
  {
    memset(&_addr, 0, sizeof(_addr));
    _addr.sin_family = AF_INET;
    _addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &_addr.sin_addr);
  }
  explicit InetAddress(uint16_t port = 0)
    : InetAddress("0.0.0.0", port)
  {}
  InetAddress(const struct sockaddr_in& addr)
    : _addr(addr)
  {}
  ~InetAddress() {}
  sa_family_t family() const
  {
    return AF_INET;
  }
  std::string ip() const
  {
    return inet_ntoa(_addr.sin_addr);
  }
  int port() const
  {
    return ntohs(_addr.sin_port);
  }
  std::string toIpPort() const
  {
    return ip() + ":" + std::to_string(port());
  }
  const struct sockaddr_in* getSockAddr() const
  {
    return &_addr;
  }
  struct sockaddr_in* getSockAddr()
  {
    return &_addr;
  }

private:
  struct sockaddr_in _addr;
};

class Socket : noncopyable
{
public:
  explicit Socket(int fd)
    : _fd(fd)
  {}

  explicit Socket(sa_family_t family)
    : _fd(::socket(family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP))
  {}

  ~Socket()
  {
    ::close(_fd);
  }

  int fd() const
  {
    return _fd;
  }
  void bind(const InetAddress& addr)
  {
    ::bind(_fd, addr.getSockAddr());
  }
  void listen()
  {
    ::listen(_fd);
  }
  int accept(InetAddress* peeraddr)
  {
    int connfd = ::accept(_fd, peeraddr->getSockAddr());
    assert(connfd >= 0);
    return connfd;
  }
  void shutdownWrite()
  {
    ::shutdown(_fd, SHUT_WR);
  }
  void setReuseAddr(bool on)
  {
    int optval = on ? 1 : 0;
    ::setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
  }
  void setReusePort(bool on)
  {
    int optval = on ? 1 : 0;
    ::setsockopt(_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof optval);
  }
  void setKeepAlive(bool on)
  {
    int optval = on ? 1 : 0;
    ::setsockopt(_fd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof optval);
  }
  void setTcpNoDelay(bool on)
  {
    int optval = on ? 1 : 0;
    ::setsockopt(_fd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof optval);
  }

private:
  const int _fd;
};

#endif