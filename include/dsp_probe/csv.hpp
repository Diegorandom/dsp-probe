#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "dsp_probe/property.hpp"

namespace dsp_probe {

/// Quotes a field if, and only if, it needs it (RFC 4180).
///
/// Note what deliberately does NOT trigger quoting: a leading '=' or '+'.
/// A CSV opened in a spreadsheet will still evaluate such a cell as a formula,
/// which is a real injection vector. Callers that log untrusted strings (a
/// preset name, a file name) must neutralise that themselves; doing it here
/// would silently mutate the data this library exists to record faithfully.
[[nodiscard]] inline std::string escape_csv(const std::string& field)
{
    const bool needsQuotes = field.find_first_of(",\"\n\r") != std::string::npos;
    if (!needsQuotes)
        return field;

    std::string out;
    out.reserve(field.size() + 2);
    out.push_back('"');
    for (const char c : field) {
        if (c == '"')
            out.push_back('"'); // RFC 4180: a quote is escaped by doubling it
        out.push_back(c);
    }
    out.push_back('"');
    return out;
}

/// The header line, newline included.
[[nodiscard]] inline std::string format_header(const std::vector<std::string>& columns)
{
    std::string line;
    for (std::size_t i = 0; i < columns.size(); ++i) {
        line += escape_csv(columns[i]);
        if (i + 1 < columns.size())
            line.push_back(',');
    }
    line.push_back('\n');
    return line;
}

/// One data line, newline included. Row arity is the caller's contract; a
/// short row would silently shift every later column, so Probe refuses to
/// produce one (see Probe::push).
[[nodiscard]] inline std::string format_row(const std::vector<Property>& row)
{
    std::string line;
    for (std::size_t i = 0; i < row.size(); ++i) {
        line += escape_csv(to_string(row[i]));
        if (i + 1 < row.size())
            line.push_back(',');
    }
    line.push_back('\n');
    return line;
}

} // namespace dsp_probe
