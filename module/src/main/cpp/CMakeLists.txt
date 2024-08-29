cmake_minimum_required(VERSION 3.18.1)

if (NOT DEFINED MODULE_NAME)
    message(FATAL_ERROR "MODULE_NAME is not set")
else ()
    project(${MODULE_NAME})
endif ()

message("Build type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_CXX_STANDARD 20)

set(LINKER_FLAGS "-ffixed-x18 -Wl,--hash-style=both")
set(C_FLAGS "-fdata-sections -ffunction-sections")
set(CXX_FLAGS "${CXX_FLAGS} -fno-exceptions -fno-rtti")

if (NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(C_FLAGS "${C_FLAGS} -O2 -fvisibility=hidden -fvisibility-inlines-hidden")
    set(LINKER_FLAGS "${LINKER_FLAGS} -Wl,-exclude-libs,ALL -Wl,--gc-sections -Wl,--strip-all")
else ()
    set(C_FLAGS "${C_FLAGS} -O0")
endif ()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${C_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${C_FLAGS} ${CXX_FLAGS}")

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${LINKER_FLAGS}")
set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${LINKER_FLAGS}")

# Add the directory containing header files to the include path
include_directories(${CMAKE_SOURCE_DIR}/include)
file(GLOB SOFIXER_SOURCES SoFixer/*.cpp)
add_library(${MODULE_NAME} SHARED main.cpp gumhook.cpp dumpso.cpp ${SOFIXER_SOURCES})

## ------------------------------------------------------------------------------
# Import the frida-gum static libraries for each architecture
add_library(frida-gum-${ANDROID_ABI} STATIC IMPORTED)
set_target_properties(frida-gum-${ANDROID_ABI} PROPERTIES IMPORTED_LOCATION
        ${CMAKE_SOURCE_DIR}/frida-gum/${ANDROID_ABI}/libfrida-gum.a)
set_target_properties(frida-gum-${ANDROID_ABI} PROPERTIES INCLUDE_DIRECTORIES ${CMAKE_SOURCE_DIR}/include)
## ------------------------------------------------------------------------------

target_link_libraries(
        ${MODULE_NAME}
        $<TARGET_PROPERTY:frida-gum-${ANDROID_ABI},IMPORTED_LOCATION>
        log)

if (NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_custom_command(TARGET ${MODULE_NAME} POST_BUILD
            COMMAND ${CMAKE_STRIP} --strip-all --remove-section=.comment "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/lib${MODULE_NAME}.so")
endif ()

add_subdirectory(tool)
