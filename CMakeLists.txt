cmake_minimum_required(VERSION 3.27)
project(DualRasterizer LANGUAGES CXX)

# Profiler
include(FetchContent)
FetchContent_Declare(
    Profiler
    GIT_REPOSITORY https://github.com/MauroDeryckere/Profiler.git
    GIT_TAG main
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(Profiler)

add_subdirectory(project)
