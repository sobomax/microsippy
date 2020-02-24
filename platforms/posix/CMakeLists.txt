cmake_minimum_required(VERSION 3.5)
project (usipy_test)
find_package (Threads)

add_executable(usipy_test usipy_test.c)
add_subdirectory(../../src microsippy)
target_include_directories(usipy_test PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src
)
target_include_directories(microsippy PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(usipy_test PRIVATE
  microsippy
  ${CMAKE_THREAD_LIBS_INIT}
)
set_property(TARGET microsippy PROPERTY C_STANDARD 11)
set_property(TARGET usipy_test PROPERTY C_STANDARD 11)
