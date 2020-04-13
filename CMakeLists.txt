cmake_minimum_required(VERSION 3.15)
project(coco)

set(CMAKE_CXX_STANDARD 17)

include_directories(include)


#set(SRC_LIST main.cpp classencoder.cpp common.cpp pattern.cpp classdecoder.cpp algorithms.cpp SpookyV2.cpp patternmodel.cpp include/ner.cpp include/ner.h)

set(SRC_LIST classencoder.cpp common.cpp pattern.cpp classdecoder.cpp algorithms.cpp SpookyV2.cpp patternmodel.cpp ner.cpp bow.cpp bowmodel.cpp nermodel.cpp)


#  follow static link only effective for gcc, not clang
#set(CMAKE_CXX_STANDARD_LIBRARIES "-static-libgcc -static-libstdc++ -lwsock32 -lws2_32 ${CMAKE_CXX_STANDARD_LIBRARIES}")
##
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive")
#

#set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++ -static")

#add_executable(bowpipeline ${SRC_LIST} bowpipeline.cpp)

add_executable(pipeline ${SRC_LIST} pipeline.cpp)

add_executable(main ${SRC_LIST} main.cpp)

add_executable(test_ner ${SRC_LIST} test_ner.cpp)