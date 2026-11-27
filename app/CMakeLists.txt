cmake_minimum_required(VERSION 3.21)

project(SceneForge VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2)

qt_add_executable(SceneForge WIN32
    src/main.cpp
)

qt_add_qml_module(SceneForge
    URI SceneForge
    VERSION 1.0
    QML_FILES
        qml/Main.qml
)

target_link_libraries(SceneForge
    PRIVATE
        Qt6::Quick
        Qt6::QuickControls2
)
