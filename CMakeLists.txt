cmake_minimum_required(VERSION 3.21)
project(SimpleScope LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia)

# All sources including FTDI
set(SOURCES
    src/main.cpp
    src/scpView.h
    src/scpMainWindow.h
    src/scpMainWindow.cpp
    src/scpScopeView.h
    src/scpScopeView.cpp
    src/scpViewTerminal.h
    src/scpViewTerminal.cpp
    src/scpDataSource.h
    src/scpAudioInputSource.h
    src/scpAudioInputSource.cpp
    src/scpSignalGeneratorSource.h
    src/scpSignalGeneratorSource.cpp
    src/scpFtdiSource.h
    src/scpFtdiSource.cpp
    src/scpMessageWaveSource.h
    src/scpMessageWaveSource.cpp
)

qt_add_executable(SimpleScope ${SOURCES})

# Include headers
target_include_directories(SimpleScope PRIVATE "${CMAKE_SOURCE_DIR}/include"
                                        "${CMAKE_SOURCE_DIR}")  # for ftd2xx.h if in root

# Link Qt libraries
target_link_libraries(SimpleScope PRIVATE Qt6::Widgets Qt6::Multimedia)

# Link FTDI library (MinGW import library)
target_link_libraries(SimpleScope PRIVATE "${CMAKE_SOURCE_DIR}/amd64/libftd2xx.a")

install(TARGETS SimpleScope)
