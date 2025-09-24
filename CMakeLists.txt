cmake_minimum_required(VERSION 3.16)
project(monolock LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra -pedantic)

find_package(PkgConfig REQUIRED)
pkg_check_modules(DEPS REQUIRED
    x11
    xft
    xinerama
    freetype2
    pam
    fontconfig
)

file(GLOB SOURCES CONFIGURE_DEPENDS "src/*.cpp")

add_executable(monolock ${SOURCES})

target_include_directories(monolock PRIVATE
    src
    ${DEPS_INCLUDE_DIRS}
)

target_link_libraries(monolock PRIVATE
    ${DEPS_LIBRARIES}
)

install(TARGETS monolock DESTINATION /usr/local/bin)
