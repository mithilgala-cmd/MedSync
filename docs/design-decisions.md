# Design Decisions

Short records of decisions that are easy to defend in review and interviews.
Each entry states the context, the decision, the alternatives, and the cost.

## ADR-001: State machine as an enum plus pure functions

**Context:** The device has eight states and strict transition rules. Invalid
transitions must be rejected, not silently forced.

**Decision:** `DeviceState` is an `enum class`. `canTransition(from, to)` is a
pure function with a `switch` over the source state. `DeviceController` performs
the transition, logs it, and rejects illegal ones.

**Alternatives:** a class per state, a table of `std::function`s, or a library
such as Boost.SML. All add indirection or a dependency for a table that fits in
one screen.

**Cost:** adding a state requires editing the enum and the switch. The compiler
does not prove the switch is exhaustive; the tests cover the table.

## ADR-002: The controller is the only writer of device state

**Context:** Once worker threads exist, multiple modules could try to react to
events and update the device.

**Decision:** Components report results and faults; only `DeviceController`
changes `DeviceState`. Workers publish events to a bounded queue, and a
supervisor thread inside the controller consumes them.

**Consequence:** One writer, one mutex, one ordering point. The trade-off is
that normal state changes wait for the supervisor. Emergency stop does not wait:
the controller forces `EmergencyStop` immediately and additionally sets a
per-worker atomic abort flag that the workers poll without taking the controller
mutex. The latch itself is guarded by the controller mutex because it is only
read and written through controller entry points.

## ADR-003: No task scheduler or thread pool

**Context:** The roadmap originally mentioned a `TaskScheduler`. The workload is
a fixed set of activities: positioning, scanning, and later sensing.

**Decision:** Use explicit, long-lived worker threads with predicate-based
waiting. Milestone 2 has two workers (positioning and scanning) plus one
supervisor thread. The sensor worker arrives with Milestone 3. No generic pool,
no work stealing, no scheduler abstraction.

**Alternatives:** a thread pool with a job queue. It would add lifetime,
shutdown, and fairness problems while the maximum useful parallelism stays
small and fixed.

**Revisit when:** the number of independent jobs becomes dynamic, or profiling
shows the fixed workers are the bottleneck.

## ADR-004: Bounded queue with rejection, not unbounded growth

**Context:** Samples arrive from the console; processing is slower.

**Decision:** `BoundedQueue<T>` uses one mutex and one condition variable.
Producers call `tryPush()`, which never blocks; when the queue is full or closed
the submission is rejected with an explicit error. Consumers call `pop()`, which
blocks until an item arrives or the queue is closed and drained.

**Alternatives:** an unbounded queue (memory growth, latency), a blocking
producer (freezes the console), a second condition variable for "not full"
(unnecessary when producers never wait).

**Cost:** callers must handle rejection. This is deliberate: a medical workflow
should not silently discard or endlessly buffer work. One exception is the
worker-to-supervisor event queue, where a full queue currently drops the event
and logs a warning; the resulting liveness gap is documented in
`limitations.md`.

## ADR-005: Error codes for operator actions, exceptions for bugs

**Context:** Most failures here are expected: wrong state, bad id, out-of-range
target.

**Decision:** public commands return `bool` plus a `std::string& error` reason.
Exceptions are reserved for unrecoverable programming errors.

**Alternatives:** exceptions everywhere (hard to reason about across threads),
`std::expected` (C++23, unavailable in C++17).

**Cost:** callers must check returns. Tests assert the messages.

## ADR-006: Inject configuration and logger; no singletons

**Context:** Tests need fast, deterministic behaviour and no global state.

**Decision:** `DeviceController` takes `Config` by value and `Logger&` by
reference; it passes them to components. `Config` is validated once.

**Alternatives:** a global `Config::instance()` and a static logger. Both make
tests order-dependent and complicate threading.

**Cost:** a few more constructor parameters; explicit ownership.

## ADR-007: Synthetic scan score derived from the sample id

**Context:** Tests need reproducible scan output, and the project must not
pretend to model real imaging.

