#pragma once

#include <cstddef>
#include <cstdio>
#include <string>
#include <type_traits>
#include <variant>

namespace dsp_probe {

/// A single cell of a logged row.
///
/// Deliberately a closed set: these four are the types a DSP test actually
/// records (a dB value, a sample index, a slice length, a flag). Keeping the
/// set closed is what lets formatting stay allocation-free and total.
using Property = std::variant<float, int, double, bool>;

/// True for the types a column may hold. Used by Probe::push to fail at
/// compile time with a readable message instead of deep inside std::variant.
template <typename T>
inline constexpr bool is_property_type_v = std::is_same_v<std::decay_t<T>, float> ||
                                           std::is_same_v<std::decay_t<T>, int> ||
                                           std::is_same_v<std::decay_t<T>, double> ||
                                           std::is_same_v<std::decay_t<T>, bool>;

/// Renders one cell exactly as it will appear in the CSV.
///
/// Floating point uses 6 decimals — the same shape the original JUCE logger
/// produced with juce::String(value, 6) — so migration does not silently
/// change the numbers in existing reference CSVs.
///
/// NOTE: std::snprintf formats in the "C" locale only until someone calls
/// std::setlocale(LC_NUMERIC, ...) in the host process; under a comma-decimal
/// locale this would emit a decimal comma and corrupt the CSV. A plugin that
/// changes the process locale must either not do that, or route formatting
/// through std::to_chars (C++17 for integers; floating point needs C++26 for
/// full portability). This is documented rather than papered over.
[[nodiscard]] inline std::string to_string(const Property& p)
{
    return std::visit(
        [](const auto& v) -> std::string {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, bool>) {
                return v ? "true" : "false";
            } else if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>) {
                char buffer[32];
                const int written =
                    std::snprintf(buffer, sizeof buffer, "%.6f", static_cast<double>(v));
                if (written <= 0)
                    return {};
                return std::string(buffer, static_cast<std::size_t>(written));
            } else {
                return std::to_string(v);
            }
        },
        p);
}

} // namespace dsp_probe
