cmake_minimum_required(VERSION 3.28)
project(blockgame)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD_REQUIRED ON)

function(setup_target_warnings_and_sanitizers target)
    target_compile_options(${target} PRIVATE
        $<$<C_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wshadow -fdiagnostics-color=always>
        $<$<C_COMPILER_ID:MSVC>:/W4>
    )
    set(SANITIZER_EXPR $<$<AND:$<C_COMPILER_ID:GNU,Clang>,$<NOT:$<PLATFORM_ID:Windows>>,$<CONFIG:Debug>>:-fsanitize=address,undefined,leak>)
    target_compile_options(${target} PRIVATE ${SANITIZER_EXPR})
    target_link_options(${target} PRIVATE ${SANITIZER_EXPR})
endfunction()

# fetch an compile raylib
include(FetchContent)

FetchContent_Declare(
  raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG 5.5  # or a specific commit
)

FetchContent_MakeAvailable(raylib)

# compile the client
add_executable(blockgame
    blockgame.c
)

target_link_libraries(blockgame PRIVATE raylib)
setup_target_warnings_and_sanitizers(blockgame)

# copy compile_commands.json from the build dir to the source dir
add_custom_target(copy_compile_commands ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_BINARY_DIR}/compile_commands.json"
        "${CMAKE_SOURCE_DIR}/compile_commands.json"
    COMMENT "Copying compile_commands.json to source root"
)
