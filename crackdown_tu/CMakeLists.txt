# Title Update #6 build: the update's code (Default.xex + Default.xexp) with the hooks that are
# ported so far. Hook sources are shared with crackdown/ and compiled with CRACKDOWN_TU=1, which
# makes crackdown/src/game_addresses.h pick the update's addresses.
cmake_minimum_required(VERSION 3.25)
project(crackdown_tu LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(generated/rexglue.cmake)
set(CD_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../crackdown/src")
add_executable(crackdown_tu WIN32
    src/main.cpp
    ${CD_SRC}/hooks_events.cpp
    ${CD_SRC}/hooks_storage.cpp
    ${CD_SRC}/hooks_sleep.cpp
    ${CD_SRC}/hooks_framerate.cpp
    ${CD_SRC}/hooks_input_replay.cpp
    ${CD_SRC}/host_tuning.cpp
    ${CD_SRC}/hooks_diag_audio.cpp
    ${CD_SRC}/hooks_diag_physics.cpp
)
target_compile_definitions(crackdown_tu PRIVATE CRACKDOWN_TU=1)
# generated/ resolves to this project's (update) code, never crackdown/'s.
target_include_directories(crackdown_tu BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CD_SRC}")
rexglue_setup_target(crackdown_tu GPU_PLUGINS xenos)
