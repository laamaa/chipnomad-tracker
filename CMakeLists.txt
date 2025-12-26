cmake_minimum_required(VERSION 3.10)
project(chipnomad C)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Print build configuration
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")

# Debug build flags
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g3 -O0 -DDEBUG")
# Release build flags
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Os -flto -DNDEBUG")

# Optional: Address sanitizer for debug builds
option(ENABLE_ASAN "Enable AddressSanitizer for debug builds" OFF)
if(ENABLE_ASAN AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fsanitize=address -fno-omit-frame-pointer")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=address")
endif()

# Optional: Undefined behavior sanitizer for debug builds
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer for debug builds" OFF)
if(ENABLE_UBSAN AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fsanitize=undefined -fno-omit-frame-pointer")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=undefined")
endif()

# Platform detection
if(APPLE)
    include_directories(/opt/homebrew/include)
    link_directories(/opt/homebrew/lib)
    add_definitions(-DDESKTOP_BUILD)
elseif(UNIX)
    include_directories(/usr/include)
    link_directories(/usr/lib)
    add_definitions(-DDESKTOP_BUILD)
endif()

# Mobile build option
option(MOBILE_BUILD "Enable mobile build" OFF)
if(MOBILE_BUILD)
    add_definitions(-DMOBILE_BUILD)
endif()

# Source directories
set(COMMON_LIBS_DIRS
    tracker/src
    chipnomad_lib
    chipnomad_lib/chips
    chipnomad_lib/external/ayumi
    chipnomad_lib/export
    chipnomad_lib/corelib_file_stdio
    tracker/src/corelib
    tracker/src/import
    tracker/src/screens
    tracker/platforms/shared
    tracker/platforms/sdl3-mobile
)

# Collect all source files (non-recursive, matching Makefile behavior)
foreach(dir ${COMMON_LIBS_DIRS})
    file(GLOB dir_sources "${dir}/*.c")
    list(APPEND SOURCES ${dir_sources})
endforeach()

# Include directories
set(INCLUDE_DIRS ${COMMON_LIBS_DIRS})
list(APPEND INCLUDE_DIRS chipnomad_lib)

# Add include directories
foreach(dir ${INCLUDE_DIRS})
    include_directories(${dir})
endforeach()

# Find SDL3
# Try pkg-config first, then fall back to find_library
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(SDL3 QUIET SDL3)
endif()

# Create executable
add_executable(chipnomad ${SOURCES})

# Link SDL3
if(SDL3_FOUND)
    # Use pkg-config results
    target_include_directories(chipnomad PRIVATE ${SDL3_INCLUDE_DIRS})
    target_link_directories(chipnomad PRIVATE ${SDL3_LIBRARY_DIRS})
    target_link_libraries(chipnomad ${SDL3_LIBRARIES} m)
    target_compile_options(chipnomad PRIVATE ${SDL3_CFLAGS_OTHER})
else()
    # Fall back to find_library
    find_library(SDL3_LIBRARY 
        NAMES SDL3
        PATHS 
            /opt/homebrew/lib
            /usr/lib
            /usr/local/lib
    )
    
    find_path(SDL3_INCLUDE_DIR
        NAMES SDL3/SDL.h
        PATHS
            /opt/homebrew/include
            /usr/include
            /usr/local/include
    )
    
    if(NOT SDL3_LIBRARY OR NOT SDL3_INCLUDE_DIR)
        message(FATAL_ERROR "SDL3 not found. Please install SDL3 development libraries.")
    endif()
    
    target_include_directories(chipnomad PRIVATE ${SDL3_INCLUDE_DIR})
    target_link_libraries(chipnomad ${SDL3_LIBRARY} m)
endif()

# Set output directory
set_target_properties(chipnomad PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/sdl3-mobile"
)

# Make executable
if(UNIX)
    add_custom_command(TARGET chipnomad POST_BUILD
        COMMAND chmod +x $<TARGET_FILE:chipnomad>
    )
endif()

