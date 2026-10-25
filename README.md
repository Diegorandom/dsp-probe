# dsp-probe

**Real-time-safe probes for testing DSP code.** A header-only C++17 library that
lets you measure what a piece of audio DSP is actually doing — without breaking
the one rule the audio thread has: never allocate, never lock, never block.

Extracted from **RaiderPro**, a commercial audio plugin, where it ran inside the
DSP functional tests before being made standalone.

`C++17` · `header-only` · `zero dependencies` · `MIT`

---

## The problem

A DSP bug is rarely a crash. It is a number that drifts: a gain that is 0.3 dB
wrong, a limiter that attenuates one block too late, a window that advances one
sample too far. To find it you have to *see* the numbers the DSP produces while
it runs.

And the audio thread will not let you print them.

| Constraint | Why |
|---|---|
| No `malloc` | allocation can take a lock or fault; either blows the deadline |
| No locks | priority inversion — the audio thread must never wait on anything |
| No `printf` | it formats in user space and takes internal locks |
| No debugger stepping | stepping changes the timing you are trying to measure |

So the values have to be handed to another thread through a lock-free queue and
written to a file you can open in a spreadsheet. **That hand-off is the whole
engineering problem**, and this library is that hand-off, tested.

---

## What this demonstrates

### C++

| Concept | Where |
|---|---|
| Real-time safety: `noexcept`, bounded work, all allocation hoisted to the constructor | `Probe::push` |
| Lock-free single-producer/single-consumer ring buffer with acquire/release ordering | `probe.hpp` |
| Cache-line separation so producer and consumer never share a line (false sharing) | `alignas(64)` on `read_`/`write_` |
| `std::variant` + `std::visit` + `if constexpr` for typed, total, allocation-free formatting | `property.hpp` |
| Variadic templates + fold expressions to write a row of N typed values | `Probe::push` |
| `static_assert` producing a readable error at the call site, not inside `std::variant` | `is_property_type_v` |
| Dependency inversion: a `Sink` interface decoupling the real-time core from all I/O | `sink.hpp` |
| Header-only library design: `INTERFACE` target, presets, `install()`/`export()` | `CMakeLists.txt` |

### DSP

| Concept | Where |
|---|---|
| Block processing: the unit of measurement is the block, not the sample | `examples/csv_from_sine.cpp` |
| Backpressure policy — **drop and count, never block** | `Probe::push`, `dropped()` |
| Why the *incoming* row is the one dropped, keeping the queue ordered and intact | `test_probe.cpp` |
| Peak and RMS per block as the primitive measurements | example |
| The thread boundary as a design constraint, not an afterthought | throughout |

---

## The hot path

```cpp
// AUDIO THREAD. No allocation, no locks, no exceptions, bounded time.
template <typename... Args>
void push(const Args&... args) noexcept
{
    static_assert((is_property_type_v<Args> && ...),
                  "Probe columns accept only float, int, double or bool");

    if (!running_.load(std::memory_order_acquire))
        return;

    const auto writeIndex = write_.load(std::memory_order_relaxed);
    const auto readIndex  = read_.load(std::memory_order_acquire);

    if (writeIndex - readIndex >= capacity_) {          // full: drop, and count it
        dropped_.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    Row& slot = rows_[writeIndex & mask_];
    std::size_t i = 0;
    ((slot[i++] = Property{args}), ...);                // fold over the columns

    write_.store(writeIndex + 1, std::memory_order_release);
}
```

Two decisions are visible in those fifteen lines, and both are the point:

- **It drops the incoming row instead of blocking.** On the audio thread the only
  correct response to backpressure is to lose a row rather than miss a deadline.
  Dropping on *write* keeps everything already queued ordered and consistent —
  the alternative would corrupt the history you are about to analyse.
- **The loss is counted.** `dropped()` is what stops that policy from becoming
  silent data loss. A test that ignores it is a test that might be lying.

```cpp
// NON-AUDIO THREAD. Any timer will do.
probe.drain(sink);

// When the test ends:
probe.stop(sink);                // disarm + drain the remainder
assert(probe.dropped() == 0);    // if this fails, the CSV is missing rows
```

---

## Getting started

```cmake
include(FetchContent)
FetchContent_Declare(dsp-probe
  GIT_REPOSITORY https://github.com/Diegorandom/dsp-probe.git
  GIT_TAG main)
FetchContent_MakeAvailable(dsp-probe)

target_link_libraries(your_test PRIVATE dsp_probe::dsp_probe)
```

