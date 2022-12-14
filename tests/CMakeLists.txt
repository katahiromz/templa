# wildcard.exe
add_executable(wildcard wildcard.cpp)
target_link_libraries(wildcard libtempla)

# wildcard_test
add_test(NAME wildcard_test COMMAND $<TARGET_FILE:wildcard>)
