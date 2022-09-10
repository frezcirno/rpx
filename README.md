# RPX

# Features

- Non-blocking socket (i.e. using epoll)
- Reactor + threadpool model, one loop per thread

# Support Handlers

- `StaticHandler`:
  - Serve static resources, like nginx's `root` or `alias`

- `ProxyHandler`:
  - Act as a reverse proxy

# Requirements

- llhttp
- pcre2
- zlog

# Reference

- muduo
