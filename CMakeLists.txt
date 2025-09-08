cmake_minimum_required(VERSION 3.15)
project(cortex LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(cortex
    src/main.cpp
    src/core/Environment.cpp
    src/shell/Parser.cpp
    src/shell/Shell.cpp
    src/shell/CommandRegistry.cpp
    src/vfs/FolderVfs.cpp
    src/commands/Cd.cpp
    src/commands/Pwd.cpp
    src/commands/Ls.cpp
    src/commands/Echo.cpp
    src/commands/Mkdir.cpp
    src/commands/Touch.cpp
    src/commands/Cat.cpp
    src/commands/Rm.cpp
    src/commands/Cp.cpp
    src/commands/Mv.cpp
    src/commands/EnvCmd.cpp
    src/commands/SetCmd.cpp
    src/commands/UnsetCmd.cpp
    src/commands/Help.cpp
    src/commands/Version.cpp
    src/commands/Stat.cpp
    src/commands/Clear.cpp
    src/commands/Head.cpp
    src/commands/Tail.cpp
    src/commands/Find.cpp
    src/commands/Grep.cpp
    src/commands/Pack.cpp
    src/commands/Unpack.cpp
)

target_include_directories(cortex PRIVATE src)

if(MSVC)
  target_compile_options(cortex PRIVATE /W4)
else()
  target_compile_options(cortex PRIVATE -Wall -Wextra -Wpedantic)
endif()
