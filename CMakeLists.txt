cmake_minimum_required(VERSION 3.15)
project(c-woomer VERSION 1.0.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -Wall -Wextra")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")

# Find required packages
find_package(PkgConfig REQUIRED)

# Try to find raylib using pkg-config first
pkg_check_modules(RAYLIB raylib)

if(NOT RAYLIB_FOUND)
    # If raylib is not found via pkg-config, try to find it using find_package
    find_package(raylib QUIET)
    
    if(NOT raylib_FOUND)
        # If raylib is still not found, fetch it from GitHub
        message(STATUS "raylib not found, fetching from GitHub...")
        
        include(FetchContent)
        FetchContent_Declare(
            raylib
            GIT_REPOSITORY https://github.com/raysan5/raylib.git
            GIT_TAG 5.0
        )
        
        # Set raylib build options
        set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
        
        FetchContent_MakeAvailable(raylib)
    endif()
endif()

# Create executable
add_executable(${PROJECT_NAME}
    src/main.c
)

# Link libraries
if(RAYLIB_FOUND)
    # Using pkg-config found raylib
    target_link_libraries(${PROJECT_NAME} ${RAYLIB_LIBRARIES})
    target_include_directories(${PROJECT_NAME} PRIVATE ${RAYLIB_INCLUDE_DIRS})
    target_compile_options(${PROJECT_NAME} PRIVATE ${RAYLIB_CFLAGS_OTHER})
else()
    # Using CMake found or fetched raylib
    target_link_libraries(${PROJECT_NAME} raylib)
endif()

# Platform-specific libraries
if(WIN32)
    target_link_libraries(${PROJECT_NAME} winmm)
elseif(UNIX AND NOT APPLE)
    # Linux specific libraries
    target_link_libraries(${PROJECT_NAME} m pthread dl)
    
    # Find X11 and other Linux dependencies
    find_package(X11)
    if(X11_FOUND)
        target_link_libraries(${PROJECT_NAME} ${X11_LIBRARIES})
    endif()
endif()

# Copy shaders directory to build directory
if(EXISTS "${CMAKE_SOURCE_DIR}/shaders")
    file(COPY "${CMAKE_SOURCE_DIR}/shaders" DESTINATION "${CMAKE_BINARY_DIR}")
else()
    # Create shaders directory if it doesn't exist
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/shaders")
    message(WARNING "Shaders directory not found. Created empty shaders directory in build folder.")
    message(WARNING "Please add your spotlight.fs shader file to the shaders directory.")
endif()

# Set output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# Installation rules
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

if(EXISTS "${CMAKE_SOURCE_DIR}/shaders")
    install(DIRECTORY "${CMAKE_SOURCE_DIR}/shaders"
        DESTINATION share/${PROJECT_NAME}
    )
endif()

# Print build information
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C compiler: ${CMAKE_C_COMPILER}")
message(STATUS "Project: ${PROJECT_NAME} v${PROJECT_VERSION}")
