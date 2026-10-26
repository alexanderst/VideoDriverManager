cmake_minimum_required(VERSION 3.16)

project(VideoDriverManager
        VERSION 1.1.0
        LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(VideoDriverManager
        src/main.cpp

        src/DiagnosticsEngine.h
        src/DiagnosticsEngine.cpp

        src/DiagnosticsDialog.h
        src/DiagnosticsDialog.cpp
)

target_link_libraries(VideoDriverManager
        PRIVATE Qt6::Widgets
)

install(TARGETS VideoDriverManager
        RUNTIME DESTINATION bin
)
