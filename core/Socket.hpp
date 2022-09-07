#ifndef __SOCKET_HPP__
#define __SOCKET_HPP__

#include "Utils.hpp"
#include <pcre.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <unistd.h>

inline struct sockaddr* sockaddr_cast(struct sockaddr_in* addr)
{
  return reinterpret_cast<struct sockaddr*>(addr);
}

const inline struct sockaddr* sockaddr_cast(const struct sockaddr_in* addr)
{
  return reinterpret_cast<const struct sockaddr*>(addr);
}

inline struct sockaddr* sockaddr_cast(struct sockaddr_in6* addr)
{
  return reinterpret_cast<struct sockaddr*>(addr);
}

const inline struct sockaddr* sockaddr_cast(const struct sockaddr_in6* addr)
{
  return reinterpret_cast<const struct sockaddr*>(addr);
}

inline int socket(sa_family_t family)
{
  int rv = ::socket(family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
  return rv;
}

inline int bind(int fd, const struct sockaddr_in* addr)
{
  return ::bind(fd, sockaddr_cast(addr), sizeof(*addr));
}

inline int bind(int fd, const struct sockaddr_in6* addr)
{
  return ::bind(fd, sockaddr_cast(addr), sizeof(*addr));
}

inline int listen(int fd)
{
  return ::listen(fd, SOMAXCONN);
}

inline int accept(int fd, struct sockaddr_in* addr)
{
  socklen_t addrlen = sizeof(*addr);
  return ::accept(fd, sockaddr_cast(addr), &addrlen);
}

inline int accept(int fd, struct sockaddr_in6* addr)
{
  socklen_t addrlen = sizeof(*addr);
  return ::accept(fd, sockaddr_cast(addr), &addrlen);
}

inline int connect(int sockfd, const struct sockaddr_in* addr)
{
  return ::connect(sockfd, sockaddr_cast(addr), static_cast<socklen_t>(sizeof(*addr)));
}

inline int connect(int sockfd, const struct sockaddr_in6* addr)
{
  return ::connect(sockfd, sockaddr_cast(addr), static_cast<socklen_t>(sizeof(*addr)));
}

inline int getSocketError(int sockfd)
{
  int optval;
  socklen_t optlen = static_cast<socklen_t>(sizeof optval);

  if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0) {
    return errno;
  } else {
    return optval;
  }
}

inline sa_family_t getSockFamily(int sockfd)
{
  struct sockaddr_storage ss;
  socklen_t len = sizeof(ss);
  if (::getsockname(sockfd, reinterpret_cast<struct sockaddr*>(&ss), &len) < 0) {
    return AF_UNSPEC;
  }
  return ss.ss_family;
}

inline void getLocalAddr(int sockfd, struct sockaddr_in& localaddr)
{
  socklen_t addrlen = static_cast<socklen_t>(sizeof(localaddr));
  if (::getsockname(sockfd, sockaddr_cast(&localaddr), &addrlen) < 0) {
    perror("getsockname");
    abort();
  }
  assert(addrlen == sizeof(localaddr));
}

inline void getLocalAddr(int sockfd, struct sockaddr_in6& localaddr)
{
  socklen_t addrlen = static_cast<socklen_t>(sizeof(localaddr));
  if (::getsockname(sockfd, sockaddr_cast(&localaddr), &addrlen) < 0) {
    perror("getsockname");
    abort();
  }
  assert(addrlen == sizeof(localaddr));
}

void getPeerAddr(int sockfd, struct sockaddr_in& peeraddr)
{
  socklen_t addrlen = static_cast<socklen_t>(sizeof(peeraddr));
  if (::getpeername(sockfd, sockaddr_cast(&peeraddr), &addrlen) < 0) {
    perror("getpeername");
    abort();
  }
  assert(addrlen == sizeof(peeraddr));
}

void getPeerAddr(int sockfd, struct sockaddr_in6& peeraddr)
{
  socklen_t addrlen = static_cast<socklen_t>(sizeof(peeraddr));
  if (::getpeername(sockfd, sockaddr_cast(&peeraddr), &addrlen) < 0) {
    perror("getpeername");
    abort();
  }
  assert(addrlen == sizeof(peeraddr));
}

