#pragma once

#include <functional>
#include <cstdint>
#include <map>
#include <string>
#include "utils/DriverException.h"
#include "MasiEthernet.h"
#include "filters/PipeFilter.h"

namespace hwlib {
    struct DeviceError {
        DriverException exception;
    };

    enum class AssemblyStatus {
        ALIVE,
        DISCONNECTED,
        BYSY
    };

    typedef std::function<void(std::map<std::string, AssemblyStatus>)> StatusCallback;
    typedef std::function<void(std::pair<std::string, AssemblyStatus>)> AssemblyStatusCallback;
    typedef std::function<void(DriverException)> ErrorCallback;

    typedef std::shared_ptr<PipeFilter> PipeFilterPtr;

    struct ISyncRun {
        virtual void syncPrepare() = 0;

        virtual void syncRun() = 0;
    };

    struct IAssembly {

        virtual ~IAssembly() {};

        virtual void setErrorCallback(const ErrorCallback &callback) = 0;

        virtual void setAssemblyStatusCallback(const AssemblyStatusCallback &callback) = 0;

        virtual void setContextCallback(const ContextCallback &callback) = 0;

        virtual void connect() = 0;

        virtual void disconnect() = 0;

        virtual void setExposure(uint64_t micros) = 0;

        virtual void setDoubleExposure(uint32_t t1, uint16_t r1, uint32_t t2, uint16_t r2) = 0;

        virtual void setMeasurement(const Measurement &config) = 0;

        virtual void setPipeFilter(const PipeFilterPtr &filter) = 0;

        virtual void writeCr(bool o0, bool o1, bool o2) = 0;

        virtual void read() = 0;

        virtual void cancelReading() = 0;

        virtual AssemblyStatus getAssemblyStatus() = 0;

        virtual Exposure getExposure() = 0;

        virtual std::shared_ptr<AssemblyContext> getContext() = 0;

        virtual AssemblyParams getAssemblyParams() = 0;
    };
}
