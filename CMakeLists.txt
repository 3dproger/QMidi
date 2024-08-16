cmake_minimum_required(VERSION 3.14)

project(QMidi VERSION 1.0)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOMOC ON)

find_package(QT NAMES Qt6 Qt5 COMPONENTS Core REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core REQUIRED)

# Common QMidi source files & library
aux_source_directory("${PROJECT_SOURCE_DIR}/src" SOURCES)
set(LIBRARIES Qt${QT_VERSION_MAJOR}::Core)
set(LIBRARY_NAME Qt${QT_VERSION_MAJOR}Midi)

# Platform specific QMidi source files & libraries
if(WIN32)
    set(LIBRARIES ${LIBRARIES} winmm)
    set(SOURCES ${SOURCES} "${PROJECT_SOURCE_DIR}/src/OS/QMidi_Win32.cpp")
elseif(APPLE)
    set(LIBRARIES ${LIBRARIES} "-framework CoreMIDI" "-framework CoreFoundation" "-framework CoreAudio")
    set(SOURCES ${SOURCES} "${PROJECT_SOURCE_DIR}/src/OS/QMidi_CoreMidi.cpp")
elseif(UNIX)
    set(LIBRARIES ${LIBRARIES} asound)
    set(SOURCES ${SOURCES} "${PROJECT_SOURCE_DIR}/src/OS/QMidi_ALSA.cpp")
elseif(HAIKU)
    set(LIBRARIES ${LIBRARIES} midi2)
    set(SOURCES ${SOURCES} "${PROJECT_SOURCE_DIR}/src/OS/QMidi_Haiku.cpp")
endif()

# Add library
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib) # static libs
add_library(${LIBRARY_NAME} ${SOURCES})
target_include_directories(${LIBRARY_NAME} PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(${LIBRARY_NAME} ${LIBRARIES})
