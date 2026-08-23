/* Plan 9 native shim: wall clock via time()/nsec(). */
#ifndef KRYON_PLAN9_SHIM_TIME_H
#define KRYON_PLAN9_SHIM_TIME_H

#include "kryon_plan9_libc.h"

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

static void kryon_plan9_tm_from_plan9(struct tm *out, const Tm *in);

static struct tm*
kryon_plan9_localtime(const time_t *tp)
{
    static struct tm out;
    Tm *in;

    if(tp == nil)
        return nil;
    in = localtime(*tp);
    if(in == nil)
        return nil;
    out.tm_sec = in->sec;
    out.tm_min = in->min;
    out.tm_hour = in->hour;
    out.tm_mday = in->mday;
    out.tm_mon = in->mon;
    out.tm_year = in->year;
    out.tm_wday = in->wday;
    out.tm_yday = in->yday;
    out.tm_isdst = 0;
    return &out;
}

static struct tm*
kryon_plan9_gmtime(const time_t *tp)
{
    static struct tm out;
    Tm *in;

    if(tp == nil)
        return nil;
    in = gmtime(*tp);
    if(in == nil)
        return nil;
    kryon_plan9_tm_from_plan9(&out, in);
    return &out;
}

static struct tm*
kryon_plan9_gmtime_r(const time_t *tp, struct tm *out)
{
    Tm *in;

    if(tp == nil || out == nil)
        return nil;
    in = gmtime(*tp);
    if(in == nil)
        return nil;
    kryon_plan9_tm_from_plan9(out, in);
    return out;
}

static void
kryon_plan9_tm_to_plan9(const struct tm *in, Tm *out)
{
    memset(out, 0, sizeof(*out));
    out->sec = in->tm_sec;
    out->min = in->tm_min;
    out->hour = in->tm_hour;
    out->mday = in->tm_mday;
    out->mon = in->tm_mon;
    out->year = in->tm_year;
    out->wday = in->tm_wday;
    out->yday = in->tm_yday;
}

static void
kryon_plan9_tm_from_plan9(struct tm *out, const Tm *in)
{
    out->tm_sec = in->sec;
    out->tm_min = in->min;
    out->tm_hour = in->hour;
    out->tm_mday = in->mday;
    out->tm_mon = in->mon;
    out->tm_year = in->year;
    out->tm_wday = in->wday;
    out->tm_yday = in->yday;
    out->tm_isdst = 0;
}

static time_t
kryon_plan9_mktime(struct tm *tp)
{
    Tm in;
    Tm *normalized;
    time_t seconds;

    if(tp == nil)
        return (time_t)-1;
    kryon_plan9_tm_to_plan9(tp, &in);
    seconds = tm2sec(&in);
    normalized = localtime(seconds);
    if(normalized != nil)
        kryon_plan9_tm_from_plan9(tp, normalized);
    return seconds;
}

static size_t
kryon_plan9_strftime(char *dst, size_t dst_size, const char *format, const struct tm *tp)
{
    static char *weekdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static char *months[] = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };
    int n;

    if(dst == nil || dst_size == 0 || format == nil || tp == nil)
        return 0;
    if(strcmp(format, "%a") == 0)
        n = snprint(dst, dst_size, "%s", weekdays[(tp->tm_wday + 7) % 7]);
    else if(strcmp(format, "%B %Y") == 0)
        n = snprint(dst, dst_size, "%s %d", months[(tp->tm_mon + 12) % 12], tp->tm_year + 1900);
    else if(strcmp(format, "%Y-%m-%dT%H:%M:%SZ") == 0)
        n = snprint(dst, dst_size, "%04d-%02d-%02dT%02d:%02d:%02dZ",
            tp->tm_year + 1900, tp->tm_mon + 1, tp->tm_mday,
            tp->tm_hour, tp->tm_min, tp->tm_sec);
    else
        n = snprint(dst, dst_size, "%04d-%02d-%02d",
            tp->tm_year + 1900, tp->tm_mon + 1, tp->tm_mday);
    if(n < 0 || (size_t)n >= dst_size)
        return 0;
    return (size_t)n;
}

static double
kryon_plan9_difftime(time_t end, time_t begin)
{
    return (double)(end - begin);
}

#define localtime(tp) kryon_plan9_localtime(tp)
#define gmtime(tp) kryon_plan9_gmtime(tp)
#define gmtime_r(tp, out) kryon_plan9_gmtime_r(tp, out)
#define mktime(tp) kryon_plan9_mktime(tp)
#define strftime(dst, dst_size, format, tp) kryon_plan9_strftime(dst, dst_size, format, tp)
#define difftime(end, begin) kryon_plan9_difftime(end, begin)

#endif
