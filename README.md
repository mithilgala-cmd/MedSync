# MedSync

Educational C++17 simulation of a multithreaded robotic medical device
monitoring and control system.

> **Scope:** MedSync is a software simulation built for learning and portfolio
> purposes. It is **not** a medical device, has **no** hardware integration, and
> is **not** clinically validated. Do not use it for patient care or any real
> laboratory workflow.

## Status

Milestone 1 (single-threaded device simulation) is in progress. There is no
multithreading, sensor simulation, or fault handling yet.

## Requirements

- Windows 11
- Visual Studio 2022 Build Tools with the C++ workload (MSVC 19.44 or newer)
- CMake 3.20 or newer
- Git

CMake ships with the Visual Studio Build Tools installation. If `cmake` is not
on your PATH, add:

```
C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin
```

## Build and run

```powershell
cmake --preset debug
cmake --build --preset debug
.\build\Debug\medsync.exe
```

## Test

GoogleTest is fetched automatically at configure time and pinned to a specific
release.

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

## Project layout

```
include/medsync/   Public headers
src/               Implementation and console entry point
tests/             Unit and integration tests
docs/              Architecture and design notes
```

## License

MIT. See [LICENSE](LICENSE).
