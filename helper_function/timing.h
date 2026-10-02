/*
 * timing.h  -  PM Mini Project helper (works on Windows, macOS and Raspberry Pi / Linux)
 *
 *   uint64_t now_ns(void)     current time in nanoseconds (only differences are meaningful)
 *   void     sleep_ms(int ms) pause for about ms milliseconds
 *
 * Tip: #include "timing.h" before any other header.
 */
#ifndef TIMING_H
#define TIMING_H

#if defined(__linux__) && !defined(_POSIX_C_SOURCE) && !defined(_GNU_SOURCE)
#define _POSIX_C_SOURCE 200809L          /* makes clock_gettime visible on Linux */
#endif

#include <stdint.h>

#ifdef _WIN32
  #include <windows.h>

  static inline uint64_t now_ns(void) {
      static LARGE_INTEGER freq;
      static int ready = 0;
      LARGE_INTEGER count;
      if (!ready) { QueryPerformanceFrequency(&freq); ready = 1; }
      QueryPerformanceCounter(&count);
      /* split into whole seconds + remainder so the result never overflows */
      uint64_t f = (uint64_t)freq.QuadPart, c = (uint64_t)count.QuadPart;
      return (c / f) * 1000000000ULL + (c % f) * 1000000000ULL / f;
  }

  static inline void sleep_ms(int ms) { Sleep((DWORD)ms); }

#else   /* macOS, Linux, Raspberry Pi */
  #include <time.h>

  static inline uint64_t now_ns(void) {
      struct timespec t;
      clock_gettime(CLOCK_MONOTONIC, &t);
      return (uint64_t)t.tv_sec * 1000000000ULL + (uint64_t)t.tv_nsec;
  }

  static inline void sleep_ms(int ms) {
      struct timespec t;
      t.tv_sec  = ms / 1000;
      t.tv_nsec = (long)(ms % 1000) * 1000000L;
      nanosleep(&t, NULL);
  }
#endif

#endif /* TIMING_H */
