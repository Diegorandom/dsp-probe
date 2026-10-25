#pragma once

// JUCE adapter — OPTIONAL. The core (probe.hpp, csv.hpp, …) has no JUCE
// dependency at all; nothing here is needed to use the library.
//
// It exists because the logger it wraps was proven inside RaiderPro, in the
// plugin's real DSP functional tests, before being extracted here. Everything
// that is *required* to be JUCE lives in this file and only here:
// juce::FileOutputStream for the file, juce::Timer for the pump, and a CsvLogger
// that keeps the exact API surface of the original header so an existing call
// site needs no changes. The behaviour is not speculative — only its packaging
// in this library is new.

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include <cstddef>
#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "dsp_probe/csv.hpp"
#include "dsp_probe/probe.hpp"
#include "dsp_probe/sink.hpp"

namespace dsp_probe::juce_support {

/// CSV file sink backed by juce::FileOutputStream.
///
/// Kept separate from CsvFileSink because JUCE projects often already have a
/// juce::File in hand (the original resolved it to the user's Documents
/// directory) and re-deriving it as a std::string path is lossy on Windows.
class FileSink final : public Sink
{
public:
    FileSink(juce::File file, std::vector<std::string> columns)
        : file_(std::move(file)), columns_(std::move(columns))
    {
    }

    bool open()
    {
        file_.deleteFile();
        stream_ = file_.createOutputStream();
        if (stream_ == nullptr)
            return false;

        stream_->writeText(format_header(columns_), false, false, "\n");
        return true;
    }

    void writeRow(const std::vector<Property>& row) override
    {
        if (stream_ == nullptr || row.size() != columns_.size())
            return;

        stream_->writeText(format_row(row), false, false, "\n");
    }

    void flush() override
    {
        if (stream_ != nullptr)
            stream_->flush();
    }

    void close()
    {
        if (stream_ == nullptr)
            return;

        stream_->flush();
        stream_.reset();
    }

    [[nodiscard]] bool isOpen() const noexcept { return stream_ != nullptr; }

private:
    juce::File file_;
    std::vector<std::string> columns_;
    std::unique_ptr<juce::FileOutputStream> stream_;
};

/// Drop-in replacement for RaiderPro's CsvLogger.
///
/// Proven in that plugin's functional tests; this class is the same behaviour
/// with the packaging changed. Same method names, same constants, same default
/// file location. What changed:
/// the row queue is dsp_probe::Probe (single-producer ring buffer with a drop
/// counter) instead of juce::AbstractFifo, and the #if JUCE_DEBUG gating is
/// gone — start() now succeeds in Release too, because a library silently
/// doing nothing is how you end up debugging a test that never logged.
class CsvLogger final : private juce::Timer
{
public:
    static constexpr int kCapacity = 8192;
    static constexpr int kDrainTimerHz = 30;

    CsvLogger(std::initializer_list<std::string> columns, juce::String fileName)
        : probe_(columns, static_cast<std::size_t>(kCapacity)), fileName_(std::move(fileName))
    {
    }

    ~CsvLogger() override { stop(); }

    bool start()
    {
        stop();

        const auto file = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                              .getChildFile(fileName_ + ".csv");

        auto sink = std::make_unique<FileSink>(file, probe_.columns());
        if (!sink->open())
            return false;

        sink_ = std::move(sink);
        probe_.start();
        startTimerHz(kDrainTimerHz);
        return true;
    }

    void stop()
    {
        stopTimer();

        if (sink_ != nullptr) {
            probe_.stop(*sink_);
            sink_->close();
            sink_.reset();
        } else {
            probe_.stop();
        }
    }

    /// AUDIO THREAD.
    template <typename... Args>
    void pushFromAudioThread(const Args&... args) noexcept
    {
        probe_.push(args...);
    }

    /// NON-AUDIO THREAD. Also called by the internal timer at kDrainTimerHz.
    void flushFromNonAudioThread(int maxRows = kCapacity)
    {
        if (sink_ == nullptr)
            return;

        probe_.drain(*sink_, static_cast<std::size_t>(juce::jmax(1, maxRows)));
        sink_->flush();
    }

    [[nodiscard]] int getDroppedRowCount() const noexcept
    {
        return static_cast<int>(probe_.dropped());
    }

    [[nodiscard]] const std::vector<std::string>& getColumnNames() const noexcept
    {
        return probe_.columns();
    }

private:
    void timerCallback() override { flushFromNonAudioThread(); }

    Probe probe_;
    juce::String fileName_;
    std::unique_ptr<FileSink> sink_;
};

} // namespace dsp_probe::juce_support