bool isSelfConnect(int sockfd)
{
  struct sockaddr_in localaddr;
  getLocalAddr(sockfd, localaddr);
  if (localaddr.sin_family != AF_INET)
    return false;

  struct sockaddr_in peeraddr;
  getPeerAddr(sockfd, peeraddr);
  const struct sockaddr_in* laddr4 = reinterpret_cast<struct sockaddr_in*>(&localaddr);
  const struct sockaddr_in* raddr4 = reinterpret_cast<struct sockaddr_in*>(&peeraddr);
  return laddr4->sin_port == raddr4->sin_port && laddr4->sin_addr.s_addr == raddr4->sin_addr.s_addr;
}

class InetAddress
{
public:
  InetAddress()
    : _addrlen(sizeof(_addr6))
  {}

  InetAddress(const struct sockaddr_in& addr)
    : _addr(addr)
    , _addrlen(sizeof(_addr))
  {}

  InetAddress(const struct sockaddr_in6& addr6)
    : _addr6(addr6)
    , _addrlen(sizeof(_addr6))
  {}

  ~InetAddress() {}

  bool parseHost(const char* host, uint16_t port)
  {
    if (host == nullptr) {
      // 0.0.0.0
      _addr.sin_family = AF_INET;
      _addr.sin_port = htons(port);
      _addr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else if (strchr(host, ':')) {
      // ipv6
      _addr6.sin6_family = AF_INET6;
      _addr6.sin6_port = htons(port);
      if (::inet_pton(AF_INET6, host, &_addr6.sin6_addr) <= 0) {
        perror("inet_pton");
        abort();
      }
    } else {
      // ipv4 or hostname
      struct hostent hent;
      struct hostent* he = NULL;
      int herrno;
      char buf[8192];
      int rv = gethostbyname_r(host, &hent, buf, sizeof(buf), &he, &herrno);
      if (rv != 0) {
        dzlog_error("gethostbyname_r failed: %s", hstrerror(herrno));
        return false;
      }
      if (he->h_addrtype == AF_INET) {
        _addr.sin_family = AF_INET;
        _addr.sin_port = htons(port);
        memcpy(&_addr.sin_addr, he->h_addr, sizeof(struct in_addr));
      } else {
        _addr6.sin6_family = AF_INET6;
        _addr6.sin6_port = htons(port);
        memcpy(&_addr6.sin6_addr, he->h_addr, sizeof(struct in6_addr));
      }
    }
    return true;
  }

  sa_family_t family() const
  {
    return _family;
  }

  std::string ip() const
  {
    char buf[64];
    inet_ntop(_family,
              _family == AF_INET ? (void*)&_addr.sin_addr : (void*)&_addr6.sin6_addr,
              buf,
              sizeof(buf));
    return buf;
  }

  uint16_t port() const
  {
    return ntohs(_family == AF_INET ? _addr.sin_port : _addr6.sin6_port);
  }

  std::string toIpPort() const
  {
    return ip() + ":" + std::to_string(port());
  }

  const struct sockaddr* getSockAddr() const
  {
    return sockaddr_cast(&_addr);
  }

  struct sockaddr* getSockAddr()
  {
    return sockaddr_cast(&_addr);
  }

  socklen_t getAddrLen() const
  {
    return _addrlen;
  }

  socklen_t& getAddrLen()
  {
    return _addrlen;
  }

private:
  union
  {
    sa_family_t _family;
    struct sockaddr_in _addr;
    struct sockaddr_in6 _addr6;
  };
  socklen_t _addrlen;
};

inline int bind(int sockfd, const InetAddress& addr)
{
  return ::bind(sockfd, addr.getSockAddr(), addr.getAddrLen());
}

inline int connect(int sockfd, const InetAddress& addr)
{
  return ::connect(sockfd, addr.getSockAddr(), addr.getAddrLen());
}

inline int accept(int sockfd, InetAddress& addr)
{
  return ::accept(sockfd, addr.getSockAddr(), &addr.getAddrLen());
}

inline void getPeerAddr(int sockfd, InetAddress& addr)
{
  if (::getpeername(sockfd, addr.getSockAddr(), &addr.getAddrLen()) < 0) {
    perror("getpeername");
    abort();
  }
}

inline void getSockAddr(int sockfd, InetAddress& addr)
{
  if (::getsockname(sockfd, addr.getSockAddr(), &addr.getAddrLen()) < 0) {
    perror("getsockname");
    abort();
  }
}

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
    int rv = ::bind(_fd, addr);
    if (rv < 0) {
      perror("bind");
      abort();
    }
  }

  void listen()
  {
    int rv = ::listen(_fd);
    if (rv < 0) {
      perror("listen");
      abort();
    }
  }

  int accept(InetAddress& peeraddr)
  {
    return ::accept(_fd, peeraddr);
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