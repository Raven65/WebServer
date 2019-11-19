add_executable(LoggingTest LoggingTest.cpp)
target_link_libraries(LoggingTest base)

add_executable(test01 test01.cpp)
target_link_libraries(test01 net)

add_executable(test02 test02.cpp)
target_link_libraries(test02 web)
