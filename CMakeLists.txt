# CMakeLists.txt
# BTHL-SpiritBox - Beyond The Horizon Labs Spirit Box
# Author: David St John (davestj)
# Date: 2026-03-30
# Purpose: Multi-sensor paranormal audio detection system using SDR, EMF, and AI transcription
# License: MIT
#
# CHANGELOG:
# 2026-03-30 - Initial project scaffold with SoapySDR, Qt6, whisper.cpp, PortAudio

cmake_minimum_required(VERSION 3.21)

project(BTHL-SpiritBox
    VERSION 1.0.0
    DESCRIPTION "Multi-sensor paranormal audio detection and transcription system"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# We set the default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# We define our compile options for all targets
add_compile_options(-Wall -Wextra -Wpedantic)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -O0)
else()
    add_compile_options(-O2)
endif()

# ─── Dependencies ───────────────────────────────────────────────────────────

# Qt6 - We use Widgets, Multimedia, SerialPort, Charts, and Concurrent
find_package(Qt6 REQUIRED COMPONENTS
    Widgets
    Multimedia
    SerialPort
    Charts
    Concurrent
    Core
)

# SoapySDR - We use the multi-device abstraction layer for SDR hardware
find_package(SoapySDR REQUIRED)

# PortAudio - We use this for low-latency real-time audio output
find_package(PkgConfig REQUIRED)
pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)

# whisper.cpp - We build this as a submodule for local speech-to-text
# Users should clone whisper.cpp into lib/whisper.cpp
set(WHISPER_DIR ${CMAKE_SOURCE_DIR}/lib/whisper.cpp)
if(EXISTS ${WHISPER_DIR}/CMakeLists.txt)
    set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory(${WHISPER_DIR} ${CMAKE_BINARY_DIR}/whisper)
    set(WHISPER_FOUND TRUE)
    message(STATUS "whisper.cpp found and will be built from source")
else()
    set(WHISPER_FOUND FALSE)
    message(WARNING "whisper.cpp not found in lib/whisper.cpp - transcription will be disabled")
    message(WARNING "Run: git clone https://github.com/ggerganov/whisper.cpp.git lib/whisper.cpp")
endif()

# ─── Source Files ───────────────────────────────────────────────────────────

set(CORE_SOURCES
    src/core/SweepEngine.cpp
    src/core/SweepProfile.cpp
    src/core/AudioDemodulator.cpp
)

set(AUDIO_SOURCES
    src/audio/AudioRingBuffer.cpp
    src/audio/AudioOutputManager.cpp
)

set(DETECTION_SOURCES
    src/detection/VoiceActivityDetector.cpp
    src/detection/WhisperTranscriber.cpp
)

set(EMF_SOURCES
    src/emf/EMFSerialReader.cpp
    src/emf/EMFCorrelator.cpp
)

set(SESSION_SOURCES
    src/session/SessionRecorder.cpp
    src/session/SessionPlayer.cpp
)

set(UI_SOURCES
    src/ui/MainWindow.cpp
    src/ui/SweepVisualizerWidget.cpp
    src/ui/EMFTimelineWidget.cpp
    src/ui/AudioWaveformWidget.cpp
    src/ui/DetectionLogWidget.cpp
)

set(SENSOR_SOURCES
    src/sensors/SensorDevice.cpp
    src/sensors/USBDeviceManager.cpp
)

set(HUB_SOURCES
    src/hub/SensorHub.cpp
)

set(ALL_SOURCES
    src/main.cpp
    ${CORE_SOURCES}
    ${AUDIO_SOURCES}
    ${DETECTION_SOURCES}
    ${EMF_SOURCES}
    ${SESSION_SOURCES}
    ${SENSOR_SOURCES}
    ${HUB_SOURCES}
    ${UI_SOURCES}
)

# ─── Executable ─────────────────────────────────────────────────────────────

add_executable(${PROJECT_NAME} ${ALL_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${SoapySDR_INCLUDE_DIRS}
    ${PORTAUDIO_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::SerialPort
    Qt6::Charts
    Qt6::Concurrent
    Qt6::Core
    SoapySDR
    ${PORTAUDIO_LINK_LIBRARIES}
)

target_link_directories(${PROJECT_NAME} PRIVATE
    ${PORTAUDIO_LIBRARY_DIRS}
)

# We conditionally link whisper.cpp if it was found
if(WHISPER_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE whisper)
    target_include_directories(${PROJECT_NAME} PRIVATE ${WHISPER_DIR}/include)
    target_compile_definitions(${PROJECT_NAME} PRIVATE WHISPER_AVAILABLE=1)
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE WHISPER_AVAILABLE=0)
endif()

# We link platform-specific audio frameworks on macOS
if(APPLE)
    find_library(COREAUDIO_LIB CoreAudio REQUIRED)
    find_library(AUDIOUNIT_LIB AudioUnit REQUIRED)
    find_library(AUDIOTOOLBOX_LIB AudioToolbox REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${COREAUDIO_LIB}
        ${AUDIOUNIT_LIB}
        ${AUDIOTOOLBOX_LIB}
    )
endif()

# ─── Install ────────────────────────────────────────────────────────────────

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
install(DIRECTORY profiles/ DESTINATION share/${PROJECT_NAME}/profiles)

# ─── Summary ────────────────────────────────────────────────────────────────

message(STATUS "")
message(STATUS "╔══════════════════════════════════════════════╗")
message(STATUS "║  BTHL-SpiritBox v${PROJECT_VERSION}                      ║")
message(STATUS "║  Beyond The Horizon Labs                     ║")
message(STATUS "╠══════════════════════════════════════════════╣")
message(STATUS "║  Qt6:        ${Qt6_VERSION}")
message(STATUS "║  SoapySDR:   Found")
message(STATUS "║  PortAudio:  ${PORTAUDIO_VERSION}")
message(STATUS "║  Whisper:    ${WHISPER_FOUND}")
message(STATUS "║  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "╚══════════════════════════════════════════════╝")
message(STATUS "")
