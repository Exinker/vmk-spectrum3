add_library(asio INTERFACE)
target_compile_definitions(asio INTERFACE ASIO_STANDALONE)
target_include_directories(asio INTERFACE "../thirdparty/asio/asio/include")
#target_link_libraries(asio INTERFACE pthread)


add_library(threadpool INTERFACE)
target_include_directories(threadpool INTERFACE "../thirdparty/thread-pool/include")

IF (WIN32)
    add_library(tinyxml STATIC "../thirdparty/tinyxml2/tinyxml2.cpp")
ELSE()
    add_library(tinyxml SHARED "../thirdparty/tinyxml2/tinyxml2.cpp")
ENDIF()
target_include_directories(tinyxml INTERFACE "../thirdparty/tinyxml2")


add_library(spectrometer STATIC
        src/assembly/MasiEthernet.cpp
        src/utils/DriverConfig.cpp
        src/utils/Log.cpp
)

target_include_directories(spectrometer PUBLIC include)
target_link_libraries(spectrometer PUBLIC asio tinyxml threadpool)
set_property(TARGET spectrometer PROPERTY POSITION_INDEPENDENT_CODE 1)
