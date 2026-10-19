cmake_minimum_required(VERSION 3.20)
project(reciv C)

# reciv: Sid Meier's Civilization (DOS 474.05), built natively.

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_EXTENSIONS ON)
option(RECIV_MSVC "Enable the MSVC build" OFF)
if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    if(NOT RECIV_MSVC)
        message(FATAL_ERROR "MSVC is opt-in: configure with -DRECIV_MSVC=ON")
    endif()
    if(CMAKE_C_COMPILER_VERSION VERSION_LESS 19.28)
        message(FATAL_ERROR "MSVC requires Visual Studio 2019 16.8+ for C17")
    endif()
    set(RECIV_WARNINGS /W3)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS)
elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU" AND NOT CMAKE_C_COMPILER_VERSION VERSION_LESS 14)
    set(RECIV_WARNINGS -Wall -Wextra)
else()
    message(FATAL_ERROR "Use GCC 14+ or opt-in MSVC (-DRECIV_MSVC=ON)")
endif()
find_package(SDL3 REQUIRED CONFIG)

# RECIV_ASAN enables AddressSanitizer for the Linux GCC build.
option(RECIV_ASAN "Build with AddressSanitizer" OFF)
if(RECIV_ASAN)
    if(WIN32)
        message(FATAL_ERROR "RECIV_ASAN currently supports the Linux GCC build")
    endif()
    add_compile_options(-fsanitize=address -fsanitize-recover=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

# The game modules and the few platform files that define game data are
# compiled as period code:
# src/period/prelude.h gives them MSC 5.1's int and long, and they see only
# src/period/include, never the host's headers. -fwrapv: MSC arithmetic
# wraps. -fpermissive: the period code's pointer/integer mixing stays a
# warning (GCC 14 made it an error).
file(GLOB GAME_SOURCES CONFIGURE_DEPENDS src/game/*.C)
# src/platform/resident_names.c defines variables the resident layer exposed
# only by address.
set(PERIOD_SOURCES ${GAME_SOURCES} src/platform/resident_names.c src/platform/stubs.c
    src/platform/layout_check.c)
set_source_files_properties(${GAME_SOURCES} PROPERTIES LANGUAGE C)
add_library(period OBJECT ${PERIOD_SOURCES})
if(MSVC)
    # The original code assumes wrapping arithmetic, which MSVC does not
    # promise; build the period code unoptimized so it is not exploited.
    target_compile_options(period PRIVATE /TC /X /Oi- /Od /Ob0 /w /utf-8
        "/FI${CMAKE_CURRENT_SOURCE_DIR}/src/period/prelude.h")
else()
    target_compile_options(period PRIVATE
        -x c -nostdinc -fno-builtin -fwrapv -fno-strict-aliasing -fsigned-char
        -fpermissive -w
        -include "${CMAKE_CURRENT_SOURCE_DIR}/src/period/prelude.h")
endif()
target_include_directories(period PRIVATE src/period/include src/game)

# Image libraries are host code, isolated from all period prelude settings.
add_library(reciv-filters STATIC
    src/platform/filters.c
    third_party/hqx/init.c
    third_party/hqx/hq4x.c)
target_include_directories(reciv-filters PRIVATE third_party/hqx)
target_compile_definitions(reciv-filters PRIVATE HQX_STATIC)
if(MSVC)
    target_compile_options(reciv-filters PRIVATE /w)
else()
    target_compile_options(reciv-filters PRIVATE -w)
endif()

# The host side: the period C run-time and the platform, ordinary C.
add_library(reciv-core STATIC
    $<TARGET_OBJECTS:period>
    src/platform/asound-sequence.c
    src/platform/asound-voice.c
    src/platform/asound-command.c
    src/crt/crt.c
    src/platform/dos.c
    src/platform/files.c
    src/platform/keyboard.c
    src/platform/mem.c
    src/platform/mgraphic.c
    src/platform/period.c
    src/platform/picture.c
    src/platform/resident.c
    src/platform/sound.c
    src/platform/timer.c
    src/platform/video.c
    third_party/nuked-opl3/opl3.c)
target_compile_options(reciv-core PRIVATE ${RECIV_WARNINGS})
target_include_directories(reciv-core PRIVATE third_party/nuked-opl3)
if(MSVC)
    set_source_files_properties(third_party/nuked-opl3/opl3.c PROPERTIES COMPILE_OPTIONS "/w")
else()
    set_source_files_properties(third_party/nuked-opl3/opl3.c PROPERTIES COMPILE_OPTIONS "-w")
endif()
target_link_libraries(reciv-core PUBLIC SDL3::SDL3 reciv-filters)
add_executable(reciv src/platform/main.c)
target_compile_options(reciv PRIVATE ${RECIV_WARNINGS})
target_link_libraries(reciv PRIVATE reciv-core)
if(WIN32)
    # Keep compiler and thread runtimes inside the executable; bundle only SDL.
    if(MINGW)
        target_link_options(reciv-core INTERFACE -static)
    endif()
    if(TARGET SDL3::SDL3-shared)
        add_custom_command(TARGET reciv POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:SDL3::SDL3-shared> $<TARGET_FILE_DIR:reciv>
            VERBATIM)
        install(FILES $<TARGET_FILE:SDL3::SDL3-shared> DESTINATION .)
    endif()
    install(FILES README.md DESTINATION .)
endif()
install(TARGETS reciv RUNTIME DESTINATION .)
install(FILES third_party/nuked-opl3/LICENSE DESTINATION licenses RENAME Nuked-OPL3.txt)
install(FILES third_party/hqx/COPYING DESTINATION licenses RENAME HQx.txt)
# Retain notices and the exact vendored source beside portable distributions.
install(DIRECTORY third_party/hqx DESTINATION sources)
install(FILES src/platform/filters.c src/platform/filters.h DESTINATION sources/reciv)
include(CTest)
if(BUILD_TESTING)
    add_executable(reciv-graphics-layers-test tests/graphics-layers.c src/platform/mgraphic.c)
    target_include_directories(reciv-graphics-layers-test PRIVATE src/platform)
    target_compile_options(reciv-graphics-layers-test PRIVATE ${RECIV_WARNINGS})
    add_test(NAME graphics-layers COMMAND reciv-graphics-layers-test)

    add_executable(reciv-files-test tests/files.c src/platform/files.c)
    target_include_directories(reciv-files-test PRIVATE src/platform)
    target_compile_options(reciv-files-test PRIVATE ${RECIV_WARNINGS})
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/test-files)
    add_test(NAME dos-files COMMAND reciv-files-test ${CMAKE_CURRENT_BINARY_DIR}/test-files)

    # These four checks use GNU ld --wrap, which LINK.EXE cannot provide.
    if(NOT MSVC)
        add_executable(reciv-disaster-test tests/disaster.c)
        target_include_directories(reciv-disaster-test PRIVATE src/platform)
        target_compile_options(reciv-disaster-test PRIVATE -Wall -Wextra)
        target_link_libraries(reciv-disaster-test PRIVATE reciv-core)
        target_link_options(reciv-disaster-test PRIVATE
            -Wl,--wrap=rand_scaled,--wrap=terrain_at,--wrap=has_tech
            -Wl,--wrap=append_place_name,--wrap=named_msg)
        add_test(NAME city-disaster COMMAND reciv-disaster-test)

        add_executable(reciv-keyboard-test tests/keyboard.c)
        target_include_directories(reciv-keyboard-test PRIVATE src/platform)
        target_compile_options(reciv-keyboard-test PRIVATE -Wall -Wextra)
        target_link_libraries(reciv-keyboard-test PRIVATE reciv-core)
        target_link_options(reciv-keyboard-test PRIVATE
            -Wl,--wrap=reciv_spin,--wrap=reciv_poll,--wrap=reciv_wait
            -Wl,--wrap=reciv_require_slot,--wrap=reciv_text_teletype)
        add_test(NAME keyboard COMMAND reciv-keyboard-test)

        add_executable(reciv-battle-test tests/battle.c)
        target_compile_options(reciv-battle-test PRIVATE -Wall -Wextra)
        target_link_libraries(reciv-battle-test PRIVATE reciv-core)
        target_link_options(reciv-battle-test PRIVATE -Wl,--wrap=terrain_at,--wrap=map_flags,--wrap=city_at)
        add_test(NAME battle-strength COMMAND reciv-battle-test)

        add_executable(reciv-sound-timer-test tests/sound-timer.c)
        target_include_directories(reciv-sound-timer-test PRIVATE src/platform)
        target_compile_options(reciv-sound-timer-test PRIVATE -Wall -Wextra)
        target_link_libraries(reciv-sound-timer-test PRIVATE reciv-core)
        target_link_options(reciv-sound-timer-test PRIVATE
            -Wl,--wrap=SDL_GetTicksNS,--wrap=reciv_pump_events,--wrap=reciv_video_present
            -Wl,--wrap=reciv_shake_step,--wrap=reciv_sound_tick,--wrap=reciv_sound_fast_tick)
        add_test(NAME sound-timer COMMAND reciv-sound-timer-test)
    else()
        message(STATUS "MSVC: building the four portable checks; GNU --wrap checks are unavailable")
    endif()

    add_executable(reciv-asound-sequence-test tests/asound-sequence.c src/platform/asound-sequence.c)
    target_include_directories(reciv-asound-sequence-test PRIVATE src/platform)
    target_compile_options(reciv-asound-sequence-test PRIVATE ${RECIV_WARNINGS})
    add_test(NAME asound-sequence COMMAND reciv-asound-sequence-test)

    add_executable(reciv-asound-voice-test tests/asound-voice.c
        src/platform/asound-sequence.c src/platform/asound-voice.c)
    target_include_directories(reciv-asound-voice-test PRIVATE src/platform)
    target_compile_options(reciv-asound-voice-test PRIVATE ${RECIV_WARNINGS})
    add_test(NAME asound-voice COMMAND reciv-asound-voice-test)
endif()
