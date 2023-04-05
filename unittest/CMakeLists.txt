
# build threadpool_test
include_directories(../cpputil)
aux_source_directory(../cpputil SRCS)
add_executable(threadpool_test threadpool_test.cpp ${SRCS})