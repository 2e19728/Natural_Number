cmake_minimum_required(VERSION 3.20)

project(natural
	VERSION 4.0.0
	DESCRIPTION "Arbitrary-precision integers on a scalar 3-modulus NTT engine"
	HOMEPAGE_URL "https://github.com/2e19728/natural"
	LANGUAGES CXX ASM)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(NATURAL_BUILD_TESTS "Build the self-checking test suite" ON)
option(NATURAL_BUILD_BENCHMARKS "Build the throughput benchmark" ON)
option(NATURAL_BUILD_EXAMPLES "Build the examples" ON)

# ---------------------------------------------------------------- the library
# Collect every executable directly in the build directory (build/natural_test, ...) rather
# than one binary directory per subdirectory - this is a small project and the flat layout is
# what the README documents.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# The library is header-mostly plus two assembly translation units, so the CMake target is
# mostly about propagating the right usage requirements to whoever links it.
add_library(natural STATIC
	src/mul_basecase.s
	src/mul_ntt.s)
add_library(natural::natural ALIAS natural)

target_include_directories(natural PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

# The public headers carry Intel-syntax inline assembly (basic_arithmetics.h), so every
# translation unit that includes them must be assembled in Intel syntax.  PUBLIC, because
# the requirement has to reach the consumer, not just this build.
target_compile_options(natural PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-masm=intel>)

target_compile_features(natural PUBLIC cxx_std_20)
set_target_properties(natural PROPERTIES
	POSITION_INDEPENDENT_CODE ON
	CXX_EXTENSIONS OFF)

# x86-64 only: the kernels use mulx (BMI2).  The build deliberately does not force a -march;
# -march=x86-64-v3 is the lowest level that provides BMI2, and CI builds with exactly that.

# ------------------------------------------------- tests / benchmarks / examples
if(NATURAL_BUILD_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()
if(NATURAL_BUILD_BENCHMARKS)
	add_subdirectory(benchmarks)
endif()
if(NATURAL_BUILD_EXAMPLES)
	add_executable(natural_example examples/square.cpp)
	target_link_libraries(natural_example PRIVATE natural::natural)
endif()

# --------------------------------------------------------------- install / export
install(TARGETS natural EXPORT naturalTargets
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

set(NATURAL_CMAKE_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/natural)
install(EXPORT naturalTargets
	FILE naturalTargets.cmake
	NAMESPACE natural::
	DESTINATION ${NATURAL_CMAKE_DIR})

configure_package_config_file(cmake/natural-config.cmake.in
	"${CMAKE_CURRENT_BINARY_DIR}/natural-config.cmake"
	INSTALL_DESTINATION ${NATURAL_CMAKE_DIR})
write_basic_package_version_file(
	"${CMAKE_CURRENT_BINARY_DIR}/natural-config-version.cmake"
	VERSION ${PROJECT_VERSION}
	COMPATIBILITY SameMajorVersion)
install(FILES
	"${CMAKE_CURRENT_BINARY_DIR}/natural-config.cmake"
	"${CMAKE_CURRENT_BINARY_DIR}/natural-config-version.cmake"
	DESTINATION ${NATURAL_CMAKE_DIR})
