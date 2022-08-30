#ifndef __SOCKET_HPP__
#define __SOCKET_HPP__

#include "Utils.hpp"
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <string>
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
  int rv = ::bind(fd, sockaddr_cast(addr), sizeof(*addr));
  if (rv < 0) {
    perror("bind");
    abort();
  }
}

void listen(int fd)
{
  int rv = ::listen(fd, SOMAXCONN);
  if (rv < 0) {
    perror("listen");
    abort();
  }
}

int accept(int fd, struct sockaddr_in* addr)
{
  socklen_t addrlen = sizeof(*addr);
  int rv = ::accept(fd, sockaddr_cast(addr), &addrlen);
  if (rv < 0) {
    perror("accept");
    abort();
  }
  return rv;
}

int connect(int sockfd, const struct sockaddr_in* addr)
{
  return ::connect(sockfd, sockaddr_cast(addr), static_cast<socklen_t>(sizeof(*addr)));
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

struct sockaddr_in getLocalAddr(int sockfd)
{
  struct sockaddr_in localaddr;
  memset(&localaddr, 0, sizeof(localaddr));
  socklen_t addrlen = static_cast<socklen_t>(sizeof(localaddr));
  if (::getsockname(sockfd, sockaddr_cast(&localaddr), &addrlen) < 0) {
    perror("getsockname");
    abort();
  }
  return localaddr;
}

struct sockaddr_in getPeerAddr(int sockfd)
{
  struct sockaddr_in peeraddr;
  memset(&peeraddr, 0, sizeof(peeraddr));
  socklen_t addrlen = static_cast<socklen_t>(sizeof(peeraddr));
  if (::getpeername(sockfd, sockaddr_cast(&peeraddr), &addrlen) < 0) {
    perror("getpeername");
    abort();
  }
  return peeraddr;
}

bool isSelfConnect(int sockfd)
{
  struct sockaddr_in localaddr = getLocalAddr(sockfd);
  if (localaddr.sin_family != AF_INET)
    return false;

  struct sockaddr_in peeraddr = getPeerAddr(sockfd);
  const struct sockaddr_in* laddr4 = reinterpret_cast<struct sockaddr_in*>(&localaddr);
  const struct sockaddr_in* raddr4 = reinterpret_cast<struct sockaddr_in*>(&peeraddr);
  return laddr4->sin_port == raddr4->sin_port && laddr4->sin_addr.s_addr == raddr4->sin_addr.s_addr;
}

class InetAddress
{
public:
  InetAddress(const char* host, uint16_t port)
  {
    memset(&_addr, 0, sizeof(_addr));
    _addr.sin_family = AF_INET;
    _addr.sin_port = htons(port);
    if (host == nullptr) {
      _addr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
      _addr.sin_addr.s_addr = inet_addr(host);
      if (_addr.sin_addr.s_addr == INADDR_NONE) {
        struct hostent* hent = gethostbyname(host);
        if (hent == nullptr) {
          herror("gethostbyname");
          abort();
        }
        _addr.sin_addr = *reinterpret_cast<struct in_addr*>(hent->h_addr);
      }
    }
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
  uint16_t port() const
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
    return ::accept(_fd, peeraddr->getSockAddr());
  }
  void shutdownWrite()
  {
    int rv = ::shutdown(_fd, SHUT_WR);
    if (rv < 0)
      perror("shutdownWrite");
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