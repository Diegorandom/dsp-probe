#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "dsp_probe/csv.hpp"
#include "dsp_probe/property.hpp"

using dsp_probe::escape_csv;
using dsp_probe::format_header;
using dsp_probe::format_row;
using dsp_probe::Property;

TEST_CASE("to_string matches the format the original logger produced", "[property]")
{
    REQUIRE(dsp_probe::to_string(Property{true}) == "true");
    REQUIRE(dsp_probe::to_string(Property{false}) == "false");
    REQUIRE(dsp_probe::to_string(Property{42}) == "42");
    REQUIRE(dsp_probe::to_string(Property{-7}) == "-7");

    // 6 decimals: the shape juce::String(value, 6) gave, so reference CSVs
    // recorded with the old logger stay comparable.
    REQUIRE(dsp_probe::to_string(Property{1.5f}) == "1.500000");
    REQUIRE(dsp_probe::to_string(Property{-0.25}) == "-0.250000");
    REQUIRE(dsp_probe::to_string(Property{0.0}) == "0.000000");
}

TEST_CASE("escape_csv quotes only when it must", "[csv]")
{
    REQUIRE(escape_csv("plain") == "plain");
    REQUIRE(escape_csv("") == "");
    REQUIRE(escape_csv("with space") == "with space");

    REQUIRE(escape_csv("a,b") == "\"a,b\"");
    REQUIRE(escape_csv("a\nb") == "\"a\nb\"");
    REQUIRE(escape_csv("a\rb") == "\"a\rb\"");
}

TEST_CASE("escape_csv doubles inner quotes (RFC 4180)", "[csv]")
{
    REQUIRE(escape_csv("say \"hi\"") == "\"say \"\"hi\"\"\"");
    REQUIRE(escape_csv("\"") == "\"\"\"\"");
}

TEST_CASE("format_header joins columns and ends with a newline", "[csv]")
{
    REQUIRE(format_header({"a", "b", "c"}) == "a,b,c\n");
    REQUIRE(format_header({"a", "b,c"}) == "a,\"b,c\"\n");
    REQUIRE(format_header({}) == "\n");
}

TEST_CASE("format_row renders properties with commas", "[csv]")
{
    REQUIRE(format_row({Property{1}, Property{2.5f}, Property{true}}) == "1,2.500000,true\n");
    REQUIRE(format_row({Property{42}}) == "42\n"); // single column
}
