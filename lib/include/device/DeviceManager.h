#pragma once

#include <utility>

#include "device/Device.h"
#include "utils/ConfigFileParser.h"
#include "assembly/AssembliesFactory.h"
#include "AutoConfig.h"
#include "utils/DriverConfig.h"
#include "utils/Log.h"

namespace hwlib {
    class DeviceManager {
    public:

        void initialize(const DeviceConfigFile &devCfg) {
            if (isRunning) {
                disconnect();
            }
            device.initialize(AssembliesFactory::createAssemblies(devCfg));
            device.setErrorCallback([&](auto e) { errorCallback(e); });
            device.setAssemblyStatusCallback([&](auto p) {
                std::unique_lock<std::mutex> lock(statusMutex);
                auto statuses = device.getAssemblyStatuses();
                statuses[p.first] = p.second;
                device.setAssemblyStatuses(statuses);
                statusCallback(statuses);
            });
            schedulerThread = std::thread(&DeviceManager::checkAssembliesStatusLoop, this);
            isRunning = true;
        }

        virtual ~DeviceManager() {
            DeviceManager::disconnect();
        }

        void setContextCallback(const ContextCallback &callback) {
            device.setContextCallback(callback);
        }

        void setStatusCallback(const StatusCallback &callback) {
            statusCallback = callback;
        }

        void setErrorCallback(const ErrorCallback &callback) {
            errorCallback = callback;
        }

        void setLogCallback(const LogCallback &callback) {
            Log::instance = callback;
        }

        void connect() {
            Log::instance("CMD CONNECT\n");
            device.connect();
        }

        void disconnect() {
            if (!isRunning) {
                return;
            }
            Log::instance("CMD DISCONNECT\n");
            isRunning = false;
            device.cancelReading();
            schedulerThread.join();
            device.disconnect();
        }

        void setMeasurement(const Measurement &measurement) {
            Log::instance("CMD SET MEASUREMENT\n");
            device.setMeasurement(measurement);
        }

        void setPipeFilter(const PipeFilterPtr &filter) {
            Log::instance("CMD SET PIPE FILTER\n");
            device.setPipeFilter(filter);
        }

        void writeCr(bool o0, bool o1, bool o2) {
            Log::instance("CMD WRITE CR\n");
            device.writeCr(o0, o1, o2);
        }

        std::vector<AssemblyParams> getDeviceParams() {
            Log::instance("CMD GET DEVICE CONFIG\n");
            return device.getDeviceParams();
        }

        Exposure getExposure() const {
            return device.getExposure();
        }

        void read() {
            Log::instance("CMD READ\n");
            device.read();
        }

        /**
         * Синхронная функция чтения
         * @param saveNumber какие по счету обработанные кадры должны быть сохранены (frameNum % saveNumber == 0)
         * @return сохраненные кадры
         */
        std::vector<AssemblyContext> syncRead(uint64_t saveNumber = 1) {
            Log::instance("CMD SYNC READ\n");
            return device.syncRead(saveNumber);
        }

        void readBroadcast() {
            Log::instance("CMD READ BROADCAST\n");
            device.readBroadcast();
        }

        void cancelReading() {
            Log::instance("CMD CANCEL READING\n");
            device.cancelReading();
        }

    private:
        Device device;
        StatusCallback statusCallback = [](auto _) {};
        ErrorCallback errorCallback = [](auto _) {};
        std::mutex statusMutex;
        std::condition_variable statusCondition;
        std::thread schedulerThread;

        volatile bool isRunning = false;

        void checkAssembliesStatusLoop() {
            while (isRunning) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                auto oldStatuses = device.getAssemblyStatuses();
                auto newStatuses = device.readAssemblyStatuses();
                if (oldStatuses != newStatuses) {
                    statusCallback(newStatuses);
                }
            }
        }
    };
}