**Decision:** use FNV-1a over the sample id to derive a score in [80, 100].
Label the output as synthetic in code, logs, and docs.

**Alternatives:** `rand()` (not reproducible across runs), `std::hash`
(implementation-defined across standard libraries), a real signal model (out of
scope).

**Cost:** the number is meaningless as a measurement. It exists to exercise
data flow, not to imply accuracy.

## ADR-008: Two-step emergency-stop recovery

**Context:** A single `reset` that also clears the stop cause hides whether the
hazard is gone.

**Decision:** `releaseEmergencyStop()` clears the cause; `reset()` then returns
to `Idle` and invalidates homing. Reset is refused while the stop is latched.

**Alternatives:** one reset command (simpler, weaker safety story).

**Cost:** two operator actions. This mirrors the physical reset-then-software-
reset sequence and gives the tests something meaningful to assert.

## ADR-009: No Z axis in the current model

**Context:** Real stages may have three axes, but the scan workflow only needs
planar positioning.

**Decision:** `Position` has `x` and `y` only. Adding `z` later touches one type
and the bounds check.

**Cost:** less realistic mechanical model; less code to explain.

## ADR-010: A supervisor thread instead of workers changing state

**Context:** Once positioning and scanning run on their own threads, either the
workers could update `DeviceState` themselves, or some other component could
own the transitions.

**Decision:** Workers publish `DeviceEvent` values. A supervisor thread inside
`DeviceController` is the only consumer. It checks the expected state and cycle
ID, then performs the transition and submits the next job.

**Alternatives:** let each worker lock the controller and update state. That
spreads transition logic across modules and makes the lock order harder to
reason about. A separate state-machine object would add another owner without
removing the need for one serialization point.

**Cost:** every cycle pays one extra event hop and a context switch. In exchange,
there is exactly one place where state changes, and workers only depend on
queues and immutable data.

## ADR-011: Cycle IDs plus cancellation generations

**Context:** A worker can finish after an emergency stop, or a job can be popped
just before a stop and start just after a reset. A late result must not complete
a later cycle.

**Decision:** Every job carries a monotonic cycle ID; every result echoes it.
The supervisor ignores results whose ID does not match `activeCycleId_`. Each
worker also keeps a cancellation generation. `cancelPending()` increments it and
clears the queue; a job whose captured generation is stale is discarded instead
of executed.

**Alternatives:** rely only on the state check. The state can return to
`Positioning` for a new cycle before a stale event arrives, so state alone is
not enough. Add a timestamp to every result: clock ordering adds complexity for
no benefit here.

**Cost:** two integers per job and event, plus the rule that stale results are
ignored rather than reported to the operator.

## ADR-012: Asynchronous run with explicit waits

**Context:** If `runCycle()` blocked until scanning finished, the console could
not accept `estop`, `status`, or `quit` during a cycle.

**Decision:** `runCycle()` submits the move and returns. The console offers
`wait`, and tests use `waitForCycleCompletion(timeout)` or
`waitForState(state, timeout)`. Both use the predicate overload of
`condition_variable::wait_for`.

**Alternatives:** a blocking `runCycle()` with a separate command thread
(splits input handling from output and complicates the REPL), or polling in the
console (wastes time and still needs synchronization).

**Cost:** callers must decide when to wait. A cycle that faults returns `false`
from `waitForCycleCompletion()` rather than throwing.

## ADR-013: One active sample, job queues only

**Context:** The original roadmap mentioned a sample queue. The workflow,
however, has one table position and one active scan.

**Decision:** `SampleHandler` keeps a single active sample. The bounded queues
hold move jobs, scan jobs, and worker events, not a backlog of samples. A second
`load` while a sample is active is rejected.

**Alternatives:** a bounded pipeline of samples with one slot per stage. That
would require admission control, per-sample state isolation, and a much larger
test surface; it is not needed to demonstrate the concurrency model.

**Cost:** no throughput pipelining. The project stays a single-sample
simulation until a future milestone changes the requirement.
