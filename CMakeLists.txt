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
elseif(EMSCRIPTEN)
    # civ-wasm: Emscripten's Clang, with SDL3 from Emscripten's ports.
    set(RECIV_WARNINGS -Wall -Wextra)
elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU" AND NOT CMAKE_C_COMPILER_VERSION VERSION_LESS 14)
    set(RECIV_WARNINGS -Wall -Wextra)
else()
    message(FATAL_ERROR "Use GCC 14+ or opt-in MSVC (-DRECIV_MSVC=ON)")
endif()
if(EMSCRIPTEN)
    # civ-wasm: emcc links through a copy of its LLVM directory whose wasm-ld
    # adapts the period code's mismatched calls (tools/wasm).
    execute_process(COMMAND em-config LLVM_ROOT OUTPUT_VARIABLE CIVWASM_LLVM_ROOT
        OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
    set(CIVWASM_LLVM "${CMAKE_BINARY_DIR}/civwasm-llvm")
    file(MAKE_DIRECTORY "${CIVWASM_LLVM}")
    file(GLOB CIVWASM_LLVM_TOOLS "${CIVWASM_LLVM_ROOT}/*")
    foreach(tool IN LISTS CIVWASM_LLVM_TOOLS)
        get_filename_component(name "${tool}" NAME)
        if(NOT name STREQUAL "wasm-ld")
            file(CREATE_LINK "${tool}" "${CIVWASM_LLVM}/${name}" SYMBOLIC)
        endif()
    endforeach()
    file(CREATE_LINK "${CMAKE_CURRENT_SOURCE_DIR}/tools/wasm/wasm-ld" "${CIVWASM_LLVM}/wasm-ld" SYMBOLIC)
    file(WRITE "${CIVWASM_LLVM}/.real-llvm-root" "${CIVWASM_LLVM_ROOT}")
    file(WRITE "${CIVWASM_LLVM}/.fixer"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/wasm/fix-signature-mismatch.py")
    set(CMAKE_C_LINKER_LAUNCHER "${CMAKE_COMMAND};-E;env;EM_LLVM_ROOT=${CIVWASM_LLVM};EMCC_SKIP_SANITY_CHECK=1")
    add_library(SDL3::SDL3 INTERFACE IMPORTED)
    target_compile_options(SDL3::SDL3 INTERFACE --use-port=sdl3)
    target_link_options(SDL3::SDL3 INTERFACE --use-port=sdl3)
else()
    find_package(SDL3 REQUIRED CONFIG)
endif()

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
elseif(EMSCRIPTEN)
    # Clang has no -fpermissive; demote the errors GCC's permissive mode allowed.
    target_compile_options(period PRIVATE
        -x c -nostdinc -fno-builtin -fwrapv -fno-strict-aliasing -fsigned-char
        -Wno-error=int-conversion -Wno-error=incompatible-pointer-types
        -Wno-error=implicit-function-declaration -Wno-error=implicit-int
        -Wno-error=return-type -Wno-error=incompatible-function-pointer-types -w
        -include "${CMAKE_CURRENT_SOURCE_DIR}/src/period/prelude.h")
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
# civ-wasm: plain objects, not an archive, so that wasm-ld sees every
# definition and adapts every mismatched call (tools/wasm/wasm-ld).
if(EMSCRIPTEN)
    set(RECIV_CORE_KIND OBJECT)
else()
    set(RECIV_CORE_KIND STATIC)
endif()
add_library(reciv-core ${RECIV_CORE_KIND}
    $<TARGET_OBJECTS:period>
    src/platform/asound-sequence.c
    src/platform/asound-voice.c
    src/platform/asound-command.c
    src/platform/civwasm.c
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
if(EMSCRIPTEN)
    # An object library does not pass on the period objects it lists.
    target_sources(reciv PRIVATE $<TARGET_OBJECTS:period>)
    # civ-wasm: the game blocks in reciv_poll's SDL_DelayNS; Asyncify turns
    # those waits into returns to the browser's event loop.
    target_link_options(reciv PRIVATE
        -sASYNCIFY -sASYNCIFY_STACK_SIZE=262144 -sSTACK_SIZE=1048576
        -sALLOW_MEMORY_GROWTH -sINITIAL_MEMORY=134217728
        -sFORCE_FILESYSTEM -lidbfs.js -sENVIRONMENT=web
        -sEXPORTED_RUNTIME_METHODS=FS,IDBFS,callMain -sINVOKE_RUN=0)
    # Development only: bake a local copy of the original game into reciv.data.
    set(CIVWASM_PRELOAD "" CACHE PATH "Original Civilization directory to preload as /civ")
    if(CIVWASM_PRELOAD)
        target_link_options(reciv PRIVATE "--preload-file=${CIVWASM_PRELOAD}@/civ")
    endif()
endif()
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
