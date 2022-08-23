#ifndef _TIME_HPP_
#define _TIME_HPP_

#include <stdint.h>
#include <sys/time.h>

class Time
{
public:
  Time(int64_t ms)
    : _ms(ms)
  {}
  ~Time() {}

  static Time now()
  {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return Time(tv.tv_sec * 1000000 + tv.tv_usec);
  }

  Time operator+(const Time& t) const
  {
    return Time(_ms + t._ms);
  }

  Time operator-(const Time& t) const
  {
    return Time(_ms - t._ms);
  }

  operator int64_t() const
  {
    return _ms;
  }

  Time offsetBy(double sec) const
  {
    return Time(_ms + (int64_t)(sec * 1000000));
  }

  operator timespec() const
  {
    lldiv_t di = lldiv(static_cast<long long>(_ms), 1000000LL);
    struct timespec ts;
    ts.tv_sec = static_cast<time_t>(di.quot);
    ts.tv_nsec = static_cast<long>(di.rem * 1000);
    return ts;
  }

private:
  int64_t _ms;
};

#endif
