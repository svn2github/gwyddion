#ifndef __GWY_BENCHMARK_TIMER_H__
#define __GWY_BENCHMARK_TIMER_H__ 1

#include <glib.h>
#include <sys/time.h>
#include <sys/resource.h>

static struct {
    GTimer *gtimer;
    gdouble gtime;
    struct rusage start, stop;
} _gwy_benchmark_timer;

static inline void
gwy_benchmark_timer_start(void)
{
    if (!_gwy_benchmark_timer.gtimer)
        _gwy_benchmark_timer.gtimer = g_timer_new();
    else
        g_timer_start(_gwy_benchmark_timer.gtimer);
    getrusage(RUSAGE_SELF, &_gwy_benchmark_timer.start);
}

// Does not actually stop the timer, just marks down the current time.
static inline void
gwy_benchmark_timer_stop(void)
{
    _gwy_benchmark_timer.gtime = g_timer_elapsed(_gwy_benchmark_timer.gtimer, NULL);
    getrusage(RUSAGE_SELF, &_gwy_benchmark_timer.stop);
}

static inline gdouble
gwy_benchmark_timer_get_user(void)
{
    struct timeval start = _gwy_benchmark_timer.start.ru_utime;
    struct timeval stop = _gwy_benchmark_timer.stop.ru_utime;
    return ((stop.tv_sec - start.tv_sec) + 1e-6*(stop.tv_usec - start.tv_usec));
}

static inline gdouble
gwy_benchmark_timer_get_system(void)
{
    struct timeval start = _gwy_benchmark_timer.start.ru_stime;
    struct timeval stop = _gwy_benchmark_timer.stop.ru_stime;
    return ((stop.tv_sec - start.tv_sec) + 1e-6*(stop.tv_usec - start.tv_usec));
}

static inline gdouble
gwy_benchmark_timer_get_total(void)
{
    return gwy_benchmark_timer_get_user() + gwy_benchmark_timer_get_system();
}

static inline gdouble
gwy_benchmark_timer_get_wall(void)
{
    return _gwy_benchmark_timer.gtime;
}

#endif
