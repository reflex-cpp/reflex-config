# Part of the Reflex project.
# Copyright © Reflex Project Contributors.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.25)

project(reflex-config VERSION 2.0.0 LANGUAGES CXX)

option(REFLEX_CONFIG_BUILD_TESTS "Build tests" ${PROJECT_IS_TOP_LEVEL})
option(REFLEX_CONFIG_INSTALL_CMAKE_CONFIG "Install CMake Config files" ${PROJECT_IS_TOP_LEVEL})
option(REFLEX_CONFIG_USE_FETCH_CONTENT "Use FetchContent for internal dependencies" OFF)

if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 23)
endif()
set(CMAKE_CXX_EXTENSIONS OFF)
message(STATUS "reflex-config: C++ standard: ${CMAKE_CXX_STANDARD}")

if(REFLEX_CONFIG_USE_FETCH_CONTENT)
    include("${CMAKE_CURRENT_LIST_DIR}/tools/cmake/fetch_dependencies.cmake")
endif()
if(NOT TARGET reflex::reflex)
    find_package(reflex 1.0.1 REQUIRED)
endif()
if(NOT TARGET Boost::json)
    find_package(Boost REQUIRED COMPONENTS json CONFIG)
endif()
if(NOT TARGET fmt::fmt)
    find_package(fmt REQUIRED)
endif()

include(GNUInstallDirs)
include("${CMAKE_CURRENT_LIST_DIR}/tools/cmake/diagnostics.cmake")

add_library(config STATIC)
add_library(reflex::config ALIAS config)
set_target_properties(config PROPERTIES OUTPUT_NAME reflex_config)
target_compile_features(config PUBLIC cxx_std_23)
reflex_set_diagnostic_options(config)
target_sources(
    config
    PUBLIC
        FILE_SET HEADERS
            BASE_DIRS include
            FILES
                include/reflex/config/concepts.hpp
                include/reflex/config/context.hpp
                include/reflex/config/deserialize.hpp
                include/reflex/config/deserialize_and_validate.hpp
                include/reflex/config/errors.hpp
                include/reflex/config/fixed_value.hpp
                include/reflex/config/format_error_state.hpp
                include/reflex/config/fwd.hpp
                include/reflex/config/reflection_helpers.hpp
                include/reflex/config/serialize.hpp
                include/reflex/config/stringify_json.hpp
                include/reflex/config/traits.hpp
                include/reflex/config/validate.hpp
                include/reflex/config/validators.hpp
                include/reflex/config/detail/throw_formatted.hpp
    PRIVATE
        src/concepts.cpp
        src/context.cpp
        src/deserialize.cpp
        src/deserialize_and_validate.cpp
        src/errors.cpp
        src/fixed_value.cpp
        src/format_error_state.cpp
        src/reflection_helpers.cpp
        src/serialize.cpp
        src/stringify_json.cpp
        src/traits.cpp
        src/validate.cpp
        src/validators.cpp
        src/detail/fmt_compat.hpp
        src/detail/throw_formatted.cpp
)
target_link_libraries(config PUBLIC reflex::reflex Boost::json fmt::fmt)
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set_target_properties(
        config
        PROPERTIES COMPILE_PDB_NAME reflex_config COMPILE_PDB_OUTPUT_DIRECTORY "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}"
    )
endif()

if(REFLEX_CONFIG_BUILD_TESTS)
    enable_testing()
    find_package(Catch2 REQUIRED)

    add_executable(reflex_config_tests)
    reflex_set_diagnostic_options(reflex_config_tests)
    target_link_libraries(reflex_config_tests PRIVATE Catch2::Catch2WithMain reflex::config)

    target_sources(
        reflex_config_tests
        PRIVATE
            FILE_SET HEADERS
                BASE_DIRS tests
                FILES tests/error_helpers.hpp tests/generate_stringify_json_options.hpp tests/test_types.hpp
        PRIVATE
            tests/concepts.test.cpp
            tests/context.test.cpp
            tests/deserialize.test.cpp
            tests/fixed_value.test.cpp
            tests/format_error_state.test.cpp
            tests/serialize.test.cpp
            tests/stringify_json.test.cpp
            tests/test_types.cpp
            tests/traits.test.cpp
            tests/validate.test.cpp
            tests/validators.test.cpp
    )

    find_package(reflex-tool)
    include(ReflexCppMetadata)
    reflex_generate_cpp_metadata(
        TARGET reflex_config_tests
        OUTPUT_VARIABLE TEST_METADATA_FILES
        HEADERS tests/test_types.hpp
    )
    add_custom_target(reflex_config_tests_metadata DEPENDS ${TEST_METADATA_FILES})
    add_dependencies(reflex_config_tests reflex_config_tests_metadata)
    target_sources(
        reflex_config_tests
        PRIVATE FILE_SET HEADERS BASE_DIRS "${CMAKE_CURRENT_BINARY_DIR}/tests" FILES ${TEST_METADATA_FILES}
    )

    include(Catch)
    catch_discover_tests(reflex_config_tests)
endif()

install(TARGETS config EXPORT reflex-configTargets FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    install(
        FILES "${CMAKE_BINARY_DIR}/${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/reflex_config.pdb"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endif()
if(REFLEX_CONFIG_INSTALL_CMAKE_CONFIG)
    install(
        EXPORT reflex-configTargets
        FILE reflex-configTargets.cmake
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/reflex-config"
        NAMESPACE reflex::
    )
    include(CMakePackageConfigHelpers)
    configure_package_config_file(
        tools/cmake/reflex-configConfig.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/reflex-configConfig.cmake"
        INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/reflex-config"
    )
    install(
        FILES "${CMAKE_CURRENT_BINARY_DIR}/reflex-configConfig.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/reflex-config"
    )
endif()
