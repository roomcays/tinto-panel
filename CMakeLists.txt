cmake_minimum_required(VERSION 2.8)
set(PROJECT_NAME "tinto")
project(${PROJECT_NAME} C)

add_definitions(-DVERSION_STRING="0.1")

include(CheckFunctionExists)
include(CheckLibraryExists)
include(FindPkgConfig)

pkg_check_modules(X11 REQUIRED x11 xcomposite xdamage xinerama xrender xrandr>=1.3)
pkg_check_modules(PANGOCAIRO REQUIRED pangocairo)
pkg_check_modules(PANGO REQUIRED pango)
pkg_check_modules(CAIRO REQUIRED cairo)
pkg_check_modules(GLIB2 REQUIRED glib-2.0)
pkg_check_modules(GOBJECT2 REQUIRED gobject-2.0)
pkg_check_modules(IMLIB2 REQUIRED imlib2>=1.4.2)
pkg_check_modules(SN libstartup-notification-1.0>=0.12)
find_library(RT_LIBRARY rt)

if(NOT X11_FOUND OR NOT PANGOCAIRO_FOUND OR NOT PANGO_FOUND OR NOT CAIRO_FOUND OR NOT GLIB2_FOUND OR NOT GOBJECT2_FOUND OR NOT IMLIB2_FOUND)
  message(FATAL_ERROR "Package(s) needed to build tinto are missing.")
endif(NOT X11_FOUND OR NOT PANGOCAIRO_FOUND OR NOT PANGO_FOUND OR NOT CAIRO_FOUND OR NOT GLIB2_FOUND OR NOT GOBJECT2_FOUND OR NOT IMLIB2_FOUND)

string(REPLACE ";" " " FLAGS_REPLACED "${IMLIB2_LDFLAGS}")
set(CMAKE_REQUIRED_FLAGS "${FLAGS_REPLACED}")
check_library_exists("${IMLIB2_LIBRARIES}" "imlib_context_set_display" "${IMLIB2_LIBRARY_DIRS}" IMLIB_BUILD_WITH_X)
if(NOT IMLIB_BUILD_WITH_X)
  message(FATAL_ERROR "Imlib is not build with x support")
endif(NOT IMLIB_BUILD_WITH_X)

include_directories(${PROJECT_BINARY_DIR}
                     src
                     src/battery
                     src/clock
                     src/systray
                     src/taskbar
                     src/launcher
                     src/tooltip
                     src/util
                     ${X11_INCLUDE_DIRS}
                     ${PANGOCAIRO_INCLUDE_DIRS}
                     ${PANGO_INCLUDE_DIRS}
                     ${CAIRO_INCLUDE_DIRS}
                     ${GLIB2_INCLUDE_DIRS}
                     ${GOBJECT2_INCLUDE_DIRS}
                     ${IMLIB2_INCLUDE_DIRS}
		     ${SN_INCLUDE_DIRS})

set(SOURCES src/config.c
             src/panel.c
             src/server.c
             src/tinto.c
             src/clock/clock.c
             src/systray/systraybar.c
             src/launcher/launcher.c
             src/launcher/xsettings-client.c
             src/launcher/xsettings-common.c
             src/taskbar/task.c
             src/taskbar/taskbar.c
             src/taskbar/taskbarname.c
             src/tooltip/tooltip.c
             src/util/area.c
             src/util/color.c
             src/util/common.c
             src/util/timer.c
             src/util/window.c
             src/util/debug.c)

option(ENABLE_BATTERY "Enable battery status plugin." ON)

option(ENABLE_SN "Startup notification support." ON)
if(ENABLE_SN)
  if(SN_FOUND)
    add_definitions(-DHAVE_SN -DSN_API_NOT_YET_FROZEN)
  endif(SN_FOUND)
endif(ENABLE_SN)

if(ENABLE_BATTERY)
  set(SOURCES ${SOURCES} src/battery/battery.c)
  add_definitions(-DENABLE_BATTERY)
endif(ENABLE_BATTERY)

set(MANDIR share/man CACHE PATH "Directory for man pages.")
set(DATADIR share CACHE PATH "Directory for shared data.")
set(SYSCONFDIR /etc CACHE PATH "Directory for configuration files.")
set(DOCDIR share/doc/tinto CACHE PATH "Directory for documentation files.")

add_definitions(-D_BSD_SOURCE -D_POSIX_SOURCE -D_POSIX_C_SOURCE=200809L)


# add_custom_target(version ALL "${PROJECT_SOURCE_DIR}/get_svnrev.sh" "\"${PROJECT_SOURCE_DIR}\"")

link_directories(${X11_LIBRARY_DIRS}
  ${PANGOCAIRO_LIBRARY_DIRS}
  ${PANGO_LIBRARY_DIRS}
  ${CAIRO_LIBRARY_DIRS}
  ${GLIB2_LIBRARY_DIRS}
  ${GOBJECT2_LIBRARY_DIRS}
  ${IMLIB2_LIBRARY_DIRS}
  ${SN_LIBRARY_DIRS})

add_executable(${PROJECT_NAME} ${SOURCES})
target_link_libraries(${PROJECT_NAME} ${X11_LIBRARIES}
                             ${PANGOCAIRO_LIBRARIES}
                             ${PANGO_LIBRARIES}
                             ${CAIRO_LIBRARIES}
                             ${GLIB2_LIBRARIES}
                             ${GOBJECT2_LIBRARIES}
                             ${IMLIB2_LIBRARIES}
			     ${SN_LIBRARIES})
if(RT_LIBRARY)
  target_link_libraries(${PROJECT_NAME} ${RT_LIBRARY})
endif(RT_LIBRARY)

check_function_exists(strlcat HAS_STRLCAT)
check_function_exists(strlcpy HAS_STRLCPY)
if (HAS_STRLCAT AND HAST_STRLCPY)
  add_definitions(-DHAS_LESS_ERROR_PRONE_STR_FUNCS)
endif(HAS_STRLCAT AND HAST_STRLCPY)
remove_definitions(-DHAS_STRLCAT -DHAS_STRLCPY)

add_dependencies(${PROJECT_NAME} version)
set(CMAKE_C_FLAGS "-std=c11 -O3 -fstack-protector -fPIE")
set(CMAKE_C_FLAGS_DEBUG  "-std=c11 -O0 -pedantic -Wall -g -fstack-protector -Wundef -Wunreachable-code -Winit-self -Wformat-security -fPIE -Wcast-qual -ferror-limit=5 -Wbad-function-cast -Wfloat-equal -Wstrict-prototypes -Wsequence-point -Wextra")

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "release")
endif(NOT CMAKE_BUILD_TYPE)

string(TOLOWER ${CMAKE_BUILD_TYPE} CMAKE_BUILD_TYPE)
if (${CMAKE_BUILD_TYPE} STREQUAL "debug")
  set(CMAKE_C_COMPILER "clang")
  add_definitions(-DTINTO_DEVEL_MODE=1)
  remove_definitions(-DNDEBUG)
endif(${CMAKE_BUILD_TYPE} STREQUAL "debug")

set(CMAKE_EXE_LINKER_FLAGS "-pthread -fpie")

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
install(FILES config-example/tinto.conf DESTINATION ${SYSCONFDIR}/xdg/${PROJECT_NAME})
