cmake_minimum_required(VERSION 3.22)

project(
  dsp-probe
  VERSION 0.1.0
  DESCRIPTION "Real-time-safe probes for testing DSP code"
  LANGUAGES CXX)

include(GNUInstallDirs)

# ---------------------------------------------------------------- opciones
# Tests y ejemplo solo se activan por defecto cuando este repo es el proyecto
# raíz: si alguien lo consume con add_subdirectory, no debe heredar un suite
# de tests ni arrastrar Catch2.
if(PROJECT_IS_TOP_LEVEL)
  set(_dsp_probe_default ON)
else()
  set(_dsp_probe_default OFF)
endif()

option(DSP_PROBE_BUILD_TESTS "Build the Catch2 test suite" ${_dsp_probe_default})
option(DSP_PROBE_BUILD_EXAMPLES "Build the example programs" ${_dsp_probe_default})
option(DSP_PROBE_BUILD_JUCE_ADAPTER "Compile-check the JUCE adapter (fetches JUCE)" OFF)
unset(_dsp_probe_default)

# ---------------------------------------------------------------- librería
# Header-only: no hay nada que compilar para consumirla, así que INTERFACE es
# literalmente el tipo correcto y no un atajo.
add_library(dsp_probe INTERFACE)
add_library(dsp_probe::dsp_probe ALIAS dsp_probe)

target_compile_features(dsp_probe INTERFACE cxx_std_17)
target_include_directories(
  dsp_probe INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

# Warnings para *nuestro* código, no para quien nos consume: por eso no es
# INTERFACE en dsp_probe.
add_library(dsp_probe_warnings INTERFACE)
if(MSVC)
  target_compile_options(dsp_probe_warnings INTERFACE /W4 /permissive-)
else()
  target_compile_options(dsp_probe_warnings INTERFACE -Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------- ejemplo
if(DSP_PROBE_BUILD_EXAMPLES)
  add_executable(dsp_probe_example examples/csv_from_sine.cpp)
  target_link_libraries(dsp_probe_example PRIVATE dsp_probe::dsp_probe dsp_probe_warnings)
endif()

# ---------------------------------------------------------------- adaptador JUCE
# OPCIONAL, y apagado por defecto. El adaptador no es experimental: es el logger
# que ya se usó dentro de RaiderPro en sus tests funcionales de DSP, conservado
# con su misma API. Lo opcional es el coste —FetchContent de JUCE son ~100 MB y
# varios minutos—, que no debe pagar quien solo quiere el núcleo, que no tiene
# ninguna dependencia de JUCE.
if(DSP_PROBE_BUILD_JUCE_ADAPTER)
  # JUCE aborta el configure si el proyecto no habilita C (su propio
  # CMakeLists.txt:40: "A C compiler is required to build targets that depend on
  # JUCE"). Se habilita AQUÍ y no en project(): el núcleo es C++17 puro y no
  # debe arrastrar un compilador de C, que es justo lo que compra quien deja
  # esta opción apagada. Detectado por el job `juce-adapter` del 2026-10-26, el
  # primer compile real que tuvo este adaptador fuera del plugin.
  enable_language(C)

  include(FetchContent)

  # La versión de JUCE contra la que se comprueba el adaptador. Es una cache
  # variable a propósito: el valor por defecto es el último release estable,
  # pero un consumidor (o CI) debe poder apuntarla a la JUCE que usa de verdad
  # con -DDSP_PROBE_JUCE_TAG=... — que es el único caso en el que este check
  # dice algo útil.
  set(DSP_PROBE_JUCE_TAG
      "8.0.15"
      CACHE STRING "JUCE git tag used for the adapter compile check")

  FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG ${DSP_PROBE_JUCE_TAG}
    GIT_SHALLOW TRUE)

  FetchContent_MakeAvailable(JUCE)

  add_library(dsp_probe_juce INTERFACE)
  add_library(dsp_probe::juce ALIAS dsp_probe_juce)
  target_link_libraries(dsp_probe_juce INTERFACE dsp_probe::dsp_probe juce::juce_events)
  target_sources(dsp_probe_juce INTERFACE
                 ${CMAKE_CURRENT_SOURCE_DIR}/include/dsp_probe/juce_support/csv_logger.hpp)

  # Solo compila, no corre: crear un juce::Timer sin MessageManager no es el
  # objetivo del check, y hacerlo fallaría por razones ajenas a esta librería.
  add_executable(dsp_probe_juce_compile_check tests/juce_adapter_compile_check.cpp)
  target_link_libraries(dsp_probe_juce_compile_check PRIVATE dsp_probe_juce dsp_probe_warnings)
endif()

# ---------------------------------------------------------------- tests
if(DSP_PROBE_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# ---------------------------------------------------------------- instalación
include(CMakePackageConfigHelpers)

install(TARGETS dsp_probe EXPORT dsp-probeTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(
  EXPORT dsp-probeTargets
  NAMESPACE dsp_probe::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dsp-probe)

configure_package_config_file(
  cmake/dsp-probeConfig.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/dsp-probeConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dsp-probe)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/dsp-probeConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dsp-probeConfig.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/dsp-probeConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dsp-probe)
