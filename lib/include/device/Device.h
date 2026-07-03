#pragma once

#include <utility>
#include <chrono>
#include <BS_thread_pool.hpp>

#include "assembly/Assembly.h"
#include "filters/SwapFilter.h"
#include "filters/SyncReadFilter.h"
#include "filters/IntegratorFilter.h"
#include "filters/CopyNotifyFilter.h"
#include "utils/ConfigFileParser.h"
#include "filters/DefaultPipeFilters.h"

namespace hwlib {
    class Device {
    public:

        void initialize(const std::vector<std::shared_ptr<IAssembly>> &assemblies) {
            this->assemblies = assemblies;
        }

        void setErrorCallback(const ErrorCallback &callback) {
            for (auto &a: assemblies) {
                a->setErrorCallback(callback);
            }
        }

        void setAssemblyStatusCallback(const AssemblyStatusCallback &callback) {
            for (auto &a: assemblies) {
                a->setAssemblyStatusCallback(callback);
            }
        }

        void setContextCallback(const ContextCallback &callback) {
            for (auto &a: assemblies) {
                a->setContextCallback(callback);
            }
        }

        void connect() {
            if (assemblies.empty()) {
                throw DriverException(ExceptionProducer::DEVICE, ExceptionType::CONFIG_EXCEPTION, "No connected assemblies");
            }
            std::vector<std::future<void>> futures;
            for (auto &a: assemblies) {
                futures.push_back(pool.submit_task([&] { a->connect(); }));
            }
            for (auto &f: futures) {
                f.get();
            }
        }

        void disconnect() {
            std::vector<std::future<void>> futures;
            for (auto &a: assemblies) {
                futures.push_back(pool.submit_task([&] { a->disconnect(); }));
            }
            for (auto &f: futures) {
                f.get();
            }
        }

        void setMeasurement(Measurement config) {
            std::vector<std::future<void>> futures;
            for (auto &a: assemblies) {
                futures.push_back(pool.submit_task([&] { a->setMeasurement(config); }));
            }
            for (auto &f: futures) {
                f.get();
            }
        }

        void setPipeFilter(const PipeFilterPtr& filter) {
            for (auto &a: assemblies) {
                a->setPipeFilter(filter->copy());
            }
        }

        void writeCr(bool o0, bool o1, bool o2) {
            std::vector<std::future<void>> futures;
            for (auto &a: assemblies) {
                futures.push_back(pool.submit_task([&] { a->writeCr(o0, o1, o2); }));
            }
            for (auto &f: futures) {
                f.get();
            }
        }

        std::vector<AssemblyParams> getDeviceParams() {
            std::vector<std::future<AssemblyParams>> futures;
            std::vector<AssemblyParams> infos;
            for (auto &a: assemblies) {
                futures.push_back(pool.submit_task([&] { return a->getAssemblyParams(); }));
            }
            for (auto &f: futures) {
                infos.push_back(f.get());
            }
            return infos;
        }

        //Возвращает экспозицию с первой сборки
        Exposure getExposure() const {
            return assemblies.at(0)->getExposure();
        }

        void cancelReading() {
            for (auto &a: assemblies) {
                a->cancelReading();
            }
        }

        std::vector<std::shared_ptr<AssemblyContext>> getAssemblyContexts() {
            std::vector<std::shared_ptr<AssemblyContext>> contexts;
            for (auto &a: assemblies) {
                contexts.push_back(a->getContext());
            }
            return contexts;
        }

        std::vector<AssemblyContext> syncRead(uint64_t saveNumber) {
            volatile bool error = false;
            DriverException exception;
            setErrorCallback([&](auto ex) {
                error = true;
                exception = ex;
            });

            std::vector<std::vector<AssemblyContext>> buffers;
            for (auto &a: assemblies) {
                buffers.emplace_back();
                CoreFilterPtr syncFilter = std::make_shared<SyncReadFilter>(buffers.at(buffers.size() - 1), saveNumber);
                a->getContext()->pipeFilter->getFilters().push_back(syncFilter);
            }

            read();

            bool reading = true;
            while (reading) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                if (error) {
                    throw exception;
                }
                auto contexts = getAssemblyContexts();
                reading = false;
                for (auto &c : contexts) {
                    if (c->measurement.readFramesNum != c->frameState.frameNumber) {
                        reading = true;
                    }
                }
            }

            for (auto &a: assemblies) {
                a->getContext()->pipeFilter->getFilters().pop_back();
            }

            std::vector<AssemblyContext> plainBuffer;
            for(auto && buf : buffers) {
                plainBuffer.insert(plainBuffer.end(), buf.begin(), buf.end());
            }

            return plainBuffer;
        }

        void read() {
            for (auto &a: assemblies) {
                a->read();
            }
        }

        std::map<std::string, AssemblyStatus> getAssemblyStatuses() {
            return assemblyStatuses;
        }

        void setAssemblyStatuses(const std::map<std::string, AssemblyStatus> &statuses) {
            assemblyStatuses = statuses;
        }

        std::map<std::string, AssemblyStatus> readAssemblyStatuses() {
            std::map<std::string, std::future<AssemblyStatus>> futures;
            std::map<std::string, AssemblyStatus> statuses;
            for (auto &a: assemblies) {
                futures.emplace(a->getContext()->assemblyParams.id, pool.submit_task([&] { return a->getAssemblyStatus(); }));
            }
            for (auto &[addr, f]: futures) {
                statuses.insert({addr, f.get()});
            }
            assemblyStatuses = statuses;
            return statuses;
        }

        void readBroadcast() {
            for (auto it = ++assemblies.begin(); it != assemblies.end(); it++) {
                auto *a = dynamic_cast<ISyncRun *>(it->get());
                if (a == nullptr) {
                    throw DriverException(ExceptionProducer::DEVICE, ExceptionType::INVALID_ARGUMENT, "This assembly does not support synchronous reading");
                }
                a->syncPrepare();
            }

            auto *a = dynamic_cast<ISyncRun *>(assemblies.begin()->get());
            if (a == nullptr) {
                throw DriverException(ExceptionProducer::DEVICE, ExceptionType::INVALID_ARGUMENT, "This assembly does not support synchronous reading");
            }
            a->syncRun();
        }


    private:
        std::vector<std::shared_ptr<IAssembly>> assemblies;
        std::map<std::string, AssemblyStatus> assemblyStatuses;
        BS::thread_pool<> pool;
    };
}
