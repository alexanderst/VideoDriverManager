cmake_minimum_required(VERSION 3.16)
project(VideoDriverManager LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
include(CTest)
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Test)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Test)

add_executable(VideoDriverManager
    src/main.cpp
    src/DiagnosticsDialog.cpp
    src/DiagnosticsDialog.h
    src/DiagnosticsEngine.cpp
    src/DiagnosticsEngine.h
)
target_link_libraries(VideoDriverManager PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

if(BUILD_TESTING)
    add_executable(tst_diagnostics
        src/tests/tst_diagnostics.cpp
        src/DiagnosticsDialog.cpp
        src/DiagnosticsDialog.h
        src/DiagnosticsEngine.cpp
        src/DiagnosticsEngine.h
    )
    target_link_libraries(tst_diagnostics PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Test
    )
    target_compile_definitions(tst_diagnostics PRIVATE
        APP_EXECUTABLE="$<TARGET_FILE:VideoDriverManager>"
    )
    add_dependencies(tst_diagnostics VideoDriverManager)
    add_test(NAME diagnostics_unit_tests COMMAND tst_diagnostics)
    set_tests_properties(diagnostics_unit_tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 90
    )
endif()
