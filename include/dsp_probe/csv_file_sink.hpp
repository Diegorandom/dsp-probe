#pragma once

#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "dsp_probe/csv.hpp"
#include "dsp_probe/sink.hpp"

namespace dsp_probe {

/// Writes rows to a CSV file with std::ofstream — no JUCE required.
///
/// The header is written once, when the file is opened, so an empty run still
/// produces a readable file with column names rather than a zero-byte mystery.
class CsvFileSink final : public Sink
{
public:
    CsvFileSink(const std::string& path, std::vector<std::string> columns)
        : out_(path, std::ios::binary | std::ios::trunc), columns_(std::move(columns))
    {
        if (out_)
            out_ << format_header(columns_);
    }

    void writeRow(const std::vector<Property>& row) override
    {
        if (out_ && row.size() == columns_.size())
            out_ << format_row(row);
    }

    void flush() override
    {
        if (out_)
            out_.flush();
    }

    /// False when the file could not be opened. Check this before trusting a
    /// test that "produced no output".
    [[nodiscard]] bool good() const noexcept { return static_cast<bool>(out_); }

    [[nodiscard]] const std::vector<std::string>& columns() const noexcept { return columns_; }

private:
    std::ofstream out_;
    std::vector<std::string> columns_;
};

} // namespace dsp_probe
