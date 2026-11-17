# MedSync

Educational C++17 simulation of a robotic medical device monitoring and
control system, built to practise object-oriented design, state machines,
multithreading, debugging, and testing.

> **Scope:** MedSync is a software simulation for learning and portfolio use.
> It is **not** a medical device, has **no** hardware integration, and is **not**
> clinically validated. Do not use it for patient care or real laboratory work.

## Current status

**Milestone 1 — single-threaded device simulation: complete.**

What works today:

- Validated state machine: `Idle`, `Homing`, `Ready`, `Positioning`, `Scanning`,
  `Fault`, `EmergencyStop`, `ShuttingDown`.
- Command API with explicit rejection reasons for invalid actions.
- Sample id, target, and configuration validation.
- Two-axis stage model with homing and deterministic stepping.
- Synthetic scan record produced from the sample id.
- Emergency stop with a two-step recovery: `release`, then `reset`, then re-home.
- Thread-safe logger with monotonic elapsed timestamps.
- Console REPL.
- Unit tests for the state table, controller, validation, and components.

Not implemented yet: worker threads, mutexes, condition variables, the sample
queue, sensor simulation, fault taxonomy, fault injection, and concurrency
tests. See the roadmap below.

## Requirements

- Windows 11
- Visual Studio 2022 Build Tools with the C++ workload (MSVC 19.44 or newer)
- CMake 3.20 or newer
- Git

CMake ships with the Visual Studio Build Tools. If `cmake` is not on your PATH,
add:

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

GoogleTest is fetched at configure time and pinned to `v1.18.0`.

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

## Example session

```
medsync> home
[+2 ms] [INFO] state: Idle -> Homing
[+2 ms] [INFO] stage homed at (0, 0)
[+2 ms] [INFO] state: Homing -> Ready

medsync> load S-001 10 20
[+3 ms] [INFO] sample 'S-001' accepted for target (10, 20)

medsync> run
[+3 ms] [INFO] state: Ready -> Positioning
[+3 ms] [INFO] stage reached (10, 20)
[+3 ms] [INFO] state: Positioning -> Scanning
[+3 ms] [INFO] scan completed for sample 'S-001' (synthetic quality 81)
[+3 ms] [INFO] sample 'S-001' completed with quality 81
[+3 ms] [INFO] state: Scanning -> Ready

medsync> estop
[+4 ms] [WARN] EMERGENCY STOP: state Ready -> EmergencyStop

medsync> reset
error: emergency stop is still latched; release it first

medsync> release
[+5 ms] [INFO] emergency stop released; reset required before homing

medsync> reset
[+5 ms] [INFO] state: EmergencyStop -> Idle
[+5 ms] [INFO] reset complete; the stage must be homed before use
```

In the actual console the timestamps are millisecond values; they are shortened
here for readability.

## Architecture

```
main.cpp (console REPL)
     |
     v
DeviceController ── owns ──> DeviceState
   |-- PositioningController
   |-- ScanningModule
   |-- SampleHandler
   `-- Logger& (shared, injected)
```

- The controller is the only writer of device state.
- Components report results; they never change device state.
- Console parsing lives only in `main.cpp`, so the core library is testable
  without stdin.
- No singletons, factories, or generic event bus.

Full module map, state diagram, transition table, and the planned threading
model: [`docs/architecture.md`](docs/architecture.md).

## Roadmap

| Milestone | Scope | Status |
|---|---|---|
| 1 | State machine, OOP design, validation, console simulation | Complete (single-threaded) |
| 2 | Worker threads, sample queue, mutex/condition-variable synchronisation | Planned |
| 3 | Sensor readings, timed positioning and scanning, module communication | Planned |
| 4 | Fault detection, recovery, emergency-stop propagation, event logging | Planned |
| 5 | Unit, integration, concurrency, and fault-injection tests | In progress (unit tests only) |
| 6 | Final architecture diagrams, build/test docs, design notes | In progress |

## Design notes

- Decisions and alternatives: [`docs/design-decisions.md`](docs/design-decisions.md)
- Explicit non-claims and known gaps: [`docs/limitations.md`](docs/limitations.md)

## Project layout

```
include/medsync/   Public headers
src/               Implementation and console entry point
tests/             GoogleTest unit tests
docs/              Architecture, decisions, limitations
CMakeLists.txt     Build configuration
CMakePresets.json  Configure/build/test presets
AGENTS.md          Instructions for coding agents in this repository
```

## License

MIT. See [LICENSE](LICENSE).
