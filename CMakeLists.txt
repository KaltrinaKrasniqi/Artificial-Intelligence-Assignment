cmake_minimum_required(VERSION 3.16)
project(ProjektAI LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
enable_testing()

function(configure_program target)
  target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/shared")
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  add_test(NAME ${target}_tests COMMAND ${target} --test)
endfunction()

foreach(problem IN ITEMS social_golfers sudoku latin_square)
  add_executable(${problem}
    problems/${problem}/main.cpp
    problems/${problem}/solver.cpp
    problems/${problem}/validator.cpp
    problems/${problem}/tests.cpp)
  configure_program(${problem})
  add_executable(${problem}_landscape
    problems/${problem}/landscape.cpp
    problems/${problem}/validator.cpp)
  if(NOT problem STREQUAL "social_golfers")
    target_sources(${problem}_landscape PRIVATE problems/${problem}/solver.cpp)
  endif()
  configure_program(${problem}_landscape)
endforeach()

add_executable(shared_tests tests/shared_tests.cpp)
target_include_directories(shared_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME shared_helpers_tests COMMAND shared_tests)
