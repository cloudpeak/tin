set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Ensure all targets in the build tree use the same MSVC runtime library.
# Abseil sets CMAKE_MSVC_RUNTIME_LIBRARY in its own subdirectory scope, which
# does not propagate to the parent scope. Without this, abseil objects get
# _ITERATOR_DEBUG_LEVEL=2 (Debug DLL runtime) while tin/examples get
# _ITERATOR_DEBUG_LEVEL=0, causing lld-link /failifmismatch errors.
# This must be set BEFORE any add_subdirectory calls.
# Use CACHE FORCE so the value survives across reconfigures and wins over
# any default that project() or toolchain files might set.
if(WIN32)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" CACHE STRING "" FORCE)
endif()

# base (Chromium //base) requires C++20 (class-scope deduction guides in
# weak_ptr.h, etc.). tin itself only needs C++17, but since base headers are
# included from tin sources, we have to compile the whole tree as C++20.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Jumbo (unity) build for base — merges multiple .cc files into fewer
# translation units to speed up compilation. Pass -DBASE_JUMBO_BUILD=ON
# to enable. Default OFF for debuggability.
option(BASE_JUMBO_BUILD "Enable jumbo (unity) build for base" OFF)

# build third party.
if(NOT BASE_DIR)
	set(BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/base)
	message("BASE_DIR not defined, default to ${BASE_DIR}")
	# Pass the abseil-cpp source directory down to base's CMakeLists.txt so
	# it can find absl regardless of where base is consumed from. Without
	# this, base would look for absl at ${BASE_DIR}/../third_party/abseil-cpp
	# (i.e. tin/third_party/third_party/abseil-cpp), which does not exist.
	# Use CACHE FORCE so the value survives reconfigures and wins over
	# base's own default in ${CHROMIUM_SRC}/third_party/abseil-cpp.
	set(BASE_ABSL_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/abseil-cpp
		CACHE PATH "Path to abseil-cpp source tree" FORCE)
	add_subdirectory("${BASE_DIR}")
	set_property(TARGET base PROPERTY FOLDER "tin_deps")
endif()

if(NOT ZCONTEXT_DIR)
	set(ZCONTEXT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zcontext)
	message("ZCONTEXT_DIR not defined, default to ${ZCONTEXT_DIR}") 
	add_subdirectory("${ZCONTEXT_DIR}")
	set_property(TARGET zcontext PROPERTY FOLDER "tin_deps")
endif()




cmake_minimum_required(VERSION 2.6)
project(tin CXX)

# Re-assert the MSVC runtime library after project(tin) — project() can
# reset CMAKE_MSVC_RUNTIME_LIBRARY to its default. All targets (tin,
# examples) must use the same runtime as abseil/base (MultiThreadedDebugDLL
# in Debug, which sets _ITERATOR_DEBUG_LEVEL=2).
# Use CACHE FORCE so the value survives across reconfigures and wins over
# any default that project() or toolchain files might set.
if(WIN32)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" CACHE STRING "" FORCE)
endif()

# Install/packaging helpers. Included early so that
# ${CMAKE_INSTALL_INCLUDEDIR} etc. are available to
# target_include_directories below.
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)


# default to debug build.
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

message("Current build type is : ${CMAKE_BUILD_TYPE}")
message("path is  : ${PROJECT_SOURCE_DIR}")
message("sysmtem name is  : ${CMAKE_SYSTEM_NAME}")
message("compiler is ${CMAKE_CXX_COMPILER_ID}")

# On WSL/Linux with Homebrew clang, force libc++ (libstdc++ 11 headers
# don't survive clang 22). base/CMakeLists.txt already does this in its own
# scope, but tin's own sources need it too.
# Guard with UNIX so this does NOT run on Windows clang-cl (which uses the
# MSVC standard library and has no /home/linuxbrew path).
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND UNIX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++ -L/home/linuxbrew/.linuxbrew/lib")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -stdlib=libc++ -L/home/linuxbrew/.linuxbrew/lib")
  link_directories(/home/linuxbrew/.linuxbrew/lib)
endif()

if(CMAKE_SYSTEM_NAME MATCHES ".*BSD.*" OR CMAKE_SYSTEM_NAME MATCHES "Darwin")
message("Darwin or BSD variants found")
endif()
if(MSVC)
    message("MSVC version is: ${MSVC_VERSION}")
endif()


include_directories(
		${CMAKE_INSTALL_PREFIX}/include
)

