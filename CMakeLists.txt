cmake_minimum_required(VERSION 3.14)

if (NOT DEFINED ENV{VITASDK} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  message(FATAL_ERROR "Please define VITASDK env var to point to your SDK path or set CMAKE_TOOLCHAIN_FILE.")
endif ()

set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "Toolchain file")

project(so_loader C CXX)

include("${VITASDK}/share/vita.cmake" REQUIRED)

# Vita application configuration
set(VITA_APP_NAME "mcsm_loader")
set(VITA_TITLEID "MCSM00000")
set(VITA_VPKNAME "mcsm_loader")
set(VITA_VERSION "01.00")
set(VITA_MKSFOEX_FLAGS "-d ATTRIBUTE2=12")

# Optional: used for "send" and "dump" targets
set(PSVITAIP "192.168.0.198" CACHE STRING "PSVita IP (for FTP access)")
set(PARSECOREPATH "$ENV{PARSECOREPATH}" CACHE STRING "vita-parse-core main.py")

# -----------------------------------------------------------------------------
# Game-specific definitions (DO NOT CHANGE NAMES — used in baseplate)
# -----------------------------------------------------------------------------
# Telltale Android titles often hardcode their package path. Mirror it under ux0:data.
# Minecraft Story Mode package name:
#   com.telltalegames.minecraft100
set(DATA_PATH "ux0:data/com.telltalegames.minecraft100/" CACHE STRING "Path to data (with trailing /)")

# Main library for this port (your loader entry .so)
# NOTE: you said the main one is called libmain.so
set(SO_PATH "${DATA_PATH}libmain.so" CACHE STRING "Path to .so")

add_definitions(-DDATA_PATH="${DATA_PATH}" -DSO_PATH="${SO_PATH}")

# -----------------------------------------------------------------------------
# Additional Android .so libraries (additive only)
# -----------------------------------------------------------------------------
set(GAMEENGINE_SO "${DATA_PATH}libGameEngine.so" CACHE STRING "Path to libGameEngine.so")
set(FMOD_SO "${DATA_PATH}libfmod.so" CACHE STRING "Path to libfmod.so")
set(FMODSTUDIO_SO "${DATA_PATH}libfmodstudio.so" CACHE STRING "Path to libfmodstudio.so")
add_definitions(
  -DGAMEENGINE_SO="${GAMEENGINE_SO}"
  -DFMOD_SO="${FMOD_SO}"
  -DFMODSTUDIO_SO="${FMODSTUDIO_SO}"
)

# Optional runtime-loading switches (only affect code if you use these defines)
option(LOAD_GAMEENGINE_SO "Load libGameEngine.so at runtime" ON)
option(LOAD_FMOD "Load libfmod.so at runtime" ON)
option(LOAD_FMODSTUDIO "Load libfmodstudio.so at runtime (if needed)" ON)
if (LOAD_GAMEENGINE_SO)
  add_definitions(-DLOAD_GAMEENGINE_SO)
endif()
if (LOAD_FMOD)
  add_definitions(-DLOAD_FMOD)
endif()
if (LOAD_FMODSTUDIO)
  add_definitions(-DLOAD_FMODSTUDIO)
endif()
option(DEBUG_TRACE "Enable verbose so_loader tracing" ON)
if (DEBUG_TRACE)
  add_definitions(-DDEBUG_TRACE=1)
endif()

option(LOAD_MAIN_SO_FOR_TRACE "Load libmain.so only for trace/binding insights" OFF)
if (LOAD_MAIN_SO_FOR_TRACE)
  add_definitions(-DLOAD_MAIN_SO_FOR_TRACE)
endif()

option(TRACE_PLT_BINDINGS "Trace libmain -> libGameEngine PLT bindings" OFF)
if (TRACE_PLT_BINDINGS)
  add_definitions(-DTRACE_PLT_BINDINGS=1)
endif()

# -----------------------------------------------------------------------------
# Telltale OBB / TTARCH notes
# -----------------------------------------------------------------------------
# For Minecraft Story Mode Android, the OBB is NOT a zip.
# After extracting the OBB container, you will have .ttarch2 packs and _rescdesc_*.lua.
# This template assumes you COPY those extracted files straight into DATA_PATH.
# Your code should treat DATA_PATH as the asset root and/or redirect Android paths to it.
add_definitions(-DTELLTALE_DATA_ROOT="${DATA_PATH}")
add_definitions(-DTELLTALE_TTARCH2_LAYOUT)

