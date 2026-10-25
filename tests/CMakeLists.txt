include(FetchContent)

# Catch2 v3 por FetchContent: sin submódulos, sin dependencia del sistema, y
# `SYSTEM` para que sus warnings no ensucien la salida de nuestros targets.
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.7.1
  GIT_SHALLOW TRUE
  SYSTEM)

FetchContent_MakeAvailable(Catch2)

add_executable(
  dsp_probe_tests
  test_csv.cpp
  test_probe.cpp
  test_file_sink.cpp)

target_link_libraries(dsp_probe_tests PRIVATE dsp_probe::dsp_probe Catch2::Catch2WithMain
                                         dsp_probe_warnings)

add_test(NAME dsp_probe_tests COMMAND dsp_probe_tests)
