#-------------------------------------------------------------------------
# codec2 - CMake
#-------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.5)

project(codec2)

# Compiler flags
set(CMAKE_CXX_STANDARD 17)
# TODO add -Werror
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-error=deprecated-declarations -Wno-enum-compare")
set(AGL_LINUX OFF CACHE BOOL "AGL(Automotive Grade Linux): ON|OFF. Default [OFF]")
set(SUPPORT_DMABUF_HEAP OFF CACHE BOOL "Support DMA buffer heap: ON|OFF. Default [OFF]")

if(AGL_LINUX)
    add_definitions(-D_AGL_LINUX_)
endif()

if(SUPPORT_DMABUF_HEAP)
    add_definitions(-D_SUPPORT_DMABUF_)
endif()

# Build debug version
# set(CMAKE_BUILD_TYPE Debug)

# Include directories
set(build_include_dirs
#C2 includes
    core/include
    sfplugin
    vndk/include
    vndk/internal
    vndk_platform/include)

include_directories(${build_include_dirs})

# Output path
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

#-------------------------------------------------------------------------
# libcodec2_vndk
#-------------------------------------------------------------------------

set(CODEC2_VNDK_SOURCE
#libstagefright foundation
    ../libstagefright/foundation/AString.cpp
#C2 vndk code
    vndk/util/C2ParamUtils.cpp
    vndk/util/C2InterfaceUtils.cpp
    vndk/util/C2InterfaceHelper.cpp
    vndk/util/C2Debug.cpp
    vndk/C2Config.cpp
    vndk/C2Buffer.cpp
    vndk/C2Store.cpp
    vndk/C2AllocatorIon.cpp
    sfplugin/ReflectedParamUpdater.cpp
#C2 platform vndk implementation
    vndk_platform/C2AllocatorGBM.cpp
    vndk_platform/LinuxC2Debug.cpp
    )

if(SUPPORT_DMABUF_HEAP)
    set(CODEC2_VNDK_SOURCE ${CODEC2_VNDK_SOURCE}
        vndk_platform/C2DmaLinearAllocator.cpp)
endif()

add_library(codec2_vndk SHARED ${CODEC2_VNDK_SOURCE})
target_link_libraries(codec2_vndk ion cutils dl)
if(SUPPORT_DMABUF_HEAP)
    target_link_libraries(codec2_vndk dmabufheap)
endif()

set(CODEC2_VNDK_LIBS
    codec2_vndk)

set(CODEC2_INCLUDES
    vndk_platform/include/
    vndk/internal/
    sfplugin/
    core/include/
    vndk/include/)

install(TARGETS ${CODEC2_VNDK_LIBS} DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(DIRECTORY ${CODEC2_INCLUDES} DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