link_directories(
		${CMAKE_INSTALL_PREFIX}/lib
)

if (UNIX)
# posix MACROS
add_definitions(-D__STDC_FORMAT_MACROS)
elseif(WIN32)
# win32 MACROS
add_definitions(-D_SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS)
add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
add_definitions(-D_SCL_SECURE_NO_DEPRECATE)
add_definitions(-D__STDC_CONSTANT_MACROS)
add_definitions(-D__STDC_FORMAT_MACROS)
add_definitions(-DCERT_CHAIN_PARA_HAS_EXTRA_FIELDS)
add_definitions(-DNOMINMAX)
add_definitions(-D_UNICODE)
add_definitions(-DUNICODE)
add_definitions(-D_CRT_RAND_S)
add_definitions(-D"USE_ASH=1")
endif()

# compiler flags.
if (UNIX)
    # Append (do NOT overwrite) — CMAKE_CXX_FLAGS already contains
    # `-stdlib=libc++` set above for Clang, and we must keep it so tin's own
    # sources use the same standard library as base.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb -Wall -Wextra -Woverloaded-virtual -Wno-unused-parameter -Wno-missing-field-initializers")
elseif(WIN32)
    # Do NOT set CMAKE_CXX_FLAGS with MSVC switches (/Zi, /EHsc, etc.).
    # Setting them via CMAKE_CXX_FLAGS causes CMake to emit them as
    # <AdditionalOptions> in the .vcxproj, which overrides the
    # <RuntimeLibrary> property (leaving it empty) and causes
    # _ITERATOR_DEBUG_LEVEL mismatches with abseil/base.
    # Instead, let CMake's defaults handle runtime library selection
    # (MultiThreadedDebugDLL via CMAKE_MSVC_RUNTIME_LIBRARY) and use
    # target_compile_options on the tin target below for extra flags.
endif()

# -Wno-unnamed-type-template-args is a GCC/Clang flag. Guard with UNIX so
# it does not interfere with the MSVC generator on Windows clang-cl.
if(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang" AND UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unnamed-type-template-args")
endif()

string(REPLACE ";" " " CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

# P2-1: include/ is the PUBLIC include path (clean public headers, no
# runtime/ internals). The project root is PRIVATE to the tin target so
# that internal .cc files can still find tin/runtime/... headers.
include_directories(${PROJECT_SOURCE_DIR}/include)
# Do NOT add ${BASE_DIR} to the global include path here. Doing so adds
# `-I tin/third_party/base`, which shadows glibc's <features.h> with base's
# features.h — and libstdc++ 11's <bits/os_defines.h> does `#include
# <features.h>` to pull in glibc macros like __BEGIN_DECLS / __THROW / __N.
# base_compile_config (transitively applied via `target_link_libraries(tin
# PUBLIC base)`) already provides `-I ${CHROMIUM_SRC}` (so `#include
# "base/..."` resolves) and `-iquote ${BASE_ROOT}` (so `#include
# "features.h"` / `#include "build/build_config.h"` resolve, without
# affecting `#include <features.h>`).
# tin sources use base:: directly (strlcpy, NetToHost16, RefCountedThreadSafe,
# build_config) and std::atomic for atomic operations. The former cliff_shim
# and quark (Boost.Atomic fork) dependencies have been removed.
include_directories(${ZCONTEXT_DIR}/zcontext/include)

if (UNIX)
# Note: do NOT hard-code CMAKE_CXX_COMPILER here — the caller picks the
# compiler via -DCMAKE_CXX_COMPILER=... (clang++ on WSL, g++ elsewhere).
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 ")
endif()

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)

