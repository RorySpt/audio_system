cmake_minimum_required(VERSION 3.21)

project(audio_system VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(AUDIO_SYSTEM_BUILD_SOLOUD_OFFICIAL_DEMOS "Build selected SoLoud official demo targets" ON)
option(AUDIO_SYSTEM_BUILD_SOLOUD_GRAPHICAL_DEMOS "Build SDL2 + ImGui SoLoud official graphical demos" ON)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/deps/SDL2")

file(GLOB SOLOUD_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/core/*.cpp"
)

set(SOLOUD_WAV_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/audiosource/wav/dr_impl.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/audiosource/wav/soloud_wav.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/audiosource/wav/soloud_wavstream.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/audiosource/wav/stb_vorbis.c"
)

set(SOLOUD_SPEECH_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/audiosource/speech/darray.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/audiosource/speech/klatt.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/audiosource/speech/resonator.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/audiosource/speech/soloud_speech.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/audiosource/speech/tts.cpp"
)

set(SOLOUD_OPENMPT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/audiosource/openmpt/soloud_openmpt.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/audiosource/openmpt/soloud_openmpt_dll.c"
)

set(SOLOUD_MISC_AUDIOSOURCE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/audiosource/ay/chipplayer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/audiosource/ay/sndbuffer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/audiosource/ay/sndchip.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/audiosource/ay/sndrender.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/audiosource/ay/soloud_ay.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/audiosource/monotone/soloud_monotone.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/audiosource/noise/soloud_noise.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/audiosource/sfxr/soloud_sfxr.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/audiosource/tedsid/sid.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/audiosource/tedsid/soloud_tedsid.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/audiosource/tedsid/ted.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/audiosource/vic/soloud_vic.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/audiosource/vizsn/soloud_vizsn.cpp"
)

set(SOLOUD_BACKEND_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/backend/miniaudio/soloud_miniaudio.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/backend/null/soloud_null.cpp"
)

set(SOLOUD_FILTER_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/filter/soloud_bassboostfilter.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/filter/soloud_biquadresonantfilter.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/filter/soloud_dcremovalfilter.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/filter/soloud_duckfilter.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/filter/soloud_echofilter.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/filter/soloud_eqfilter.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/filter/soloud_fftfilter.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/filter/soloud_flangerfilter.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/filter/soloud_freeverbfilter.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/filter/soloud_lofifilter.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/filter/soloud_robotizefilter.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/filter/soloud_waveshaperfilter.cpp"
)

add_library(audio_system_runtime STATIC
    ${SOLOUD_CORE_SOURCES}
    ${SOLOUD_WAV_SOURCES}
    ${SOLOUD_SPEECH_SOURCES}
    ${SOLOUD_OPENMPT_SOURCES}
    ${SOLOUD_MISC_AUDIOSOURCE_SOURCES}
    ${SOLOUD_BACKEND_SOURCES}
    ${SOLOUD_FILTER_SOURCES}
)

target_compile_definitions(audio_system_runtime
    PRIVATE
        WITH_MINIAUDIO
        WITH_NULL
)

target_include_directories(audio_system_runtime
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/include"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/audiosource/wav"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/audiosource/speech"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/audiosource/openmpt"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/audiosource/ay"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/audiosource/monotone"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/audiosource/noise"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/audiosource/sfxr"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/audiosource/tedsid"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/audiosource/vic"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/audiosource/vizsn"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/backend/miniaudio"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/backend/null"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/src/filter"
)

if(MSVC)
    target_compile_definitions(audio_system_runtime PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(audio_system_runtime PRIVATE /utf-8)
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(audio_system_runtime PUBLIC pthread m dl)
endif()

add_library(audio_system_soloud_demo_framework STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/demos/common/soloud_demo_framework.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/demos/common/imgui/imgui.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/demos/common/imgui/imgui_draw.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/demos/common/imgui/imgui_demo.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/demos/common/glew/GL/glew.c"
)

target_include_directories(audio_system_soloud_demo_framework
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/demos/common"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/demos/common/imgui"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/demos/common/glew"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/SDL2/include"
)

target_link_libraries(audio_system_soloud_demo_framework
    PUBLIC
        audio_system_runtime
        SDL2::SDL2main
        SDL2::SDL2-static
        opengl32
)

target_compile_definitions(audio_system_soloud_demo_framework PRIVATE GLEW_STATIC)

if(MSVC)
    target_compile_options(audio_system_soloud_demo_framework PRIVATE /utf-8)
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

if(AUDIO_SYSTEM_BUILD_SOLOUD_OFFICIAL_DEMOS)
    function(audio_system_add_soloud_official_demo demo_name source_path)
        set(target_name "audio_system_soloud_${demo_name}")
        add_executable("${target_name}" "${source_path}")
        target_link_libraries("${target_name}" PRIVATE audio_system_runtime)

        if(MSVC)
            target_compile_options("${target_name}" PRIVATE /utf-8)
        endif()
    endfunction()

    audio_system_add_soloud_official_demo(
        enumerate
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/demos/enumerate/main.cpp"
    )
    audio_system_add_soloud_official_demo(
        null
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/demos/null/main.cpp"
    )
    audio_system_add_soloud_official_demo(
        simplest
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/demos/simplest/main.cpp"
    )
    audio_system_add_soloud_official_demo(
        welcome
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/demos/welcome/main.cpp"
    )

    set(SOLOUD_WELCOME_AMBIENCE_SOURCE
        "${CMAKE_CURRENT_SOURCE_DIR}/data/soloud_official/audio/windy_ambience.ogg"
    )
    set(SOLOUD_WELCOME_BRUCE_SOURCE
        "${CMAKE_CURRENT_SOURCE_DIR}/data/soloud_official/audio/BRUCE.S3M"
    )

    if(EXISTS "${SOLOUD_WELCOME_AMBIENCE_SOURCE}")
        add_custom_command(
            TARGET audio_system_soloud_welcome POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:audio_system_soloud_welcome>/audio"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SOLOUD_WELCOME_AMBIENCE_SOURCE}"
                "$<TARGET_FILE_DIR:audio_system_soloud_welcome>/audio/windy_ambience.ogg"
        )
    endif()

    if(EXISTS "${SOLOUD_WELCOME_BRUCE_SOURCE}")
        add_custom_command(
            TARGET audio_system_soloud_welcome POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:audio_system_soloud_welcome>/audio"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SOLOUD_WELCOME_BRUCE_SOURCE}"
                "$<TARGET_FILE_DIR:audio_system_soloud_welcome>/audio/BRUCE.S3M"
        )
    endif()

    if(AUDIO_SYSTEM_BUILD_SOLOUD_GRAPHICAL_DEMOS)
        function(audio_system_add_soloud_graphical_demo demo_name)
            set(target_name "audio_system_soloud_${demo_name}")
            add_executable("${target_name}" ${ARGN})
            target_link_libraries("${target_name}" PRIVATE audio_system_soloud_demo_framework)

            if(MSVC)
                target_compile_options("${target_name}" PRIVATE /utf-8)
            endif()

            add_custom_command(
                TARGET "${target_name}" POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target_name}>/audio"
                COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target_name}>/graphics"
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${CMAKE_CURRENT_SOURCE_DIR}/data/soloud_official/audio"
                    "$<TARGET_FILE_DIR:${target_name}>/audio"
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${CMAKE_CURRENT_SOURCE_DIR}/data/soloud_official/graphics"
                    "$<TARGET_FILE_DIR:${target_name}>/graphics"
            )
        endfunction()

        audio_system_add_soloud_graphical_demo(
            env
            "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/demos/env/main.cpp"
        )

        audio_system_add_soloud_graphical_demo(
            megademo
            "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/demos/megademo/3dtest.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/demos/megademo/annex.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/demos/megademo/ay.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/demos/megademo/filterfolio.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/demos/megademo/main.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/demos/megademo/mixbusses.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/demos/megademo/monotone.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/demos/megademo/multimusic.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/demos/megademo/pewpew.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/demos/megademo/radiogaga.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/demos/megademo/space.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/demos/megademo/speakers.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/demos/megademo/speechfilter.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/demos/megademo/tedsid.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/demos/megademo/thebutton.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/demos/megademo/virtualvoices.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/deps/soloud/demos/megademo/wavformats.cpp"
        )

        if(MSVC)
            target_compile_options(audio_system_soloud_megademo PRIVATE /Zc:strictStrings-)
        endif()
    endif()
endif()
