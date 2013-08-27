cmake_minimum_required(VERSION 2.8)

project(cf4ocl C)

add_definitions(-std=c99 -Wall -Werror)

include(FindPkgConfig)

pkg_check_modules(GLIB glib-2.0)

if (APPLE)
    find_path(CL_INCLUDE_DIRS OpenCL/cl.h HINTS ${CMAKE_FIND_ROOT_PATH})
    find_library(CL_LIBRARIES OpenCL)
else()
    find_path(CL_INCLUDE_DIRS CL/cl.h HINTS ${CMAKE_FIND_ROOT_PATH})
    find_library(CL_LIBRARY NAMES OpenCL libOpenCL)
    find_library(GAL_LIBRARY NAMES GAL libGAL)
    set(CL_LIBRARIES ${CL_LIBRARY} ${CLC_LIBRARY} ${GAL_LIBRARY})    
endif()

message("Glib ${GLIB_INCLUDE_DIRS} ${GLIB_LIBRARIES}")
message("OpenCL ${CL_INCLUDE_DIRS} ${CL_LIBRARIES}")

include_directories(${GLIB_INCLUDE_DIRS} ${CL_INCLUDE_DIRS})
link_directories(${GLIB_LIBRARY_DIRS})

add_library(cf4ocl SHARED clutils.c clprofiler.c)
target_link_libraries(cf4ocl ${GLIB_LIBRARIES} ${CL_LIBRARIES})
install(TARGETS cf4ocl LIBRARY DESTINATION lib)
install(FILES clutils.h clprofiler.h gerrorf.h DESTINATION include)

add_executable(cl_device_query device_query.c)
target_link_libraries(cl_device_query cf4ocl)
install(TARGETS cl_device_query RUNTIME DESTINATION bin)

add_executable(cl_kernel_info kernel_info.c)
target_link_libraries(cl_kernel_info cf4ocl)
install(TARGETS cl_kernel_info RUNTIME DESTINATION bin)

