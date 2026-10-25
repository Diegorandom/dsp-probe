#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include "dsp_probe/property.hpp"
#include "dsp_probe/sink.hpp"

namespace dsp_probe {

/// A bounded, lock-free hand-off from the audio thread to a logging thread.
///
/// Single producer (the audio thread) / single consumer (whatever thread
/// drains). push() never allocates, never locks and never blocks: when the
/// ring is full it drops the row and counts it, because on the audio thread
/// the only correct answer to backpressure is to lose data rather than miss a
/// deadline. The drop counter is what makes that loss visible instead of
/// silent — check dropped() after a run.
///
/// All memory is allocated in the constructor, which is the whole point: a
/// std::vector inside push() would be a real-time violation, and this type
/// exists to be usable where that matters.
class Probe
{
public:
    /// 8192 rows of headroom at, say, 64-sample blocks and 48 kHz is ~11 s of
    /// audio, comfortably more than a 30 Hz drain needs.
    static constexpr std::size_t kDefaultCapacity = 8192;

    explicit Probe(std::initializer_list<std::string> columns,
                   std::size_t capacity = kDefaultCapacity)
        : columns_(columns),
          capacity_(roundUpToPowerOfTwo(capacity)),
          mask_(capacity_ - 1)
    {
        rows_.assign(capacity_, Row(columns_.size()));
    }

    Probe(const Probe&) = delete;
    Probe& operator=(const Probe&) = delete;

    /// Arms the probe. Until this is called push() is a no-op, so a logger can
    /// be constructed and wired up without capturing anything by accident.
    void start() noexcept { running_.store(true, std::memory_order_release); }

    void stop() noexcept { running_.store(false, std::memory_order_release); }

    /// Disarms and drains whatever is still queued.
    std::size_t stop(Sink& sink)
    {
        stop();
        const auto drained = drain(sink);
        sink.flush();
        return drained;
    }

    /// AUDIO THREAD. Real-time safe: no allocation, no locks, no exceptions.
    ///
    /// The arity check is a *runtime* guard rather than a compile-time one
    /// because columns are decided at runtime; a mismatch counts as a dropped
    /// row instead of writing a short line that would shift every later column.
    template <typename... Args>
    void push(const Args&... args) noexcept
    {
        static_assert(sizeof...(Args) > 0, "a row needs at least one column");
        static_assert((is_property_type_v<Args> && ...),
                      "dsp_probe::Probe columns accept only float, int, double or bool");

        if (!running_.load(std::memory_order_acquire))
            return;

        if (sizeof...(Args) != columns_.size()) {
            dropped_.fetch_add(1, std::memory_order_relaxed);
            return;
        }

        const auto writeIndex = write_.load(std::memory_order_relaxed);
        const auto readIndex = read_.load(std::memory_order_acquire);

        if (writeIndex - readIndex >= capacity_) {
            dropped_.fetch_add(1, std::memory_order_relaxed);
            return;
        }

        Row& slot = rows_[writeIndex & mask_];
        std::size_t i = 0;
        ((slot[i++] = Property{args}), ...);

        write_.store(writeIndex + 1, std::memory_order_release);
    }

    /// NON-AUDIO THREAD. Moves up to maxRows queued rows into the sink and
    /// returns how many were written.
    std::size_t drain(Sink& sink, std::size_t maxRows = static_cast<std::size_t>(-1))
    {
        const auto writeIndex = write_.load(std::memory_order_acquire);
        auto readIndex = read_.load(std::memory_order_relaxed);

        std::size_t written = 0;
        while (readIndex != writeIndex && written < maxRows) {
            sink.writeRow(rows_[readIndex & mask_]);
            ++readIndex;
            ++written;
        }

        read_.store(readIndex, std::memory_order_release);
        return written;
    }

    [[nodiscard]] std::uint64_t dropped() const noexcept
    {
        return dropped_.load(std::memory_order_relaxed);
    }

    void resetDropped() noexcept { dropped_.store(0, std::memory_order_relaxed); }

    /// Rows queued but not yet drained.
    [[nodiscard]] std::size_t pending() const noexcept
    {
        return write_.load(std::memory_order_acquire) - read_.load(std::memory_order_acquire);
    }

    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }

    [[nodiscard]] const std::vector<std::string>& columns() const noexcept { return columns_; }

    [[nodiscard]] bool isRunning() const noexcept
    {
        return running_.load(std::memory_order_acquire);
    }

private:
    using Row = std::vector<Property>;

    /// The counter wrap (writeIndex & mask_) needs a power-of-two capacity, and
    /// it also removes a modulo from the audio path.
    static std::size_t roundUpToPowerOfTwo(std::size_t value)
    {
        if (value <= 1)
            return 1;

        std::size_t power = 1;
        while (power < value)
            power <<= 1;
        return power;
    }

    std::vector<std::string> columns_;
    std::vector<Row> rows_;
    std::size_t capacity_;
    std::size_t mask_;

    // Productor y consumidor escriben índices distintos. Si comparten línea de
    // caché se invalidan mutuamente en cada bloque, convirtiendo un hand-off
    // lock-free en un ping-pong de coherencia. Se alinean por separado.
    alignas(64) std::atomic<std::size_t> read_{0};
    alignas(64) std::atomic<std::size_t> write_{0};
    std::atomic<std::uint64_t> dropped_{0};
    std::atomic<bool> running_{false};
};

} // namespace dsp_probe