# Keep OBB_PATH macro available (OFF by default). If later you implement direct-container reading,
# enable USE_OBB and point OBB_PATH at the container file.
set(OBB_PATH "${DATA_PATH}main.com.telltalegames.minecraft100.obb" CACHE STRING "Path to main OBB file")
set(PATCH_OBB_PATH "${DATA_PATH}patch.com.telltalegames.minecraft100.obb" CACHE STRING "Path to patch OBB file")
option(USE_OBB "Enable loading assets from OBB" OFF)
add_definitions(-DOBB_PATH="${OBB_PATH}")
add_definitions(-DPATCH_OBB_PATH="${PATCH_OBB_PATH}")
if (USE_OBB)
  add_definitions(-DUSE_OBB)
endif()

# SDL2 support toggle
option(USE_SDL2 "Enable SDL2 support for SDL-based Android games" ON)
if (USE_SDL2)
  add_definitions(-DUSE_SDL2)
endif()

option(USE_SCELIBC_IO "Use SceLibcBridge for IO and some other std functions" ON)
if (USE_SCELIBC_IO)
  add_definitions(-DUSE_SCELIBC_IO)
endif()

set(SHADER_FORMAT "GLSL" CACHE STRING "Preferred shader format (one of 'GLSL', 'CG', 'GXP')")
if (${SHADER_FORMAT} STREQUAL "GLSL")
  add_definitions(-DUSE_GLSL_SHADERS)
elseif (${SHADER_FORMAT} STREQUAL "CG")
  add_definitions(-DUSE_CG_SHADERS)
elseif (${SHADER_FORMAT} STREQUAL "GXP")
  add_definitions(-DUSE_GXP_SHADERS)
else()
  message(FATAL_ERROR "SHADER_FORMAT option can only be one of: 'GLSL', 'CG', 'GXP'")
endif()

set(valid_dump_formats GLSL CG)
option(DUMP_COMPILED_SHADERS "When using GLSL or CG shader format, cache compiled shaders on disk" ON)
if (DUMP_COMPILED_SHADERS)
  if (SHADER_FORMAT IN_LIST valid_dump_formats)
    add_definitions(-DDUMP_COMPILED_SHADERS)
  else()
    message(WARNING "DUMP_COMPILED_SHADERS has no effect for the chosen shader format ${SHADER_FORMAT}")
  endif()
endif()

# Unique debug definition for loader's logging to not interfere with anything
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_definitions(-DDEBUG_SOLOADER)
  #add_definitions(-DFALSOJNI_DEBUGLEVEL=0)
  #add_definitions(-DDEBUG_OPENGL)
endif()

# makes sincos, sincosf, etc. visible
add_definitions(-D_GNU_SOURCE -D__POSIX_VISIBLE=999999)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -mfloat-abi=softfp -std=gnu11 -Wno-deprecated")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,-q -mfloat-abi=softfp -std=gnu++20 -Wno-write-strings -Wno-psabi")

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O1 -g3")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O1 -g3")
else ()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -g0 -ffast-math")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -g0 -ffast-math")
endif()

add_executable(${CMAKE_PROJECT_NAME}
               source/main.c
               source/dynlib.c
               source/java.c
               source/android_shims.c
               source/patch.c
               source/reimpl/bits/_ctype.c
               source/reimpl/controls.c
               source/reimpl/controls.h
               source/reimpl/egl.c
               source/reimpl/errno.c
               source/reimpl/io.c
               source/reimpl/log.c
               source/reimpl/mem.c
               source/reimpl/pthr.c
               source/reimpl/sys.c
               source/reimpl/time64.c
               source/reimpl/asset_manager.cpp
               source/utils/dialog.c
               source/utils/glutil.c
               source/utils/init.c
               source/utils/logger.c
               source/utils/settings.c
               source/utils/utils.c
               lib/falso_jni/FalsoJNI.c
               lib/falso_jni/FalsoJNI_ImplBridge.c
               lib/falso_jni/FalsoJNI_Logger.c
               lib/falso_jni/converter.c
               lib/sha1/sha1.c
               lib/fios/fios.c
               lib/so_util/so_util.c)

add_subdirectory(lib/libc_bridge)
add_dependencies(${CMAKE_PROJECT_NAME} SceLibcBridge)

target_include_directories(${CMAKE_PROJECT_NAME} SYSTEM
                           PUBLIC ${CMAKE_SOURCE_DIR}/lib
                           )

