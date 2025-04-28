cmake_minimum_required(VERSION 3.15)
project(multithread_mail_ru)

set(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} )

SET(    CHAPTER_1_SOURCES
        ####--Chapter 1--####
        main.cpp
)

add_executable(test_project.x ${CMAKE_CURRENT_SOURCE_DIR}/source/main.cpp)