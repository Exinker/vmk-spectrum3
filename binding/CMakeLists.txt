set(PYMODULE_NAME pyspectrum3)

pybind11_add_module(${PYMODULE_NAME} src/pymodule.cpp)
target_link_libraries(${PYMODULE_NAME} PRIVATE spectrometer)
target_compile_definitions(${PYMODULE_NAME} PRIVATE
        PYMODULE_NAME=${PYMODULE_NAME})