add_executable(quack_buffer_test EXCLUDE_FROM_ALL test_stream_buffer.cpp)
target_include_directories(quack_buffer_test
                           PRIVATE ${CMAKE_SOURCE_DIR}/third_party/catch)
if(WIN32)
  target_link_libraries(quack_buffer_test duckdb_static quack_extension)
else()
  target_link_libraries(quack_buffer_test duckdb quack_extension)
endif()

add_custom_target(test_quack_buffer
                  COMMAND $<TARGET_FILE:quack_buffer_test>
                  DEPENDS quack_buffer_test
                  USES_TERMINAL)
