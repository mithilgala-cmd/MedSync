# AGENTS.md

Instructions for coding agents working in this repository. Read this before
changing code.

## Project summary

MedSync is an educational C++17 simulation of a robotic medical device. It is
**not** a medical device and must never be described as clinically validated or
ready for real use.

## Required commands

Configure, build, and test before any commit:

```powershell
cmake --preset debug
cmake --build --preset debug
ctest --test-dir build -C Debug --output-on-failure
```

If `cmake` is not on PATH, use:
`C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`

## Conventions

- C++17 only. Do not introduce C++20 features (`std::jthread`, `std::format`,
  `std::span`, concepts).
- Namespace `medsync`. Headers use `.hpp` and `#pragma once`.
- Public headers live in `include/medsync/`, implementations in `src/`.
- The core library must not read from `stdin` or write to `stdout` directly.
  Only `Logger` writes, and `main.cpp` owns command parsing.
- Errors from user actions return `bool` plus a `std::string& error` reason.
  Exceptions are reserved for unrecoverable programming errors.
- Prefer the standard library. Add a dependency only with a written reason.
- No singleton, global mutable state, service locator, or generic event bus.
- No abstraction (interface, factory, base class) until it has at least two
  real users.

## Architecture rules

- `DeviceController` is the only writer of `DeviceState`.
- Worker threads (from Milestone 2) report events; they never change device
  state directly.
- All multi-field state is guarded by a mutex. The emergency-stop flag is the
  only atomic, because it is a single latching bit.
- Never call back into `DeviceController` while holding a component lock.
- Every condition-variable wait must use the predicate overload.
- `shutdown()` must be idempotent and must join every started thread.

## Honesty rules

- Do not report a feature as complete unless it is implemented and tested.
- Do not invent benchmarks, latency figures, or test results.
- Keep `docs/limitations.md` accurate when behaviour changes.

## Git

Work on short-lived branches. Make one logical change per commit and keep every
commit buildable and test-green. Do not rewrite published history.
