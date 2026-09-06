cmake_minimum_required(VERSION 3.22)
project(nexo VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4)
pkg_check_modules(WEBKIT REQUIRED IMPORTED_TARGET webkitgtk-6.0)

add_executable(nexo src/main.c)
target_link_libraries(nexo PRIVATE PkgConfig::GTK4 PkgConfig::WEBKIT)
target_compile_options(nexo PRIVATE -Wall -Wextra -Wpedantic -O2)

target_compile_definitions(nexo PRIVATE NEXO_VERSION=\"${PROJECT_VERSION}\")

install(TARGETS nexo RUNTIME DESTINATION bin)
install(FILES packaging/nexo.desktop DESTINATION share/applications)
install(FILES packaging/icons/nexo.svg DESTINATION share/icons/hicolor/scalable/apps)