target_include_directories(${CMAKE_PROJECT_NAME}
                           PUBLIC ${CMAKE_SOURCE_DIR}/source
                           )

# -----------------------------------------------------------------------------
# Link configuration
# -----------------------------------------------------------------------------
# NOTE: libsndfile tends to drag codec deps (mpg123/lame/opus). Make it optional.
option(USE_SNDFILE "Link libsndfile (may require mpg123/mp3lame/opus/etc)" ON)

set(AUDIO_LIBS "")
set(SDL2_LIBS "")
if (USE_SDL2)
  list(APPEND SDL2_LIBS SDL2)
endif()
if (USE_SNDFILE)
  # Order matters with static libs.
  list(APPEND AUDIO_LIBS
    sndfile
    vorbis
    opus
    opusfile
    mp3lame
    mpg123
    vorbisenc
    ogg
    FLAC
  )
endif()

target_link_libraries(${CMAKE_PROJECT_NAME}
                      -Wl,--whole-archive pthread -Wl,--no-whole-archive
                      OpenSLES
                      ${SDL2_LIBS}
                      ${AUDIO_LIBS}
                      stdc++
                      vitaGL
                      vitashark
                      SceShaccCgExt
                      taihen_stub
                      SceShaccCg_stub
                      z
                      m
                      ScePgf_stub
                      mathneon
                      kubridge_stub
                      ${CMAKE_BINARY_DIR}/lib/libc_bridge/stubs/libSceLibcBridge_stub.a
                      SceHid_stub
                      SceTouch_stub
                      SceMotion_stub
                      SceAppMgr_stub
                      SceAppUtil_stub
                      SceAudio_stub
                      SceAudioIn_stub
                      SceCommonDialog_stub
                      SceDisplay_stub
                      SceKernelDmacMgr_stub
                      SceFios2_stub
                      SceCtrl_stub
                      SceGxm_stub
                      ScePower_stub
                      SceTouch_stub
                      SceVshBridge_stub
                      )

vita_create_self(eboot.bin ${CMAKE_PROJECT_NAME} UNSAFE NOASLR)

vita_create_vpk(${VITA_VPKNAME}.vpk ${VITA_TITLEID} eboot.bin
                VERSION ${VITA_VERSION}
                NAME ${VITA_APP_NAME}
                FILE ${CMAKE_SOURCE_DIR}/extras/cpuinfo cpuinfo
                ${CMAKE_SOURCE_DIR}/extras/meminfo meminfo
                ${CMAKE_SOURCE_DIR}/extras/urandom urandom
                ${CMAKE_SOURCE_DIR}/extras/livearea/icon0.png sce_sys/icon0.png
                ${CMAKE_SOURCE_DIR}/extras/livearea/pic0.png sce_sys/pic0.png
                ${CMAKE_SOURCE_DIR}/extras/livearea/startup.png sce_sys/livearea/contents/startup.png
                ${CMAKE_SOURCE_DIR}/extras/livearea/bg0.png sce_sys/livearea/contents/bg0.png
                ${CMAKE_SOURCE_DIR}/extras/livearea/template.xml sce_sys/livearea/contents/template.xml
                )

add_custom_target(send
                  COMMAND echo destroy | nc ${PSVITAIP} 1338
                  COMMAND echo destroy | nc ${PSVITAIP} 1338
                  COMMAND curl -T eboot.bin ftp://${PSVITAIP}:1337/ux0:/app/${VITA_TITLEID}/
                  COMMAND echo launch ${VITA_TITLEID} | nc ${PSVITAIP} 1338
                  DEPENDS ${VITA_VPKNAME}.vpk-vpk
                  )

add_custom_target(send_kvdb
                  COMMAND echo destroy | nc ${PSVITAIP} 1338
                  COMMAND curl -T eboot.bin ftp://${PSVITAIP}:1337/ux0:/app/${VITA_TITLEID}/
                  COMMAND echo launch ux0:/app/${VITA_TITLEID}/eboot.bin | nc ${PSVITAIP} 1338
                  DEPENDS ${VITA_VPKNAME}.vpk-vpk
                  )

add_custom_target(dump
                  COMMAND ${CMAKE_SOURCE_DIR}/extras/scripts/get_dump.sh ${PSVITAIP}
                  COMMAND python3 ${PARSECOREPATH} coredump ${CMAKE_PROJECT_NAME}
                  )

add_custom_target(reboot
                  COMMAND echo reboot | nc ${PSVITAIP} 1338
                  )
