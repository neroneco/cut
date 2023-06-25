# CUT - CPU Usage Tracker

## Description:

Simple console application to track cpu usage.

To synchronize threads, ***mutexes and conditional variables*** from pthread library were used.
In order to queue data between threads, ***linked list based queue*** was implemented.

---

## Used:
* standard: `C99`
* threads: `pthread library`
* tests: `with assert() function`
---

## Task lisk:

- [ *DONE* ] `Reader`
- [ *DONE* ] `Analyzer`
- [ *DONE* ] `Printer`
- [ *DONE* ] `Watchdog`
- [ *DONE* ] `Logger`

---

## Files:

* `src/cut.c`  -> main aplication source code
* `src/queue.c .h`  -> queues implementation
* `src/analyzer.c .h`  -> /proc/stat data analyzing functions
* `src/logger.c .h`  -> logging functions
* `tests/test_analyzer.c`  -> analyzing functions test
* `tests/test_queues.c`  -> queues test

---

## Instalation:
* Software requirements:
    - `gcc` or `clang` compiler
    - `make` build system
    - `CC` environment variable must be set to used compiler
* Commands:
  - `make` - compile all files
  - `make dbg` - compile with debug symbols
  - `make test` - compile and run tests

---

## Run:
* `./bin/cut`  -> main aplication
* `./debug/cut`  -> main aplication built with debug symbols
