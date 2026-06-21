cmake_minimum_required(VERSION 4.2.0)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(vulkan_render)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ===== Build type configuration =====
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Configurations" FORCE)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
    # Debug/Release compiler flags
    set(CMAKE_CXX_FLAGS_DEBUG   "-g -O0 -D_DEBUG"                 CACHE STRING "Flags for Debug"   FORCE)
    set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG"                     CACHE STRING "Flags for Release" FORCE)
    set(CMAKE_C_FLAGS_DEBUG     "-g -O0 -D_DEBUG"                 CACHE STRING "C Flags Debug"     FORCE)
    set(CMAKE_C_FLAGS_RELEASE   "-O2 -DNDEBUG"                     CACHE STRING "C Flags Release"   FORCE)
endif()

#Disable the built-in tests and examples of SPIRV-Reflex
set(SPIRV_REFLECT_BUILD_TESTS OFF CACHE BOOL "")
set(SPIRV_REFLECT_BUILD_EXAMPLES OFF CACHE BOOL "")

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm OPTIONAL)
find_program(SLANGC_COMPILER slangc REQUIRED)

file(GLOB_RECURSE SRC_LIST 
    "${PROJECT_SOURCE_DIR}/src/*.cpp"
    "${PROJECT_SOURCE_DIR}/src/*.cc" 
)

set(SHADER_DIR ${PROJECT_SOURCE_DIR}/shaders)
set(SHADER_SOURCE ${SHADER_DIR}/shader.slang)
set(SHADER_OUTPUT ${SHADER_DIR}/slang.spv) 

add_custom_command(
    OUTPUT ${SHADER_OUTPUT}
    COMMAND ${SLANGC_COMPILER} ${SHADER_SOURCE}
        -target spirv
        -profile spirv_1_4
        -emit-spirv-directly
        -fvk-use-entrypoint-name
        -entry vertMain
        -entry fragMain
        -o ${SHADER_OUTPUT}
    DEPENDS ${SHADER_SOURCE}
    COMMENT "Compiling Slang shader to SPIR-V"
    VERBATIM
)

add_custom_target(CompileShaders ALL
    DEPENDS ${SHADER_OUTPUT}
)

add_executable(
    main
    ${SRC_LIST}
)
add_dependencies(main CompileShaders)

target_compile_definitions(main PUBLIC SPIRV_REFLECT_USE_SYSTEM_SPIRV_H)

set_target_properties(main PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
    WIN32_EXECUTABLE FALSE
)

target_link_libraries(
    main
    PRIVATE
    Vulkan::Vulkan
    glfw
)

target_include_directories(
    main
    PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${Vulkan_INCLUDE_DIRS}
    ${GLFW3_INCLUDE_DIRS}
    ${GLM_INCLUDE_DIRS}
)




