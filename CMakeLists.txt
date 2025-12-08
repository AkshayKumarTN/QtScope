cmake_minimum_required(VERSION 3.21)
project(SimpleScope LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia)

# All sources
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
    src/scpSimulatedGeneratorSource.h
    src/scpSimulatedGeneratorSource.cpp
    src/scpSimulatedAcquisitionSource.h
    src/scpSimulatedAcquisitionSource.cpp
    src/scpMessageWaveSource.h
    src/scpMessageWaveSource.cpp
    src/scpTerminalController.h
    src/scpTerminalController.cpp
    src/scpUsbReadController.h
    src/scpUsbReadController.cpp
    src/scpUsbWriteController.h
    src/scpUsbWriteController.cpp
    src/scpWaveformGenerator.h
    src/scpWaveformGenerator.cpp
    src/scpThroughputMonitor.h
    src/scpThroughputMonitor.cpp
    src/scpFTDIInterface.h
    src/scpFTDIInterface.cpp
)

# FTDI sources only on Windows
if(WIN32)
    list(APPEND SOURCES
        src/scpFtdiSource.h
        src/scpFtdiSource.cpp
    )
endif()

qt_add_executable(SimpleScope ${SOURCES})

# Include headers
target_include_directories(SimpleScope PRIVATE "${CMAKE_SOURCE_DIR}/include"
                                        "${CMAKE_SOURCE_DIR}")  # for ftd2xx.h if in root

# Link Qt libraries
target_link_libraries(SimpleScope PRIVATE Qt6::Widgets Qt6::Multimedia)

# Link FTDI library (MinGW import library) - optional, only if building with FTDI support
# Uncomment if FTDI support is needed:
# target_link_libraries(SimpleScope PRIVATE "${CMAKE_SOURCE_DIR}/amd64/libftd2xx.a")

install(TARGETS SimpleScope)

