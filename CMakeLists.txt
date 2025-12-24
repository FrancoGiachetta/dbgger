cmake_minimum_required(VERSION 4.2.1)

project("sdb" LANGUAGES CXX)

# Configure clangd

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Create symlink immediately during configuration
execute_process(
    COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${CMAKE_BINARY_DIR}/compile_commands.json
    ${CMAKE_SOURCE_DIR}/compile_commands.json
)

# Options

option(BUILD_TESTING "Build tests" OFF)

# Packages

include(CTest)
find_package(PkgConfig REQUIRED)

pkg_check_modules(readline REQUIRED IMPORTED_TARGET readline)

if (BUILD_TESTING)
  message("yes")
  find_package(Catch2 CONFIG REQUIRED)
  add_subdirectory("test")
endif()

add_subdirectory("src")
add_subdirectory("tools")
