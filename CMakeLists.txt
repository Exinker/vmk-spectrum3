cmake_minimum_required(VERSION 3.25)

project(ethernet_spectrometer)
set(CMAKE_CXX_STANDARD 20)

add_subdirectory(thirdparty/pybind11)

IF (WIN32)
    # GDI defines ERROR
    # winapi sucks
    add_compile_definitions(NOGDI)
    add_compile_definitions(__WIN32__)
    set(WIN32_WINNT 0x0601)
ELSE()
    # set stuff for other systems
ENDIF()

add_subdirectory(lib)
add_subdirectory(example/cpp)
add_subdirectory(binding)


