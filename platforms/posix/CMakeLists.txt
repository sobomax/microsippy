cmake_minimum_required(VERSION 3.5)
project (usipy_test)
find_package (Threads)

include (TestBigEndian)
TEST_BIG_ENDIAN(IS_BIG_ENDIAN)

add_executable(usipy_test "usipy_test.c" "usipy_log.c" "usipy_stdout_watch.c")
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

IF (IS_BIG_ENDIAN)
    target_compile_definitions(microsippy PRIVATE -DUSIPY_BIGENDIAN=1)
ELSE (IS_BIG_ENDIAN)
    target_compile_definitions(microsippy PRIVATE -DUSIPY_BIGENDIAN=0)
ENDIF (IS_BIG_ENDIAN)

enable_testing()

add_test(basic ${CMAKE_SOURCE_DIR}/../../scripts/do-test.sh)

#set_target_properties(microsippy PROPERTIES COMPILE_FLAGS "-O2")
#set_target_properties(usipy_test PROPERTIES COMPILE_FLAGS "-O2")
