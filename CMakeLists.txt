cmake_minimum_required (VERSION 3.18)
project(Coutil)

option(LINK_TO_LUAMEM_LIB "Link to LuaMemory library?" ON)

add_library(coutil MODULE "src/lmodaux.c" "src/loperaux.c" "src/lchaux.c"
                          "src/lthpool.c" "src/lttyaux.c" "src/lcommunf.c"
                          "src/lfilef.c" "src/linfof.c" "src/lprocesf.c"
                          "src/lscheduf.c" "src/lstdiof.c" "src/ltimef.c"
                          "src/lchannem.c" "src/lcoroutm.c" "src/lsystemm.c"
                          "src/lthreadm.c")

include(GenerateExportHeader)
generate_export_header(coutil)

if(NOT TARGET Lua::Lua)
  if(NOT LUA_INCLUDE_DIR)
    find_package(Lua REQUIRED)
    set(
      LUA_MODULE_DIR "lib/lua/${LUA_VERSION_MAJOR}.${LUA_VERSION_MINOR}"
      CACHE PATH "Destination of Lua modules."
    )
  elseif(LUA_LIBRARY_DIR AND LUA_LIBRARY_FILE)
    cmake_path(
      APPEND LUA_LIBRARY_DIR "${LUA_LIBRARY_FILE}"
      OUTPUT_VARIABLE LUA_LIBRARIES
    )
  endif()
  add_library(Lua::Lua INTERFACE IMPORTED)
  set_target_properties(
    Lua::Lua
    PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${LUA_INCLUDE_DIR}"
      INTERFACE_LINK_LIBRARIES "${LUA_LIBRARIES}"
  )
endif()
if (NOT LIBUV_INCLUDE_DIR)
  find_path(LIBUV_INCLUDE_DIR NAMES uv.h REQUIRED)
endif()
if (NOT LIBUV_LIBRARIES)
  find_library(LIBUV_LIBRARIES NAMES uv libuv REQUIRED)
endif()
if (NOT LUAMEM_INCLUDE_DIR)
  find_path(LUAMEM_INCLUDE_DIR NAMES luamem.h REQUIRED)
endif()
if (LINK_TO_LUAMEM_LIB)
  if (NOT LUAMEM_LIBRARIES)
    find_library(LUAMEM_LIBRARIES NAMES luamem libluamem REQUIRED)
  endif()
  target_link_libraries (coutil PRIVATE ${LUAMEM_LIBRARIES})
endif()
if (WIN32)
  target_link_libraries (coutil PRIVATE ws2_32)
endif()

target_include_directories(coutil PRIVATE ${LUA_INCLUDE_DIR} ${LUAMEM_INCLUDE_DIR} ${LIBUV_INCLUDE_DIR})
target_link_libraries(coutil PRIVATE Lua::Lua ${LIBUV_LIBRARIES})
set_target_properties(coutil PROPERTIES PREFIX "")

if(NOT LUA_MODULE_DIR)
  install(TARGETS coutil)
else()
  install(TARGETS coutil LIBRARY DESTINATION ${LUA_MODULE_DIR})
endif()
