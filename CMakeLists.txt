cmake_minimum_required(VERSION 3.21)

project(audio_system VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/backend/miniaudio/soloud_miniaudio.cpp"
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
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/backend/miniaudio"
)

if(MSVC)
    target_compile_definitions(audio_system_runtime PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(audio_system_runtime PRIVATE /utf-8)
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

if(MSVC)
    target_compile_options(audio_system_demo PRIVATE /utf-8)
endif()

add_executable(audio_system_play3d_demo
    "${CMAKE_CURRENT_SOURCE_DIR}/src/play3d_demo.cpp"
)

target_link_libraries(audio_system_play3d_demo PRIVATE audio_system_runtime)

target_compile_definitions(audio_system_play3d_demo
    PRIVATE
        AUDIO_SYSTEM_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

if(MSVC)
    target_compile_options(audio_system_play3d_demo PRIVATE /utf-8)
endif()
