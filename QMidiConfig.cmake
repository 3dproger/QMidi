set(QMidi_VERSION 1.0)

get_filename_component(PREFIX ${CMAKE_CURRENT_LIST_FILE} DIRECTORY)

find_package(QT NAMES Qt6 Qt5 COMPONENTS Core REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core REQUIRED)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(QMidi_INCLUDE_DIR ${PREFIX}/src CACHE PATH "QMidi Include dir")
set(QMidi_LIBRARIES ${PREFIX}/lib/Qt${QT_VERSION_MAJOR}Midi.lib)

set(QMIDI_SRCDIR ${PREFIX}/src)
set(QMIDI_OSDIR  ${PREFIX}/src/OS)

set(QMidi_INTERFACE
    ${QMIDI_SRCDIR}/QMidiFile.cpp
    ${QMIDI_SRCDIR}/QMidiFile.h
    ${QMIDI_SRCDIR}/QMidiIn.cpp
    ${QMIDI_SRCDIR}/QMidiIn.h
    ${QMIDI_SRCDIR}/QMidiOut.cpp
    ${QMIDI_SRCDIR}/QMidiOut.h
)

set(QMIDI_LIBRARIES Qt${QT_VERSION_MAJOR}::Core)

# Platform specific QMidi source files & libraries
if(WIN32)
    set(QMidi_LIBRARIES ${QMidi_LIBRARIES} winmm)
    set(QMidi_INTERFACE ${QMidi_INTERFACE} "${QMIDI_OSDIR}/QMidi_Win32.cpp")
elseif(APPLE)
    set(QMidi_LIBRARIES ${QMidi_LIBRARIES} "-framework CoreMIDI" "-framework CoreFoundation" "-framework CoreAudio")
    set(QMidi_INTERFACE ${QMidi_INTERFACE} "${QMIDI_OSDIR}/QMidi_CoreMidi.cpp")
elseif(UNIX)
    set(QMidi_LIBRARIES ${QMidi_LIBRARIES} asound)
    set(QMidi_INTERFACE ${QMidi_INTERFACE} "${QMIDI_OSDIR}/QMidi_ALSA.cpp")
elseif(HAIKU)
    set(QMidi_LIBRARIES ${QMidi_LIBRARIES} midi2)
    set(QMidi_INTERFACE ${QMidi_INTERFACE} "${QMIDI_OSDIR}/QMidi_Haiku.cpp")
endif()

