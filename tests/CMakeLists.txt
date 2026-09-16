add_executable(natural_test test_natural.cpp)
target_link_libraries(natural_test PRIVATE natural::natural)
add_test(NAME natural_selfcheck COMMAND natural_test)
