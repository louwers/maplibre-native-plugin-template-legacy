cmake_minimum_required(VERSION 3.25 FATAL_ERROR)

project(maplibre-native-plugin-render-tests LANGUAGES C CXX)
enable_testing()

set(MAPLIBRE_NATIVE_SOURCE_DIR "" CACHE PATH "Path to a plugin-enabled MapLibre Native checkout")
if(NOT MAPLIBRE_NATIVE_SOURCE_DIR)
    message(FATAL_ERROR "Set MAPLIBRE_NATIVE_SOURCE_DIR to a plugin-enabled MapLibre Native checkout")
endif()
get_filename_component(MAPLIBRE_NATIVE_SOURCE_DIR "${MAPLIBRE_NATIVE_SOURCE_DIR}" ABSOLUTE)
if(NOT EXISTS "${MAPLIBRE_NATIVE_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "MAPLIBRE_NATIVE_SOURCE_DIR does not contain MapLibre Native")
endif()

set(plugin_backend_count 0)
foreach(backend OPENGL VULKAN METAL)
    if(MLN_WITH_${backend})
        math(EXPR plugin_backend_count "${plugin_backend_count} + 1")
    endif()
endforeach()
if(NOT plugin_backend_count EQUAL 1)
    message(FATAL_ERROR "Select exactly one plugin backend: MLN_WITH_OPENGL, MLN_WITH_VULKAN, or MLN_WITH_METAL")
endif()

set(MLN_WITH_PLUGINS ON CACHE BOOL "" FORCE)
set(MLN_WITH_GLFW OFF CACHE BOOL "" FORCE)
set(MLN_WITH_WERROR OFF CACHE BOOL "" FORCE)
add_subdirectory("${MAPLIBRE_NATIVE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/maplibre-native")

add_executable(plugin-render-tests render-tests/main.cpp)

target_compile_features(plugin-render-tests PRIVATE cxx_std_20)
target_include_directories(plugin-render-tests PRIVATE render-tests)
target_link_libraries(plugin-render-tests PRIVATE mbgl-render-test)

function(mln_add_plugin_render_tests)
    set(options)
    set(one_value_args NAME)
    set(multi_value_args SOURCES INCLUDE_DIRECTORIES COMPILE_DEFINITIONS LINK_LIBRARIES)
    cmake_parse_arguments(PLUGIN "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})
    if(NOT PLUGIN_NAME OR NOT PLUGIN_SOURCES)
        message(FATAL_ERROR "mln_add_plugin_render_tests requires NAME and SOURCES")
    endif()

    target_sources(plugin-render-tests PRIVATE ${PLUGIN_SOURCES})
    if(PLUGIN_INCLUDE_DIRECTORIES)
        target_include_directories(plugin-render-tests PRIVATE ${PLUGIN_INCLUDE_DIRECTORIES})
    endif()
    if(PLUGIN_COMPILE_DEFINITIONS)
        target_compile_definitions(plugin-render-tests PRIVATE ${PLUGIN_COMPILE_DEFINITIONS})
    endif()
    if(PLUGIN_LINK_LIBRARIES)
        target_link_libraries(plugin-render-tests PRIVATE ${PLUGIN_LINK_LIBRARIES})
    endif()
    message(STATUS "Added plugin render tests: ${PLUGIN_NAME}")
endfunction()

file(
    GLOB plugin_render_test_build_files
    CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "plugins/*/render-tests/CMakeLists.txt"
)
list(SORT plugin_render_test_build_files)
if(NOT plugin_render_test_build_files)
    message(FATAL_ERROR "No plugin render-test CMake definitions were found")
endif()
foreach(plugin_build_file IN LISTS plugin_render_test_build_files)
    get_filename_component(plugin_render_test_dir "${plugin_build_file}" DIRECTORY)
    get_filename_component(plugin_dir "${plugin_render_test_dir}" DIRECTORY)
    get_filename_component(plugin_name "${plugin_dir}" NAME)
    add_subdirectory("${plugin_render_test_dir}" "plugin-render-test-definitions/${plugin_name}")
endforeach()
