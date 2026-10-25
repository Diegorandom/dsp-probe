// Compile-and-link check for the JUCE adapter.
//
// Deliberately does NOT start() the logger: that writes into the user's
// Documents directory and would need a MessageManager. This target exists to
// prove the adapter still compiles against the JUCE version we pin, which is
// the thing that silently rots.

#include <dsp_probe/juce_support/csv_logger.hpp>

int main()
{
    dsp_probe::juce_support::CsvLogger logger({"index", "value"}, "dsp_probe_compile_check");

    return logger.getDroppedRowCount() == 0 && logger.getColumnNames().size() == 2 ? 0 : 1;
}
