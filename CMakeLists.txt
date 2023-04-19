CMAKE_MINIMUM_REQUIRED(VERSION 3.12)
PROJECT(ClienServerEcn)

SET(Boost_INCLUDE_DIR "C:/Program Files/Boost/boost_1_82_0")

FIND_PACKAGE(Boost 1.40 REQUIRED)
FIND_PACKAGE(Threads REQUIRED)


INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIR})

SET(CMAKE_CXX_STANDARD 17)

ADD_EXECUTABLE(Server Server.cpp Common.hpp json.hpp)
TARGET_LINK_LIBRARIES(Server PRIVATE Threads::Threads ${Boost_LIBRARIES})

ADD_EXECUTABLE(Client Client.cpp ClientClass.hpp Common.hpp json.hpp)
TARGET_LINK_LIBRARIES(Client PRIVATE Threads::Threads ${Boost_LIBRARIES})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/test.txt
    ${CMAKE_CURRENT_BINARY_DIR} COPYONLY)

ADD_EXECUTABLE(ClientDummy ClientDummy.cpp ClientClass.hpp Common.hpp)
TARGET_LINK_LIBRARIES(Client PRIVATE Threads::Threads ${Boost_LIBRARIES})