set(SOURCES 
tin/io/io.cc
tin/io/io_buffer.cc
tin/net/address_family.cc
tin/net/address_list.cc
tin/net/dialer.cc
tin/net/fd_mutex.cc
tin/net/inet.cc
tin/net/ip_address.cc
tin/net/ip_endpoint.cc
tin/net/listener.cc
tin/net/net.cc
tin/net/netfd_common.cc
tin/net/poll_desc.cc
tin/net/resolve.cc
tin/net/sockaddr_storage.cc
tin/net/tcp_conn.cc
tin/bufio/bufio.cc
tin/runtime/env.cc
tin/runtime/greenlet.cc
tin/runtime/m.cc
tin/runtime/p.cc
tin/runtime/raw_mutex_sema.cc
tin/runtime/runtime.cc
tin/runtime/scheduler.cc
tin/runtime/semaphore.cc
tin/runtime/threadpoll.cc
tin/runtime/unlock.cc
tin/runtime/util.cc
tin/runtime/spin.cc
tin/runtime/sysmon.cc
tin/runtime/net/netpoll.cc
tin/runtime/net/pollops.cc
tin/runtime/net/poll_descriptor.cc
tin/runtime/stack/fixedsize_stack.cc
tin/runtime/stack/stack.cc
tin/runtime/timer/timer_queue.cc
tin/sync/cond.cc
tin/sync/mutex.cc
tin/sync/rwmutex.cc
tin/sync/wait_group.cc
tin/time/time.cc
tin/config/config.cc
tin/status.cc
tin/tin.cc
tin/util/unique_id.cc
)

if (WIN32)
    LIST(APPEND SOURCES
        tin/net/winsock_util.cc
        tin/net/netfd_windows.cc
        tin/error/error_windows.cc
        tin/platform/platform_win.cc
        tin/runtime/os_win.cc
        tin/runtime/net/netpoll_windows.cc
        tin/runtime/stack/protected_fixedsize_stack_win.cc
    )
endif()

if (UNIX)
    LIST(APPEND SOURCES
        tin/runtime/os_posix.cc
		    tin/runtime/posix_util.cc
        tin/net/netfd_posix.cc
		    tin/platform/platform_posix.cc
        tin/error/error_posix.cc
		    tin/runtime/stack/protected_fixedsize_stack_posix.cc     
    )
endif()

if(CMAKE_SYSTEM_NAME MATCHES ".*BSD.*" OR CMAKE_SYSTEM_NAME MATCHES "Darwin")
    LIST(APPEND SOURCES
        tin/runtime/net/netpoll_kqueue.cc
    )
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    LIST(APPEND SOURCES
        tin/runtime/net/netpoll_epoll.cc
    )
endif()

if (WIN32)
    LIST(APPEND SOURCES
		tin/bufio/bufio.h
			tin/communication/chan.h
			tin/config/config.h
		tin/config/default.h
		tin/error/error.h
		tin/error/error_inl.h
		tin/io/io.h
		tin/io/io_buffer.h
			tin/net/address_family.h
		tin/net/address_list.h
		tin/net/dialer.h
		tin/net/fd_mutex.h
		tin/net/inet.h
		tin/net/ip_address.h
		tin/net/ip_endpoint.h
		tin/net/listener_impl.h
		tin/net/net.h
		tin/net/netfd.h
		tin/net/netfd_common.h
		tin/net/netfd_posix.h
		tin/net/netfd_windows.h
		tin/net/poll_desc.h
		tin/net/resolve.h
		tin/net/sockaddr_storage.h
		tin/net/sys_addrinfo.h
		tin/net/sys_socket.h
		tin/net/tcp_conn_impl.h
		tin/net/winsock_util.h
		tin/platform/platform.h
		tin/platform/platform_win.h
		tin/runtime/env.h
		tin/runtime/greenlet.h
		tin/runtime/guintptr.h
		tin/runtime/m.h
		tin/runtime/p.h
		tin/runtime/raw_mutex.h
		tin/runtime/runtime.h
		tin/runtime/scheduler.h
		tin/runtime/semaphore.h
		tin/runtime/spawn.h
		tin/runtime/threadpoll.h
		tin/runtime/unlock.h
		tin/runtime/util.h
		tin/runtime/spin.h
		tin/runtime/sysmon.h
		tin/runtime/net/NetPoll.h
		tin/runtime/net/pollops.h
		tin/runtime/net/poll_descriptor.h
		tin/runtime/stack/fixedsize_stack.h
		tin/runtime/stack/protected_fixedsize_stack.h
		tin/runtime/stack/stack.h
		tin/runtime/timer/timer_queue.h
		tin/sync/atomic.h
		tin/sync/atomic_flag.h
		tin/sync/mutex.h
		tin/sync/rwmutex.h
		tin/sync/wait_group.h
		tin/time/time.h
		tin/util/unique_id.h
	    )
endif()

