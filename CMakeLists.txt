cmake_minimum_required(VERSION 3.15)
project(multithread_mail_ru)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic ") #-Werror

SET ( CHAPTER_3_SOURCES
        ####--Chapter 3--####
        3_1_Berkeley_sockets_tcp_server
        3_2_Berkeley_sockets_tcp_client
        3_3_MultiPlexServerSelect
        3_4_MultiPlexServerPoll
        3_5_MultiPlexServerEpoll
        3_6_Task_Chat_server
)

foreach(source_file ${CHAPTER_3_SOURCES})
        #message("DEBUG = " ${CMAKE_CURRENT_SOURCE_DIR}/source/Chapter_3/)
        #message("DEBUG = " ${source_file}.cpp)

        add_executable(${source_file}.x ${CMAKE_CURRENT_SOURCE_DIR}/source/Chapter_3/${source_file}.cpp)
endforeach(source_file)


# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fsanitize=thread")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fsanitize=address")