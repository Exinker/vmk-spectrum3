add_library(aixlog INTERFACE)
target_include_directories(aixlog INTERFACE "../../thirdparty/aixlog/include")
set(CMAKE_EXE_LINKER_FLAGS "-static")

add_executable(example src/example.cpp)
target_link_libraries(example aixlog spectrometer)

if(MINGW)
    target_link_libraries(example ws2_32)
endif()