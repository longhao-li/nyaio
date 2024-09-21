cmake_minimum_required(VERSION 3.16)

project(nyaio VERSION 0.0.0 LANGUAGES CXX)

# Disable in-source build.
if(${PROJECT_SOURCE_DIR} STREQUAL ${PROJECT_BINARY_DIR})
    message(FATAL_ERROR "In-source build is not allowed.")
endif()

option(NYAIO_BUILD_SHARED_LIBS  "Build nyaio as shared library. The CMake BUILD_SHARED_LIBS option does not affect this project." OFF)
option(NYAIO_WARNINGS_AS_ERRORS "Treat warnings as errors when building nyaio." OFF)
option(NYAIO_BUILD_TESTS        "Build tests for nyaio." OFF)
option(NYAIO_BUILD_EXAMPLES     "Build examples for nyaio." OFF)
option(NYAIO_ENABLE_LTO         "Enable link-time optimization for shared library. Ignored for static library." OFF)

file(GLOB_RECURSE NYAIO_HEADER_FILES "include/nyaio/*.hpp")
file(GLOB_RECURSE NYAIO_SOURCE_FILES "src/nyaio/*.cpp")

if(NYAIO_BUILD_SHARED_LIBS)
    add_library(nyaio SHARED ${NYAIO_HEADER_FILES} ${NYAIO_SOURCE_FILES})

    # Options for shared library.
    target_compile_definitions(nyaio PUBLIC "NYAIO_API=__attribute__((visibility(\"default\")))")
    set_target_properties(
        nyaio
        PROPERTIES  POSITION_INDEPENDENT_CODE ON
                    CXX_VISIBILITY_PRESET hidden
    )

    if(NYAIO_ENABLE_LTO)
        include(CheckIPOSupported)
        check_ipo_supported(RESULT LTO_SUPPORTED OUTPUT LTO_SUPPORTED_MESSAGE)

        if(LTO_SUPPORTED)
            set_target_properties(nyaio PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
        else()
            message(WARNING "Link-time optimization is not supported: ${LTO_SUPPORTED_MESSAGE}")
        endif()
    endif()
else()
    add_library(nyaio STATIC ${NYAIO_HEADER_FILES} ${NYAIO_SOURCE_FILES})
    target_compile_definitions(nyaio PUBLIC "NYAIO_API=")
endif()

# Alias target to be consistent with the package name.
add_library(nyaio::nyaio ALIAS nyaio)

# This project uses C++20 coroutine and concept.
target_compile_features(nyaio PUBLIC cxx_std_20)

# Set compiler options.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        nyaio
        PRIVATE -Wall -Wextra -pedantic -Wshadow -Woverloaded-virtual -Wold-style-cast
    )

    if(NYAIO_WARNINGS_AS_ERRORS)
        target_compile_options(nyaio PRIVATE -Werror)
    endif()
endif()

target_include_directories(
    nyaio
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Configure the package.
include(GNUInstallDirs)

install(
    TARGETS     nyaio
    EXPORT      nyaio-targets
    RUNTIME     DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY     DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE     DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    EXPORT      nyaio-targets
    FILE        nyaio-targets.cmake
    NAMESPACE   nyaio::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/nyaio
)

# Generate the package configuration file.
include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/nyaio-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/nyaio-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/nyaio
)

write_basic_package_version_file(
    nyaio-config-version.cmake
    VERSION ${PACKAGE_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(
    FILES       ${CMAKE_CURRENT_BINARY_DIR}/nyaio-config.cmake
                ${CMAKE_CURRENT_BINARY_DIR}/nyaio-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/nyaio
)

# Install the header files.
install(
    DIRECTORY   ${CMAKE_CURRENT_SOURCE_DIR}/include/nyaio
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Build tests.
if(NYAIO_BUILD_TESTS)
    include(FetchContent)
    
    # CMake policy CMP0135 controls extraction behavior of the FetchContent module.
    # This policy was introduced in CMake 3.21. We set the policy to NEW to avoid
    # unnecessary downloads of the same content.
    if(POLICY CMP0135)
    	cmake_policy(SET CMP0135 NEW)
    	set(CMAKE_POLICY_DEFAULT_CMP0135 NEW)
    endif()

    # Find doctest
    message(STATUS "Fetching doctest from github.com/doctest/doctest")

    set(DOCTEST_NO_INSTALL ON CACHE INTERNAL "")
    FetchContent_Declare(
        doctest
        GIT_REPOSITORY https://github.com/doctest/doctest
        GIT_TAG        v2.4.11
    )

    FetchContent_MakeAvailable(doctest)

    # Build test target
    file(GLOB_RECURSE NYAIO_TEST_FILES "test/*.cpp")
    add_executable(nyaio-test ${NYAIO_TEST_FILES})

    target_link_libraries(nyaio-test PRIVATE nyaio doctest::doctest)

    # Configure CMake tests
    enable_testing()
    include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
    doctest_discover_tests(nyaio-test)
endif()

# Build examples.
if(NYAIO_BUILD_EXAMPLES)
    add_subdirectory(example)
endif()
