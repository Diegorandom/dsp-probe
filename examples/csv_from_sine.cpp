#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "dsp_probe/csv_file_sink.hpp"
#include "dsp_probe/probe.hpp"

// A stand-in for a piece of DSP under test: render blocks through a sine,
// measure each block, and log what was measured. Read the CSV it produces and
// you can see the block boundaries, the peak per block, and where the waveform
// crossed full scale.
//
//   ./dsp_probe_example [output.csv]

namespace {
constexpr double kPi = 3.14159265358979323846;
}

int main(int argc, char** argv)
{
    const std::string path = (argc > 1) ? argv[1] : std::string("dsp_probe_example.csv");

    constexpr int kBlockSize = 64;
    constexpr int kBlocks = 400;
    constexpr double kSampleRate = 48000.0;
    constexpr double kFrequency = 440.0;

    dsp_probe::Probe probe({"block", "peak", "rms", "clipped"}, 256);

    dsp_probe::CsvFileSink sink(path, probe.columns());
    if (!sink.good()) {
        std::fprintf(stderr, "cannot open '%s' for writing\n", path.c_str());
        return 1;
    }

    probe.start();

    double phase = 0.0;
    std::size_t written = 0;

    for (int block = 0; block < kBlocks; ++block) {
        double peak = 0.0;
        double sumOfSquares = 0.0;

        for (int i = 0; i < kBlockSize; ++i) {
            const double sample = std::sin(phase);

            phase += 2.0 * kPi * kFrequency / kSampleRate;
            if (phase > 2.0 * kPi)
                phase -= 2.0 * kPi;

            peak = std::max(peak, std::abs(sample));
            sumOfSquares += sample * sample;
        }

        const double rms = std::sqrt(sumOfSquares / kBlockSize);

        probe.push(block, static_cast<float>(peak), static_cast<float>(rms), peak > 0.999);

        // A real host would do this from a juce::Timer instead; here the loop
        // plays the part of the non-audio thread.
        written += probe.drain(sink, 64);
    }

    written += probe.stop(sink);

    std::printf("wrote %zu rows to %s (%llu dropped)\n",
                written,
                path.c_str(),
                static_cast<unsigned long long>(probe.dropped()));

    return 0;
}
