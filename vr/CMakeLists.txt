# SPDX-License-Identifier: GPL-3.0-or-later
# The Quest VR mod. Built with the NDK for the headset (OPENXR_DIR: the Khronos loader's unpacked
# AAR, tools/build_quest.py fetches it), or for a desktop with the simulated headset, which tests
# it against the real game.
cmake_minimum_required(VERSION 3.20)
project(quest_vr LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT OPENRAC_SDK)
  get_filename_component(OPENRAC_SDK "${CMAKE_CURRENT_LIST_DIR}/../third_party/OpenRAC/sdk" ABSOLUTE)
endif()
set(OpenRACMod_DIR "${OPENRAC_SDK}/cmake")
find_package(OpenRACMod 1.10 REQUIRED)

if(ANDROID)
  set(backend src/xr_openxr.cpp)
else()
  # The simulated headset for the tests, and on Windows OpenXR for players (OPENXR_PC_DIR: the Khronos
  # loader package's headers, tools/build_pc.py fetches it; the loader is opened at run time).
  set(backend src/xr_sim.cpp)
  if(WIN32)
    if(NOT OPENXR_PC_DIR)
      message(FATAL_ERROR "OPENXR_PC_DIR: the unpacked openxr_loader_windows package")
    endif()
    list(APPEND backend src/xr_openxr.cpp)
  endif()
endif()

openrac_add_mod(quest_vr MANIFEST openrac-mod.json SOURCES src/quest_vr.cpp ${backend})
target_include_directories(quest_vr PRIVATE src)
if(NOT MSVC)
  target_compile_options(quest_vr PRIVATE -Wall -Wextra)
endif()

if(ANDROID)
  if(NOT OPENXR_DIR)
    message(FATAL_ERROR "OPENXR_DIR: the unpacked openxr_loader_for_android AAR")
  endif()
  target_include_directories(quest_vr PRIVATE "${OPENXR_DIR}/prefab/modules/headers/include")
  target_link_libraries(quest_vr PRIVATE
    "${OPENXR_DIR}/jni/arm64-v8a/libopenxr_loader.so" EGL GLESv3 android log dl)
  # The loader is found beside this library in the APK.
  set_target_properties(quest_vr PROPERTIES LINK_FLAGS "-Wl,-z,max-page-size=16384")
endif()

if(WIN32)
  target_include_directories(quest_vr PRIVATE "${OPENXR_PC_DIR}/include")
  # wglGetCurrentDC and wglGetCurrentContext: the game's context for the OpenXR session.
  target_link_libraries(quest_vr PRIVATE opengl32)
endif()
