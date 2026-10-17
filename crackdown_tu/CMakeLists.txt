# Title update probe: the update's generated code with only the two loading-screen hooks.
cmake_minimum_required(VERSION 3.25)
project(crackdown_tu LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(generated/rexglue.cmake)
# Input replay is the disc build's hook file with its two guest addresses swapped for the update's
# (XInputGetState wrapper 0x82743798 -> 0x827CCB18, keystroke wrapper 0x827437C0 -> 0x827CCB40).
set(INPUT_REPLAY_TU "${CMAKE_CURRENT_BINARY_DIR}/hooks_input_replay_tu.cpp")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../crackdown/src/hooks_input_replay.cpp" _replay)
string(REPLACE "sub_82743798" "sub_827CCB18" _replay "${_replay}")
string(REPLACE "sub_827437C0" "sub_827CCB40" _replay "${_replay}")
file(WRITE "${INPUT_REPLAY_TU}" "${_replay}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../crackdown/src/hooks_input_replay.cpp")
add_executable(crackdown_tu WIN32 src/main.cpp src/hooks_tu.cpp "${INPUT_REPLAY_TU}")
target_include_directories(crackdown_tu PRIVATE src "${CMAKE_CURRENT_SOURCE_DIR}/../crackdown/src")
rexglue_setup_target(crackdown_tu GPU_PLUGINS xenos)
