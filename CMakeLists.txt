cmake_minimum_required(VERSION 3.10...3.22)
project(GVN-learning
        VERSION 1.0.0
        LANGUAGES C CXX)
cmake_minimum_required(VERSION 3.22)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Preferred mold as linker☺️
find_program(MOLD_PATH NAMES mold)
if(MOLD_PATH)
    message(STATUS "Mold is installed, using mold as linker")
    set(CMAKE_LINKER mold)
endif()

# Replace the existing gc-sections flags with platform-specific ones
if (APPLE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-dead_strip")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-dead_strip")
else()
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--gc-sections")
endif()

find_package(LLVM REQUIRED)
include(AddLLVM)
message(STATUS "Found LLVM: " ${LLVM_FOUND})
message(STATUS "LLVM version: " ${LLVM_VERSION})
message(STATUS "LLVM include directory: " ${LLVM_INCLUDE_DIRS})
message(STATUS "LLVM library directory: " ${LLVM_LIBRARY_DIRS})
message(STATUS "LLVM install path: " ${LLVM_INSTALL_PREFIX})
set(LLVM_DIR ${LLVM_INSTALL_PREFIX})
add_definitions(${LLVM_DEFINITIONS})
link_directories(${LLVM_LIBRARY_DIRS})
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})


# Set runtime bin/lib output path
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Very very important when importing LLVM library
if(NOT LLVM_ENABLE_RTTI)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif()

add_subdirectory(src)
