# Ultima VI, native port: the game's C from ../SRC (shared with the DOS build) and the
# port's own C in this folder, on SDL2.
#
#   cmake -S port -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build
#
# Windows: MSYS2 (UCRT64 or CLANG64: pacman -S mingw-w64-ucrt-x86_64-{gcc,cmake,SDL2}),
# or Visual Studio with clang-cl and SDL2's development package (-DSDL2_DIR=...), or a
# cross build from Linux: cmake -S port -B build-win -DCMAKE_TOOLCHAIN_FILE=port/cmake/mingw64.cmake
cmake_minimum_required(VERSION 3.16)
project(u6port C)

set(CMAKE_C_STANDARD 99)
set(GAME_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../SRC)

find_package(SDL2 CONFIG QUIET)
if(NOT SDL2_FOUND)
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
	set(SDL2_TARGET PkgConfig::SDL2)
else()
	if(TARGET SDL2::SDL2)
		set(SDL2_TARGET SDL2::SDL2)
	else()
		set(SDL2_TARGET ${SDL2_LIBRARIES})
		include_directories(${SDL2_INCLUDE_DIRS})
	endif()
endif()

# ---- the game's C, compiled as Turbo C saw it ----
file(GLOB GAME_C ${GAME_SRC}/*.c)
# gamestat.c's and monsdata.c's variables keep DOS's layout: the port defines each file's
# as a packed block of its own (port/tools/gen_block.py)
list(REMOVE_ITEM GAME_C ${GAME_SRC}/gamestat.c ${GAME_SRC}/monsdata.c)
add_library(u6game OBJECT ${GAME_C} game/digest.c game/gamestat_port.c game/monsdata_port.c)
target_include_directories(u6game PRIVATE ${GAME_SRC} ${CMAKE_CURRENT_SOURCE_DIR}/include)
foreach(f ${GAME_C})
	get_filename_component(n ${f} NAME_WE)
	set_source_files_properties(${f} PROPERTIES COMPILE_DEFINITIONS PORT_TU_${n})
endforeach()
if(MSVC AND NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
	message(FATAL_ERROR "build with clang-cl, MinGW or MSYS2: MSVC's cl does not take the game's C89")
endif()
target_compile_options(u6game PRIVATE
	-std=gnu89 -fsigned-char -fwrapv -fno-strict-aliasing
	-include ${CMAKE_CURRENT_SOURCE_DIR}/include/compat.h
	-Wno-everything)
if(CMAKE_C_COMPILER_ID MATCHES "Clang")
	target_compile_options(u6game PRIVATE
		-Wno-error=int-conversion -Wno-error=return-type -Wno-error=implicit-function-declaration
		-Wno-error=implicit-int -Wno-error=incompatible-function-pointer-types)
else()
	target_compile_options(u6game PRIVATE -w -fpermissive)
endif()

# ---- U.EXE's C (menu, introduction, character creation), compiled the same way; its
# names are renamed U_* (port/u/uport.h) so that they do not collide with GAME.EXE's ----
set(U_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../SRC_U)
file(GLOB U_C ${U_SRC}/*.c)
file(GLOB U_OSILIB_C ${CMAKE_CURRENT_SOURCE_DIR}/u/osilib/*.c)
add_library(u6u OBJECT ${U_C} ${U_OSILIB_C})
target_include_directories(u6u PRIVATE ${U_SRC} ${CMAKE_CURRENT_SOURCE_DIR}/u ${CMAKE_CURRENT_SOURCE_DIR}/include
	${CMAKE_CURRENT_SOURCE_DIR}/sys ${CMAKE_CURRENT_SOURCE_DIR}/platform)
foreach(f ${U_C})
	get_filename_component(n ${f} NAME_WE)
	set_source_files_properties(${f} PROPERTIES COMPILE_DEFINITIONS PORT_UTU_${n})
endforeach()
# the modules whose noise reads their own code (_CS)
set_property(SOURCE ${U_SRC}/chargen.c APPEND PROPERTY COMPILE_DEFINITIONS PORT_U_FRAME=0x0406)
set_property(SOURCE ${U_SRC}/intro.c APPEND PROPERTY COMPILE_DEFINITIONS PORT_U_FRAME=0x06E9)
target_compile_options(u6u PRIVATE
	-std=gnu89 -fsigned-char -fwrapv -fno-strict-aliasing
	"SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/include/compat.h"
	"SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/u/uport.h"
	-Wno-everything)
if(CMAKE_C_COMPILER_ID MATCHES "Clang")
	target_compile_options(u6u PRIVATE
		-Wno-error=int-conversion -Wno-error=return-type -Wno-error=implicit-function-declaration
		-Wno-error=implicit-int -Wno-error=incompatible-function-pointer-types)
else()
	target_compile_options(u6u PRIVATE -w -fpermissive)
endif()

# ---- the port ----
file(GLOB PORT_C
	sys/*.c gfx/*.c replay/*.c ui/*.c u/*.c platform/hostfs.c platform/sdl2/plat_sdl2.c)
add_executable(u6 ${PORT_C} $<TARGET_OBJECTS:u6game> $<TARGET_OBJECTS:u6u>)
target_include_directories(u6 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/platform)
target_compile_options(u6 PRIVATE -fsigned-char -fwrapv -fno-strict-aliasing)
target_link_libraries(u6 PRIVATE ${SDL2_TARGET} m)
if(WIN32)
	target_link_options(u6 PRIVATE -mwindows)
	target_link_libraries(u6 PRIVATE ole32 shell32)
endif()
if(NOT WIN32)
	target_compile_definitions(u6 PRIVATE _POSIX_C_SOURCE=200809L _DEFAULT_SOURCE)
endif()
