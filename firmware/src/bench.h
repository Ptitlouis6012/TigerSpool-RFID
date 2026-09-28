#pragma once
#include <Arduino.h>

// Code that exists for the bench and must never ship.
//
// A diagnostic added to chase a bug - a payload dumped to the console, a
// counter, a log kept in memory or sent somewhere - costs RAM, flash and time
// on every device in the field, and on a device that holds three TLS sessions
// at most, a few kilobytes of it is the difference between working and not.
// Removing it by hand before a release depends on somebody remembering.
//
// So it is not removed, it is not COMPILED: bench code sits inside
// `#if TIGERSPOOL_BENCH ... #endif`, or goes through BENCH_LOG(), and the
// switch is on only in the `tigerspool-bench` PlatformIO environment. The
// release and CI build `tigerspool`, where it is 0 and the code is not in the
// binary at all.
//
// scripts/check-bench-code.py holds the line: a `BENCH:` / `TEMP:` /
// `DO NOT SHIP` marker outside such a block, or the switch turned on for the
// production environment, fails verify.sh.
//
//   pio run -e tigerspool-bench -t upload     # a bench build
#ifndef TIGERSPOOL_BENCH
#define TIGERSPOOL_BENCH 0
#endif

#if TIGERSPOOL_BENCH
#define BENCH_LOG(...) Serial.printf(__VA_ARGS__)
#else
#define BENCH_LOG(...) do { } while (0)
#endif

// A pass profiler for the main loop. Mark the end of each section with
// BENCH_MARK("name"); BENCH_PASS(state) at the end of the pass logs every pass
// slower than BENCH_SLOW_MS with the time each section took - which is what
// "the screen feels slow" has to be turned into before anything is changed.
// Nothing of it exists in a production build.
#if TIGERSPOOL_BENCH
namespace bench {
constexpr uint32_t BENCH_SLOW_MS = 40;
struct Pass {
    uint32_t start = 0, last = 0;
    char     line[240];
    int      len = 0;
};
inline Pass& pass() { static Pass p; return p; }
inline void begin() {
    Pass& p = pass();
    p.start = p.last = millis();
    p.len = 0; p.line[0] = 0;
}
inline void mark(const char* name) {
    Pass& p = pass();
    const uint32_t now = millis(), d = now - p.last;
    p.last = now;
    if (d >= 5 && p.len < (int)sizeof(p.line) - 24)
        p.len += snprintf(p.line + p.len, sizeof(p.line) - p.len, " %s=%lu", name, (unsigned long)d);
}
inline void end(int state) {
    Pass& p = pass();
    const uint32_t total = millis() - p.start;
    if (total >= BENCH_SLOW_MS)
        Serial.printf("[bench] slow pass %lums state=%d:%s\n", (unsigned long)total, state, p.line);
}
}  // namespace bench
#define BENCH_BEGIN()      bench::begin()
#define BENCH_MARK(name)   bench::mark(name)
#define BENCH_PASS(state)  bench::end(state)
#else
#define BENCH_BEGIN()      do { } while (0)
#define BENCH_MARK(name)   do { } while (0)
#define BENCH_PASS(state)  do { } while (0)
#endif
