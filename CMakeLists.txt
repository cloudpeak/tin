set_property(GLOBAL PROPERTY USE_FOLDERS ON)
# build third party.
if(NOT BASE03_DIR)
	set(BASE03_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/base03)
	message("BASE03_DIR not defined, default to ${BASE03_DIR}") 
	add_subdirectory("${BASE03_DIR}")
	set_property(TARGET base PROPERTY FOLDER "tin_deps")
endif()

if(NOT QUARK_DIR)
	set(QUARK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/quark)
	message("QUARK_DIR not defined, default to ${QUARK_DIR}") 
	add_subdirectory("${QUARK_DIR}")
	set_property(TARGET quark PROPERTY FOLDER "tin_deps")
endif()

if(NOT ZCONTEXT_DIR)
	set(ZCONTEXT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zcontext)
	message("ZCONTEXT_DIR not defined, default to ${ZCONTEXT_DIR}") 
	add_subdirectory("${ZCONTEXT_DIR}")
	set_property(TARGET zcontext PROPERTY FOLDER "tin_deps")
endif()

cmake_minimum_required(VERSION 2.6)
project(tin CXX)


# default to debug build.
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

message("Current build type is : ${CMAKE_BUILD_TYPE}")
message("path is  : ${PROJECT_SOURCE_DIR}")
message("sysmtem name is  : ${CMAKE_SYSTEM_NAME}")
message("compiler is ${CMAKE_CXX_COMPILER_ID}")

if(CMAKE_SYSTEM_NAME MATCHES ".*BSD.*" OR CMAKE_SYSTEM_NAME MATCHES "Darwin")
message("Darwin or BSD variants found")
endif()
if(MSVC)
    message("MSVC version is: ${MSVC_VERSION}")
endif()

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
add_definitions(-D_CRT_RAND_S)
add_definitions(-D"USE_ASH=1")
endif()

# compiler flags.
if (UNIX)
    set(CMAKE_CXX_FLAGS
        -ggdb
        -Wall
        -Wextra 
        -Woverloaded-virtual
        -Wno-unused-parameter
        -Wno-missing-field-initializers 
		#-Wno-reorder
    )
elseif(WIN32)
    set(CMAKE_CXX_FLAGS 
        /Zi 
        /nologo  
        /EHsc 
        /GS 
        /Gd 
        /GR 
        /GF 
        /fp:precise 
        /Zc:wchar_t 
        /Zc:forScope 
        /errorReport:queue
        /FC
        /W3 
        /WX- 
    )
	if(${MSVC_VERSION} GREATER 1700)
	LIST(APPEND CMAKE_CXX_FLAGS
		/d2Zi+ 
		)
	endif()
endif()

if(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unnamed-type-template-args")
endif()

string(REPLACE ";" " " CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

include_directories(${PROJECT_SOURCE_DIR})
include_directories(${BASE03_DIR})
include_directories(${QUARK_DIR}/include)
include_directories(${ZCONTEXT_DIR}/zcontext/include)

if (UNIX)
set(CMAKE_CXX_COMPILER "g++")
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 ")
endif()

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)

set(SOURCES 
tin/io/io.cc
tin/io/ioutil.cc
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
tin/bufio/buffered_reader.cc
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
		tin/all.h
		tin/tin.h
		tin/bufio/bufio.h
		tin/bufio/buffered_reader.h
		tin/communication/chan.h
		tin/communication/queue.h
		tin/config/config.h
		tin/config/default.h
		tin/error/error.h
		tin/error/error_inl.h
		tin/io/io.h
		tin/io/ioutil.h
		tin/io/io_buffer.h
		tin/net/address_family.h
		tin/net/address_list.h
		tin/net/dialer.h
		tin/net/fd_mutex.h
		tin/net/inet.h
		tin/net/ip_address.h
		tin/net/ip_endpoint.h
		tin/net/listener.h
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
		tin/net/tcp_conn.h
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
		tin/sync/cond.h
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

if (WIN32)
set(DEP_LIBS tin base quark zcontext DbgHelp.lib Psapi.lib Mswsock.lib ws2_32.lib version.lib)

if(${MSVC_VERSION} GREATER 1500)
LIST(APPEND DEP_LIBS
	Mincore.lib
	)

endif()
elseif(APPLE)
set(DEP_LIBS tin base quark zcontext pthread)
else()
set(DEP_LIBS tin base quark zcontext pthread rt)
endif()
if(TIN_NO_EXAMPLES EQUAL 0)
add_subdirectory(examples)
endif()

