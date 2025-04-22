cmake_minimum_required (VERSION 3.18)
project(Coutil C)

option(LINK_TO_LUAMEM_LIB "Link to LuaMemory library?" ON)
set(MODULE_DESTINATION lib CACHE PATH "Destination of Lua binary modules.")

add_library(coutil SHARED "src/lmodaux.c" "src/loperaux.c" "src/lchaux.c"
                          "src/lthpool.c" "src/lttyaux.c" "src/lcommunf.c"
                          "src/lfilef.c" "src/linfof.c" "src/lprocesf.c"
                          "src/lscheduf.c" "src/lstdiof.c" "src/ltimef.c"
                          "src/lchannem.c" "src/lcoroutm.c" "src/lsystemm.c"
                          "src/lthreadm.c")

include(GenerateExportHeader)
generate_export_header(coutil)

if (NOT LUA_INCLUDE_DIR OR NOT LUA_LIBRARIES)
	find_package(Lua REQUIRED)
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
target_link_libraries(coutil PRIVATE ${LUA_LIBRARIES} ${LIBUV_LIBRARIES})
set_target_properties(coutil PROPERTIES PREFIX "")

install(TARGETS coutil
        RUNTIME DESTINATION ${MODULE_DESTINATION}
        LIBRARY DESTINATION ${MODULE_DESTINATION})
