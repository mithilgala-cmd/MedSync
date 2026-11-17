# Limitations

These are deliberate boundaries, not hidden defects. Keep this file accurate
when behaviour changes.

## Scope and claims

- MedSync is a **software simulation for education and portfolio use**.
- It is **not a medical device** and is **not clinically validated**.
- It has no hardware integration: no motors, cameras, sensors, or emergency-stop
  circuit.
- No result produced by this project has any diagnostic or clinical meaning.

## Current implementation (Milestone 2)

- The controller, one positioning worker, one scanning worker, and an internal
  supervisor thread exist. Thread scheduling remains ordinary Windows
  preemptive scheduling; there are no real-time guarantees.
- `runCycle()` is asynchronous. It returns after the move is queued and the
  device enters `Positioning`; callers must use `wait`/`waitForCycleCompletion`
  or poll `status` to observe completion.
- One sample is active at a time. The bounded queues hold jobs and events, not a
  backlog of samples. A second `load` before the current sample completes is
  rejected.
- Two-axis stage only; no Z axis, no acceleration, no real motion dynamics.
- Movement is instantaneous in wall-clock time: the stepping loop performs no
  sleeping. Simulated motion duration is planned for Milestone 3.
- The scan result is a deterministic hash of the sample id. It is not a model of
  any imaging modality, and scan duration is not simulated yet.
- `Fault` is reached through defensive internal failure paths (move failure,
  scan failure, or failure to dispatch the next job). There is no sensor
  threshold, fault taxonomy, automatic recovery, or fault injection yet.
- Emergency stop is a software latch. Workers check an atomic abort flag before
  starting a job and before each coordinate step, so a stop takes effect at a
  step boundary, not instantly. Real emergency stop must be hard-wired and
  independent of application software.
- The worker-to-supervisor event queue is bounded. If it is full, the worker
  drops the event and logs a warning. A dropped completion leaves the cycle
  waiting until an operator stops or resets the device; there is no watchdog or
  timeout policy yet.
- `shutdown()` is idempotent for sequential calls. Concurrent calls from
  multiple threads are outside the supported model.
- No persistence: logs go to the console and are lost on exit.
- No configuration file; defaults are compiled in and validated at start-up.
- No support for multiple devices or multiple simultaneous samples.

## Platform and verification

- Developed and tested on Windows 11 with MSVC 19.44 and CMake 3.31.
- No real-time guarantees. Windows scheduling is not deterministic, and
  millisecond-level latency claims would be meaningless.
- ThreadSanitizer is not available with MSVC. The concurrency suite is repeated
  stress testing, not a proof of race freedom. Data-race checking requires a
  separate WSL/Linux build with TSan.
- No performance benchmarks have been run, and none are claimed.
- GoogleTest is fetched from GitHub at configure time and pinned to `v1.18.0`;
  offline builds require a local cache or a vendored copy.

## Non-claims for the future

Passing the project's own tests demonstrates that the simulation behaves as
specified. It does not demonstrate hardware safety, clinical suitability,
regulatory compliance, or numerical accuracy of any scan.
