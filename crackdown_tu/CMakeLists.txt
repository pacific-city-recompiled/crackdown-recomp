# Title Update #6 build: the update's code (Default.xex + Default.xexp) with the hooks that are
# ported so far. Hook sources are shared with crackdown/ and compiled with CRACKDOWN_TU=1, which
# makes crackdown/src/game_addresses.h pick the update's addresses.
cmake_minimum_required(VERSION 3.25)
project(crackdown_tu LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(generated/rexglue.cmake)
set(CD_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../crackdown/src")
set(SDK_THIRDPARTY "${CMAKE_CURRENT_SOURCE_DIR}/../sdk/thirdparty" CACHE PATH "SDK dependencies")
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
    ${CD_SRC}/hooks_intro.cpp
    ${CD_SRC}/xui_patch.cpp
    ${CD_SRC}/hooks_pc_options.cpp
    ${CD_SRC}/pc_settings.cpp
    ${CD_SRC}/hooks_xaudio.cpp
    ${CD_SRC}/hooks_capture.cpp
    ${CD_SRC}/hooks_coop_trace.cpp
    ${CD_SRC}/hooks_aspect.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../crackdown/common/toml_edit.cpp
    ${SDK_THIRDPARTY}/crypto/sha256.cpp
)
target_compile_definitions(crackdown_tu PRIVATE CRACKDOWN_TU=1)
# generated/ resolves to this project's (update) code, never crackdown/'s.
target_include_directories(crackdown_tu BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CD_SRC}")
target_include_directories(crackdown_tu PRIVATE "${SDK_THIRDPARTY}/crypto"
    "${SDK_THIRDPARTY}/tomlplusplus/include")
rexglue_setup_target(crackdown_tu GPU_PLUGINS xenos)

# Same audited XUI patches as the disc build (they check the disc's Scene.xzp/menuHUD_3.xzp hashes
# and fail open).
set(PC_SCENE_PATCH "${CMAKE_CURRENT_SOURCE_DIR}/../patches/xui/Scene.xpatch")
set(PC_MENU_PATCH "${CMAKE_CURRENT_SOURCE_DIR}/../patches/xui/menuHUD_3.xpatch")
add_custom_target(crackdown_tu_xui_patch ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:crackdown_tu>/xui"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PC_MENU_PATCH}" "$<TARGET_FILE_DIR:crackdown_tu>/xui/menuHUD_3.xpatch"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PC_SCENE_PATCH}" "$<TARGET_FILE_DIR:crackdown_tu>/xui/Scene.xpatch"
    DEPENDS "${PC_MENU_PATCH}" "${PC_SCENE_PATCH}"
    VERBATIM)
add_dependencies(crackdown_tu crackdown_tu_xui_patch)
