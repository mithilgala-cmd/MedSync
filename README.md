# MedSync

Educational C++17 simulation of a robotic medical device monitoring and
control system, built to practise object-oriented design, state machines,
multithreading, debugging, and testing.

> **Scope:** MedSync is a software simulation for learning and portfolio use.
> It is **not** a medical device, has **no** hardware integration, and is **not**
> clinically validated. Do not use it for patient care or real laboratory work.

## Current status

**Milestones 1–3 complete: single-device simulation with a supervisor, worker
threads, timed operations, and periodic sensor sampling.**

What works today:

- Validated state machine: `Idle`, `Homing`, `Ready`, `Positioning`, `Scanning`,
  `Fault`, `EmergencyStop`, `ShuttingDown`.
- Command API with explicit rejection reasons for invalid actions.
- Sample id, target, and configuration validation.
- Two-axis stage model with homing and deterministic stepping.
- Synthetic scan record produced from the sample id.
- Asynchronous `runCycle()`: the console submits a cycle and returns while a
  supervisor thread drives the state machine.
- One positioning worker and one scanning worker, each with a bounded job
  queue; producers never block.
- Simulated step and scan durations. Both waits are cancellable, so an
  emergency stop does not have to wait for the current delay to expire.
- A sensor worker samples synthetic temperature and force readings on a fixed
  period; `status` shows the latest snapshot.
- Cycle IDs correlate worker results, and cancellation generations stop stale
  jobs from starting after a reset.
- Emergency stop with a two-step recovery: `release`, then `reset`, then re-home.
- Thread-safe logger with monotonic elapsed timestamps.
- Idempotent shutdown that stops and joins every started thread.
- Console REPL with `wait` and `status` commands.
- Unit and concurrency tests, including repeated cycles, e-stop recovery,
  shutdown while busy, status queries racing with a running cycle, timed
  waits, and sensor snapshot tests.

Not implemented yet: sensor thresholds and a fault taxonomy with automatic
detection and recovery, fault injection, and a watchdog for dropped worker
events. See the roadmap below.

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

medsync> load S-001 40 -30
[+2 ms] [INFO] sample 'S-001' loaded; run the cycle to process it

medsync> run
[+3 ms] [INFO] state: Ready -> Positioning
[+3 ms] [INFO] cycle 1 submitted for sample 'S-001'
cycle started

medsync> status
state=Positioning position=(10, 0) homed=yes sample=S-001 (Received) temp=36.0C force=1.3N

medsync> wait
[+15 ms] [DEBUG] axis x -> 20
[+108 ms] [INFO] stage reached (40, -30)
[+108 ms] [INFO] state: Positioning -> Scanning
[+170 ms] [INFO] scan completed for sample 'S-001' (synthetic quality 81)
[+171 ms] [INFO] sample 'S-001' completed with quality 81
[+171 ms] [INFO] state: Scanning -> Ready
cycle complete

medsync> status
state=Ready position=(40, -30) homed=yes sample=none temp=35.1C force=9.8N

medsync> estop
[+171 ms] [WARN] EMERGENCY STOP: state Ready -> EmergencyStop

medsync> reset
error: emergency stop is still latched; release it first

medsync> release
[+171 ms] [INFO] emergency stop released; reset required before homing

medsync> reset
[+171 ms] [INFO] state: EmergencyStop -> Idle
[+171 ms] [INFO] reset complete; the stage must be homed before use
```

This is adapted from a real run with Debug logging enabled; repeated prompt and
debug lines are omitted. Worker logs can appear before the `run` command
returns or while the console is waiting. Sensor values depend on how many
readings have been generated, so they vary with timing.

## Architecture

```
main.cpp (console thread)
     |
     v
DeviceController ── owns ──> DeviceState (mutex + condition variable)
   |-- Position job queue ──> PositioningController worker
   |-- Scan job queue ──────> ScanningModule worker
   |-- DeviceEvent queue <── both workers
   |-- supervisor thread (consumes events, drives transitions)
   |-- SensorMonitor worker (periodic readings, pull snapshot)
   |-- SampleHandler
   `-- Logger& (shared, injected)
```

- The controller is the only writer of device state.
- Workers execute jobs and publish results; they never change device state.
- The supervisor correlates each event with the active cycle ID and ignores
  stale results.
- Console parsing lives only in `main.cpp`, so the core library is testable
  without stdin.
- No singletons, factories, or generic event bus.

Full module map, state diagram, transition table, lock ordering, and event flow:
[`docs/architecture.md`](docs/architecture.md).

## Roadmap

| Milestone | Scope | Status |
|---|---|---|
| 1 | State machine, OOP design, validation, console simulation | Complete (single-threaded) |
| 2 | Worker threads, bounded job/event queues, mutex/condition-variable synchronisation | Complete |
| 3 | Sensor readings, timed positioning and scanning, module communication | Complete |
| 4 | Fault detection, recovery, emergency-stop propagation, event logging | Next |
| 5 | Unit, integration, concurrency, and fault-injection tests | In progress (unit and concurrency tests) |
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
