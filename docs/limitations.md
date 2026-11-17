# Limitations

These are deliberate boundaries, not hidden defects. Keep this file accurate
when behaviour changes.

## Scope and claims

- MedSync is a **software simulation for education and portfolio use**.
- It is **not a medical device** and is **not clinically validated**.
- It has no hardware integration: no motors, cameras, sensors, or emergency-stop
  circuit.
- No result produced by this project has any diagnostic or clinical meaning.

## Current implementation (Milestone 1)

- Single-threaded. No `std::thread`, mutex, condition variable, or atomic is used
  yet; concurrency is planned for Milestone 2.
- One active sample at a time. There is no queue and no backpressure policy yet.
- Two-axis stage only; no Z axis, no acceleration, no real motion dynamics.
- Movement is instantaneous in wall-clock time: the stepping loop performs no
  sleeping. Simulated duration arrives with the worker threads.
- The scan result is a deterministic hash of the sample id. It is not a model of
  any imaging modality.
- `Fault` is reachable only through defensive internal checks. There is no fault
  taxonomy, sensor threshold, or fault injection yet.
- Emergency stop is a software latch. Real emergency stop must be hard-wired and
  independent of application software.
- No persistence: logs go to the console and are lost on exit.
- No configuration file; defaults are compiled in and validated at start-up.
- No support for multiple devices or multiple simultaneous samples.

## Platform and verification

- Developed and tested on Windows 11 with MSVC 19.44 and CMake 3.31.
- No real-time guarantees. Windows scheduling is not deterministic, and
  millisecond-level latency claims would be meaningless.
- ThreadSanitizer is not available with MSVC. If concurrency tests are added,
  data-race checking requires a separate WSL/Linux build with TSan.
- No performance benchmarks have been run, and none are claimed.
- GoogleTest is fetched from GitHub at configure time and pinned to `v1.18.0`;
  offline builds require a local cache or a vendored copy.

## Non-claims for the future

Passing the project's own tests demonstrates that the simulation behaves as
specified. It does not demonstrate hardware safety, clinical suitability,
regulatory compliance, or numerical accuracy of any scan.
