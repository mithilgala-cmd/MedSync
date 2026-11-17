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
changes `DeviceState`.

**Consequence:** One writer, one mutex, one ordering point. The trade-off is
that safety reactions wait for the controller's next observation. Emergency
stop is therefore handled by the latching atomic flag that workers check
directly, not by a state transition.

## ADR-003: No task scheduler or thread pool

**Context:** The roadmap mentions a `TaskScheduler`. The workload is three fixed
activities: positioning, scanning, and sensing.

**Decision:** Use three explicit, long-lived worker threads with predicate-based
waiting. No generic pool, no work stealing, no scheduler abstraction.

**Alternatives:** a thread pool with a job queue. It would add lifetime,
shutdown, and fairness problems while the maximum useful parallelism stays at
three.

**Revisit when:** the number of independent jobs becomes dynamic, or profiling
shows the fixed workers are the bottleneck.

## ADR-004: Bounded queue with rejection, not unbounded growth

**Context:** Samples arrive from the console; processing is slower.

**Decision (Milestone 2):** a bounded queue protected by one mutex and two
condition variables. When full, the submission is rejected with an explicit
error.

**Alternatives:** unbounded queue (memory growth, latency), blocking the
producer (freezes the console UI).

**Cost:** callers must handle rejection. This is deliberate: a medical workflow
should not silently discard or endlessly buffer work.

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
