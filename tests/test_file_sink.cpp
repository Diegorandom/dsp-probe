#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "dsp_probe/csv_file_sink.hpp"
#include "dsp_probe/property.hpp"

using dsp_probe::CsvFileSink;
using dsp_probe::Property;

namespace {

std::filesystem::path tempCsvPath(const std::string& name)
{
    return std::filesystem::temp_directory_path() / name;
}

std::string readWholeFile(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

} // namespace

TEST_CASE("CsvFileSink writes a header even with no rows", "[sink]")
{
    const auto path = tempCsvPath("dsp_probe_header_only.csv");
    std::filesystem::remove(path);

    {
        CsvFileSink sink(path.string(), {"index", "value"});
        REQUIRE(sink.good());
        sink.flush();
    }

    REQUIRE(readWholeFile(path) == "index,value\n");
    std::filesystem::remove(path);
}

TEST_CASE("CsvFileSink writes rows in order, escaped", "[sink]")
{
    const auto path = tempCsvPath("dsp_probe_rows.csv");
    std::filesystem::remove(path);

    {
        CsvFileSink sink(path.string(), {"index", "value", "flag"});
        REQUIRE(sink.good());

        sink.writeRow({Property{1}, Property{2.5f}, Property{true}});
        sink.writeRow({Property{2}, Property{-0.125}, Property{false}});
        sink.flush();
    }

    REQUIRE(readWholeFile(path) == "index,value,flag\n1,2.500000,true\n2,-0.125000,false\n");
    std::filesystem::remove(path);
}

TEST_CASE("CsvFileSink ignores a row whose arity does not match", "[sink]")
{
    const auto path = tempCsvPath("dsp_probe_bad_row.csv");
    std::filesystem::remove(path);

    {
        CsvFileSink sink(path.string(), {"a", "b"});
        REQUIRE(sink.good());

        sink.writeRow({Property{1}}); // short row: would shift every later column
        sink.writeRow({Property{2}, Property{3.0f}});
        sink.flush();
    }

    REQUIRE(readWholeFile(path) == "a,b\n2,3.000000\n");
    std::filesystem::remove(path);
}

TEST_CASE("CsvFileSink reports failure for an unwritable path", "[sink]")
{
    CsvFileSink sink("/this/directory/does/not/exist/out.csv", {"a"});
    REQUIRE_FALSE(sink.good());
}
