#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "assembly/MasiEthernet.h"
#include "assembly/Assembly.h"
#include "device/DeviceManager.h"
#include <pybind11/numpy.h>
#include <pybind11/functional.h>
#include <pybind11/embed.h>

#include <utility>

namespace py = pybind11;
using namespace hwlib;

template<class T>
py::array_t<T> make_array(T *dataPtr, int length) {
    auto arr = py::array_t<T>(
            {length},
            {sizeof(T)},
            dataPtr
    );
    return arr;
}

PYBIND11_MODULE(PYMODULE_NAME, m){



    py::class_<DeviceManager>(m, "DeviceManager")
            .def(py::init<>())
            .def("initialize", &DeviceManager::initialize, py::call_guard<py::gil_scoped_release>())
            .def("connect", &DeviceManager::connect, py::call_guard<py::gil_scoped_release>())
            .def("disconnect", [](DeviceManager &self){
                // This is done because stop joins thread
                // Which calls python callback
                pybind11::gil_scoped_release gil_release;
                self.disconnect();
            })
            .def("set_measurement", &DeviceManager::setMeasurement, py::call_guard<py::gil_scoped_release>())
            .def("write_cr", &DeviceManager::writeCr, py::call_guard<py::gil_scoped_release>())
            .def("read", &DeviceManager::read, py::call_guard<py::gil_scoped_release>())
            .def("read_broadcast", &DeviceManager::readBroadcast, py::call_guard<py::gil_scoped_release>())
            .def("cancel_reading", &DeviceManager::cancelReading, py::call_guard<py::gil_scoped_release>())
            .def("get_device_config", &DeviceManager::getDeviceParams, py::call_guard<py::gil_scoped_release>())
            .def("set_measurement", &DeviceManager::setMeasurement, py::call_guard<py::gil_scoped_release>())
            .def("set_pipe_filter", &DeviceManager::setPipeFilter, py::call_guard<py::gil_scoped_release>())


            .def("set_error_callback", [](DeviceManager &self, const ErrorCallback& cb){
                self.setErrorCallback([=](DriverException e){
                    // We must acquire GIL before calling python code
                    py::gil_scoped_acquire acq;
                    cb(std::move(e));
                });

            })

            .def("set_log_callback", [](DeviceManager &self, const LogCallback& cb){
                self.setLogCallback([=](std::string msg){
                    // We must acquire GIL before calling python code
                    py::gil_scoped_acquire acq;
                    cb(std::move(msg));
                });
            })

            .def("set_status_callback", [](DeviceManager &self, const StatusCallback& cb){
                self.setStatusCallback([=](std::map<std::string, AssemblyStatus> statuses){
                    // We must acquire GIL before calling python code
                    py::gil_scoped_acquire acq;
                    cb(std::move(statuses));
                });

            })

            .def("set_context_callback", [](DeviceManager &self, const ContextCallback& pyCallback){
                self.setContextCallback([=](AssemblyContext& context) {
                    // We must acquire GIL before calling python code
                    py::gil_scoped_acquire acq;
                    pyCallback(context);
                });
            });

    py::class_<AssemblyContext>(m, "AssemblyContext")
            .def_readonly("frame_state", &AssemblyContext::frameState)
            .def_readonly("assembly_params", &AssemblyContext::assemblyParams)
            .def_property_readonly("result", [](const AssemblyContext& self) { return *self.result; });

    py::class_<FrameState>(m, "FrameState")
            .def_readonly("frame_number", &FrameState::frameNumber);


    py::class_<PipeFilter, std::shared_ptr<PipeFilter>>(m, "PipeFilter");

    py::class_<DefaultCopyPipeFilter>(m, "DefaultCopyPipeFilter")
            .def_static("instance", &DefaultCopyPipeFilter::instance, py::arg("integrCount") = 1, py::arg("darkSignal") = std::vector<AssemblyContext>());

    py::class_<Exposure>(m, "Exposure")
            .def(py::init<uint64_t>())
            .def(py::init<DoubleTimer>())
            .def_readonly("single", &Exposure::single)
            .def_readonly("is_double", &Exposure::isDouble)
            .def_readonly("double", &Exposure::dbl);

    py::class_<DriverConfig>(m, "DriverConfig")
            .def_static("parse", &DriverConfig::parse);

    py::class_<Measurement>(m, "Measurement")
            .def(py::init<Exposure, uint64_t, uint64_t>())
            .def_readonly("exposure", &Measurement::exposure)
            .def_readonly("read_frames_num", &Measurement::readFramesNum)
            .def_readonly("skip_frames_num", &Measurement::skipFramesNum);

    py::class_<AssemblyParams>(m, "AssemblyParams")
            .def_readonly("id", &AssemblyParams::id)
            .def_readonly("assembly_info", &AssemblyParams::assemblyInfo)
            .def_property_readonly("swap_file", [=](AssemblyParams &c) { return make_array(c.swapFile.file, c.swapFile.length); });


    py::class_<ConfigFileParser>(m, "ConfigFileParser")
            .def(py::init<std::string>())
            .def("read_file", &ConfigFileParser::readFile)
            .def("write_file", &ConfigFileParser::writeFile);

    py::class_<AutoConfig>(m, "AutoConfig")
            .def(py::init<>())
            .def("config", &AutoConfig::config);

    py::class_<DeviceConfigFile>(m, "DeviceConfigFile")
            .def(py::init<std::vector<AssemblyConfigFile>>())
            .def_readonly("assemblies", &DeviceConfigFile::assemblies);

    py::class_<AssemblyConfigFile>(m, "AssemblyConfigFile")
            .def(py::init<std::string>())
            .def_readonly("addr", &AssemblyConfigFile::addr);


    py::exec(R"pybind(
class DriverException(RuntimeError):
    def __init__(self, messages: list[str], ep, et):
        self.messages = messages
        self.ep = ep
        self.et = et
        super().__init__(messages[0]))pybind",
             m.attr("__dict__"), m.attr("__dict__"));

    const py::object pyDriverException = m.attr("DriverException");

    py::register_exception_translator([](std::exception_ptr p) {

        const auto setPyException = [](const char* pyTypeName, const auto& e) {
            const py::object pyClass = py::module_::import("pyspectrum3").attr(pyTypeName);
            const py::object pyInstance = pyClass(e.getMessages(), e.getExceptionProducer(), e.getExceptionType());
            PyErr_SetObject(pyClass.ptr(), pyInstance.ptr());
        };

        try {
            if (p) std::rethrow_exception(p);
        } catch (const DriverException &e) {
            setPyException("DriverException", e);
        }
    });

    py::class_<DriverException>(m, "AsyncDriverException")
            .def(py::init<ExceptionProducer, ExceptionType, std::string>())
            .def("add_message", &DriverException::addMessage)
            .def("get_messages", &DriverException::getMessages)
            .def("get_exception_producer", &DriverException::getExceptionProducer)
            .def("get_exception_type", &DriverException::getExceptionType);
    py::enum_<ExceptionProducer>(m, "ExceptionProducer")
            .value("ASSEMBLY", ExceptionProducer::ASSEMBLY)
            .value("DEVICE", ExceptionProducer::DEVICE);
    py::enum_<ExceptionType>(m, "ExceptionType")
            .value("NETWORK_EXCEPTION", ExceptionType::NETWORK_EXCEPTION)
            .value("CONFIG_EXCEPTION", ExceptionType::CONFIG_EXCEPTION)
            .value("INVALID_ARGUMENT", ExceptionType::INVALID_ARGUMENT)
            .value("UNKNOWN_EXCEPTION", ExceptionType::UNKNOWN_EXCEPTION)
            .value("CALLBACK_EXCEPTION", ExceptionType::CALLBACK_EXCEPTION);
    py::class_<AssemblyInfo>(m, "AssemblyInfo")
            .def_readonly("num_chips", &AssemblyInfo::numChips)
            .def_readonly("pixels_per_chip", &AssemblyInfo::pixelsPerChip)
            .def_readonly("chips_type", &AssemblyInfo::chipsType)
            .def_readonly("min_exposure", &AssemblyInfo::minExposure)
            .def_readonly("tr_0", &AssemblyInfo::mTr0)
            .def_readonly("ui_0", &AssemblyInfo::mUi0);
    py::class_<SwapFile>(m, "SwapFile")
            .def_readonly("file", &SwapFile::file)
            .def_readonly("length", &SwapFile::length);
    py::enum_<ChipType>(m, "ChipType")
            .value("BLPP1", ChipType::BLPP1)
            .value("BLPP12", ChipType::BLPP12)
            .value("BLPP369", ChipType::BLPP369)
            .value("MT001", ChipType::MT001)
            .value("BLPP2000", ChipType::BLPP2000)
            .value("BLPP4000", ChipType::BLPP4000);
    py::enum_<AssemblyStatus>(m, "AssemblyStatus")
            .value("ALIVE", AssemblyStatus::ALIVE)
            .value("DISCONNECTED", AssemblyStatus::DISCONNECTED)
            .value("BYSY", AssemblyStatus::BYSY);
}