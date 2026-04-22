cmake_minimum_required(VERSION 3.21)

project(audio_system VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(WANT_STATIC ON CACHE BOOL "" FORCE)
set(WANT_PLAYER OFF CACHE BOOL "" FORCE)
set(WANT_PLAYERSTATIC OFF CACHE BOOL "" FORCE)
set(WANT_DEVTEST OFF CACHE BOOL "" FORCE)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/deps/wildmidi")

file(GLOB SOLOUD_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/core/*.cpp"
)

set(SOLOUD_WAV_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/audiosource/wav/dr_impl.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/audiosource/wav/soloud_wav.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/audiosource/wav/stb_vorbis.c"
)

add_library(audio_system_runtime STATIC
    ${SOLOUD_CORE_SOURCES}
    ${SOLOUD_WAV_SOURCES}
    "${CMAKE_CURRENT_SOURCE_DIR}/src/soloud_miniaudio_backend.cpp"
)

target_compile_definitions(audio_system_runtime
    PRIVATE
        WITH_MINIAUDIO
)

target_include_directories(audio_system_runtime
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/include"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/audiosource/wav"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/miniaudio"
)

if(MSVC)
    target_compile_definitions(audio_system_runtime PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(audio_system_runtime PUBLIC pthread m dl)
endif()

add_executable(audio_system_demo
    "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
)

target_link_libraries(audio_system_demo PRIVATE audio_system_runtime)

target_compile_definitions(audio_system_demo
    PRIVATE
        AUDIO_SYSTEM_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

add_executable(audio_system_midi_demo
    "${CMAKE_CURRENT_SOURCE_DIR}/src/midi_demo.cpp"
)

target_compile_definitions(audio_system_midi_demo
    PRIVATE
        AUDIO_SYSTEM_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(audio_system_midi_demo
    PRIVATE
        audio_system_runtime
        libwildmidi-static
)
