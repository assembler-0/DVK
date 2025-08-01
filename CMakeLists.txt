cmake_minimum_required(VERSION 3.30)
project(DVK LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
add_executable(dvk dvk.cpp
        src/ProjectCreator.cpp
        src/AutoInstaller.cpp
        src/ProjectCloner.cpp

)
target_include_directories(dvk PUBLIC "include")
target_link_libraries(dvk PUBLIC fmt)