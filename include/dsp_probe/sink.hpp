#pragma once

#include <utility>
#include <vector>

#include "dsp_probe/property.hpp"

namespace dsp_probe {

/// Where drained rows go.
///
/// The only thing the core knows about output. Keeping this an interface is
/// what makes the library JUCE-free: a file sink, an in-memory sink for tests,
/// and the JUCE FileOutputStream sink all look identical from Probe's side.
class Sink
{
public:
    Sink() = default;
    Sink(const Sink&) = delete;
    Sink& operator=(const Sink&) = delete;
    virtual ~Sink() = default;

    /// Called on the draining (non-audio) thread only.
    virtual void writeRow(const std::vector<Property>& row) = 0;

    virtual void flush() {}
};

/// Keeps every drained row in memory. Exists so tests can assert on what a
/// Probe produced without touching a filesystem.
class MemorySink final : public Sink
{
public:
    explicit MemorySink(std::vector<std::string> columns) : columns_(std::move(columns)) {}

    void writeRow(const std::vector<Property>& row) override { rows_.push_back(row); }

    [[nodiscard]] const std::vector<std::vector<Property>>& rows() const noexcept { return rows_; }

    [[nodiscard]] const std::vector<std::string>& columns() const noexcept { return columns_; }

    void clear() { rows_.clear(); }

private:
    std::vector<std::string> columns_;
    std::vector<std::vector<Property>> rows_;
};

} // namespace dsp_probe