macro(source_group_by_dir source_files)
    if(MSVC)
        set(sgbd_cur_dir ${CMAKE_CURRENT_SOURCE_DIR})
        foreach(sgbd_file ${${source_files}})
            string(REGEX REPLACE ${sgbd_cur_dir}/\(.*\) \\1 sgbd_fpath ${sgbd_file})
            string(REGEX REPLACE "\(.*\)/.*" \\1 sgbd_group_name ${sgbd_fpath})
            string(COMPARE EQUAL ${sgbd_fpath} ${sgbd_group_name} sgbd_nogroup)
            string(REPLACE "/" "\\" sgbd_group_name ${sgbd_group_name})
            if(sgbd_nogroup)
                set(sgbd_group_name "\\")
            endif(sgbd_nogroup)
            source_group(${sgbd_group_name} FILES ${sgbd_file})
        endforeach(sgbd_file)
    endif(MSVC)
endmacro(source_group_by_dir)

source_group_by_dir(SOURCES)

add_library(tin ${SOURCES})

# Explicitly set the MSVC runtime library for the tin target and all
# downstream examples. CMAKE_MSVC_RUNTIME_LIBRARY should handle this, but
# with the ClangCL toolset the generated <RuntimeLibrary> property can end
# up empty. Setting /MDd (Debug) /MD (Release) via target_compile_options
# ensures the correct runtime is used, matching abseil/base.
if(WIN32)
  target_compile_options(tin PRIVATE
    "$<$<CONFIG:Debug>:/MDd>"
    "$<$<CONFIG:Release>:/MD>"
    "$<$<CONFIG:RelWithDebInfo>:/MD>"
    "$<$<CONFIG:MinSizeRel>:/MD>"
  )
endif()

# P2-1: Export include/ as the PUBLIC include path so that downstream
# targets (examples, `find_package(tin)` consumers) resolve `#include "tin/..."`
# via the clean public headers. The project root remains PRIVATE (internal).
target_include_directories(tin PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Internal include path: project root (for tin/runtime/..., tin/net/sys_socket.h, etc.)
target_include_directories(tin PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
)

# tin's public headers include <absl/...> and <base/...>, so the dependency
# on base (and the absl components not already PUBLIC-linked by base) must
# be PUBLIC for downstream targets (e.g. examples) to compile.
target_link_libraries(tin PUBLIC
	base
	absl::log
	absl::check
	absl::str_format
	absl::bind_front
)



file(GLOB ABSL_LIBS "${CMAKE_INSTALL_PREFIX}/lib/*absl_*")
if (WIN32)
# tin now uses base:: directly and std::atomic; the former cliff_shim and
# quark dependencies have been removed.
set(DEP_LIBS tin base zcontext DbgHelp.lib Psapi.lib Mswsock.lib ws2_32.lib version.lib
	${ABSL_LIBS}
)

if(${MSVC_VERSION} GREATER 1500)
LIST(APPEND DEP_LIBS
	Mincore.lib
	)

endif()
elseif(APPLE)
set(DEP_LIBS tin base zcontext pthread)
else()
set(DEP_LIBS tin base zcontext pthread rt absl::log_globals)
endif()

if(NOT DEFINED TIN_BUILD_EXAMPLES)
	set(TIN_BUILD_EXAMPLES 1)
endif()
if(${TIN_BUILD_EXAMPLES})
add_subdirectory(examples)
endif()

# Unit tests (optional). Pass -DTIN_BUILD_TESTS=OFF to skip.
if(NOT DEFINED TIN_BUILD_TESTS)
	set(TIN_BUILD_TESTS 1)
endif()
if(${TIN_BUILD_TESTS})
	add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# Install rules.
# ---------------------------------------------------------------------------
# Install the static library and all public headers so that downstream
# projects can link against tin via target_link_libraries(my_app PRIVATE tin)
# and target_include_directories(my_app PRIVATE /path/to/tin/install/include).
#
# NOTE: full find_package(tin) support (via install(EXPORT) + tinConfig.cmake)
# is not yet enabled because tin's PUBLIC link dependencies (base, absl::*,
# zcontext) are built via add_subdirectory and are not part of any
# CMake export set. Enabling install(EXPORT) requires those dependencies to
# be exported first (or found via find_dependency in the config file). That
# is tracked as a follow-up task — see docs/api-design-review.md §2.5.
# ---------------------------------------------------------------------------

install(TARGETS tin
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install all public headers under tin/ (preserving the directory layout).
# Only *.h files are installed; internal headers that happen to live under
# tin/ are also installed, which is harmless and keeps the include paths
# stable for code that still references e.g. tin/runtime/runtime.h.
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tin/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/tin
  FILES_MATCHING PATTERN "*.h"
)