```cpp
#include "dsp_probe/csv_file_sink.hpp"
#include "dsp_probe/probe.hpp"

dsp_probe::Probe probe({"block", "peak", "gain_reduction_db"}, 512);
dsp_probe::CsvFileSink sink("limiter.csv", probe.columns());
probe.start();

probe.push(blockIndex, peak, gainReductionDb);   // audio thread
probe.drain(sink);                               // any other thread
```

`drain()` writes to any `Sink`. `MemorySink` keeps rows in memory, so unit tests
can assert on what a probe produced without touching a filesystem.

### API

| Member | Thread | Notes |
|---|---|---|
| `Probe(columns, capacity)` | any | Allocates everything. Capacity rounds up to a power of two (8192 default) |
| `start()` / `stop()` | any | `push()` is inert until `start()` |
| `push(args...)` | **audio** | `noexcept`. Wrong arity counts as a drop, never writes a short row |
| `drain(sink, maxRows)` | non-audio | Returns rows written |
| `stop(sink)` | non-audio | Disarms, drains the remainder, flushes |
| `dropped()` / `pending()` | any | Rows lost; rows queued |

Columns hold `float`, `int`, `double` or `bool` — a closed set, enforced with a
`static_assert`. Floating point is written with **6 decimals**, matching what the
original plugin logger produced, so reference CSVs stay comparable across the
migration.

---

## Where it came from

This is the `CSVLogger.h` that **RaiderPro** used in its DSP functional tests,
extracted into a standalone, testable library. The behaviour was proven in the
plugin before the extraction; what is new here is the packaging:

| Original (inside the plugin) | Here |
|---|---|
| `juce::AbstractFifo` + a parallel vector of rows | `Probe`: one SPSC ring, acquire/release on the indices |
| `#if JUCE_DEBUG` around everything | **removed** — a library that silently no-ops is how you end up debugging a test that never logged |
| `juce::FileOutputStream` only | a `Sink` seam: `std::ofstream` in the core, JUCE in the adapter |
| `juce::Timer` owned by the logger | `drain()` you call from your own timer |
| CSV escaping, drop counter, header-once | **kept**, and now covered by tests |

**JUCE is optional.** It is confined to one adapter header —
`dsp_probe::juce_support::CsvLogger` — which keeps the exact original API
(`start()`, `pushFromAudioThread()`, `getDroppedRowCount()`, …) so an existing
call site needs no changes. The core has no JUCE dependency at all.

---

## Verified, not asserted

```bash
cmake --preset debug && cmake --build --preset debug && ctest --preset debug
```

- **86 assertions across 18 test cases**, covering the parts of a lock-free ring
  buffer that actually break: index wrap-around, dropping at capacity (and that
  the survivors are the *oldest*, not a corrupted mix), arity mismatch,
  `maxRows` batching, and a disarmed probe ignoring `push()` without counting it
  as a drop.
- **ASan + UBSan** via `--preset asan`.
- **CI** on Linux (gcc and clang) plus a sanitizer job; macOS and Windows on
  demand. The Windows and macOS jobs are manual on purpose — see the cost note
  in the workflow.

## What is not solved

Written down rather than left to be discovered:

- **Row layout is a vector of vectors.** One heap allocation per slot at
  construction and a pointer chase per drained row. A single flat buffer would be
  one allocation and contiguous; it is declared rather than fixed because the
  cost lands on construction and on the draining thread — never on the audio
  thread, which is the boundary that matters.
- **`dropped()` is a total, not a timeline.** It says how many rows were lost,
  not when, so it cannot localise an overload to a block.
- **The extracted adapter has not been compiled in this repository yet.** Its
  behaviour is the plugin's proven behaviour; the extraction (namespace, `Sink`
  seam) is new code. The JUCE CI job is its first real compile.
- **Number formatting follows the process locale** until something calls
  `std::setlocale(LC_NUMERIC, …)`; a comma-decimal locale would corrupt the CSV.
- **CSV injection is not neutralised.** A field starting with `=` is still
  evaluated as a formula by a spreadsheet. Sanitising here would mutate the data
  the library exists to record faithfully.

## License

MIT — see [LICENSE](LICENSE).
