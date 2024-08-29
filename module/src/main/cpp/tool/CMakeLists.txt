cmake_minimum_required(VERSION 3.18.1)

if (NOT DEFINED TOOL_NAME)
    message(FATAL_ERROR "TOOL_NAME is not set")
else ()
    project(${TOOL_NAME})
endif ()

set(SRC_FILE "${CMAKE_SOURCE_DIR}/include/logcat.h")
add_custom_command(
        OUTPUT "${SRC_FILE}"
        COMMAND sed -i '' 's|".*"|"/data/adb/modules/${MODULE_DIR}/config"|g' "${SRC_FILE}"
        COMMENT "Replacing strings in logcat.h with the config file path"
)
add_custom_target(replace_config ALL DEPENDS "${SRC_FILE}")

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

add_executable(${TOOL_NAME} main.cpp logcat.cpp)
add_dependencies(${TOOL_NAME} replace_config)
target_link_libraries(${TOOL_NAME} log)

if (NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_custom_command(TARGET ${TOOL_NAME} POST_BUILD
            COMMAND ${CMAKE_STRIP} --strip-all --remove-section=.comment "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${TOOL_NAME}")
endif ()