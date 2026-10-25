#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "dsp_probe/probe.hpp"
#include "dsp_probe/sink.hpp"

using dsp_probe::MemorySink;
using dsp_probe::Probe;
using dsp_probe::Property;

TEST_CASE("push is inert until start() is called", "[probe]")
{
    Probe probe({"index", "value"}, 8);
    MemorySink sink(probe.columns());

    probe.push(1, 2.5f);
    REQUIRE(probe.pending() == 0);
    REQUIRE_FALSE(probe.isRunning());

    probe.start();
    probe.push(1, 2.5f);
    REQUIRE(probe.pending() == 1);
}

TEST_CASE("drain preserves order and values", "[probe]")
{
    Probe probe({"index", "value"}, 8);
    MemorySink sink(probe.columns());
    probe.start();

    probe.push(7, 0.5f);
    probe.push(9, -1.25f);

    REQUIRE(probe.drain(sink) == 2);
    REQUIRE(sink.rows().size() == 2);

    REQUIRE(std::get<int>(sink.rows()[0][0]) == 7);
    REQUIRE(std::get<float>(sink.rows()[0][1]) == 0.5f);
    REQUIRE(std::get<int>(sink.rows()[1][0]) == 9);
    REQUIRE(std::get<float>(sink.rows()[1][1]) == -1.25f);
}

TEST_CASE("a full ring drops rows and counts every one", "[probe]")
{
    Probe probe({"v"}, 4);
    MemorySink sink(probe.columns());
    probe.start();

    for (int i = 0; i < 6; ++i)
        probe.push(i);

    REQUIRE(probe.pending() == 4);
    REQUIRE(probe.dropped() == 2);

    // The four that survived are the OLDEST, not the newest: dropping happens
    // on write, so it must not corrupt what was already queued.
    probe.drain(sink);
    REQUIRE(sink.rows().size() == 4);
    REQUIRE(std::get<int>(sink.rows()[0][0]) == 0);
    REQUIRE(std::get<int>(sink.rows()[3][0]) == 3);
}

TEST_CASE("wrong arity is counted as dropped, never written short", "[probe]")
{
    Probe probe({"a", "b"}, 4);
    probe.start();

    probe.push(1);        // one value, two columns
    probe.push(1, 2.0f, 3); // three values, two columns

    REQUIRE(probe.dropped() == 2);
    REQUIRE(probe.pending() == 0);

    // And a well-formed row still passes afterwards.
    probe.push(4, 5.0f);
    REQUIRE(probe.pending() == 1);
}

TEST_CASE("the ring wraps without losing or reordering rows", "[probe]")
{
    Probe probe({"v"}, 4);
    MemorySink sink(probe.columns());
    probe.start();

    for (int round = 0; round < 5; ++round) {
        for (int i = 0; i < 4; ++i)
            probe.push(round * 4 + i);
        REQUIRE(probe.drain(sink) == 4);
    }

    REQUIRE(sink.rows().size() == 20);
    for (int i = 0; i < 20; ++i)
        REQUIRE(std::get<int>(sink.rows()[static_cast<std::size_t>(i)][0]) == i);
}

TEST_CASE("capacity is rounded up to a power of two", "[probe]")
{
    REQUIRE(Probe({"v"}, 1).capacity() == 1);
    REQUIRE(Probe({"v"}, 5).capacity() == 8);
    REQUIRE(Probe({"v"}, 8192).capacity() == 8192);
    REQUIRE(Probe({"v"}, 8193).capacity() == 16384);
}

TEST_CASE("drain honours maxRows and leaves the rest queued", "[probe]")
{
    Probe probe({"v"}, 8);
    MemorySink sink(probe.columns());
    probe.start();

    for (int i = 0; i < 4; ++i)
        probe.push(i);

    REQUIRE(probe.drain(sink, 2) == 2);
    REQUIRE(probe.pending() == 2);
    REQUIRE(probe.drain(sink, 2) == 2);
    REQUIRE(probe.pending() == 0);
}

TEST_CASE("stop() drains what is left and disarms", "[probe]")
{
    Probe probe({"v"}, 8);
    MemorySink sink(probe.columns());
    probe.start();

    probe.push(1);
    probe.push(2);

    REQUIRE(probe.stop(sink) == 2);
    REQUIRE_FALSE(probe.isRunning());
    REQUIRE(probe.pending() == 0);

    probe.push(3); // disarmed: ignored, and NOT counted as a drop
    REQUIRE(probe.pending() == 0);
    REQUIRE(probe.dropped() == 0);
}

TEST_CASE("resetDropped clears the counter without touching the queue", "[probe]")
{
    Probe probe({"v"}, 2);
    probe.start();

    for (int i = 0; i < 4; ++i)
        probe.push(i);

    REQUIRE(probe.dropped() == 2);
    probe.resetDropped();
    REQUIRE(probe.dropped() == 0);
    REQUIRE(probe.pending() == 2);
}
