#pragma once

#include <utility>
#include <iostream>

#include "MasiEthernet.h"
#include "utils/BlockingQueue.h"
#include "IAssembly.h"
#include "filters/PipeFilter.h"
#include "filters/ContextHolder.h"
#include "utils/FrameBuffer.h"
#include "utils/Log.h"

namespace hwlib {
    enum class AssemblyReaderCommand {
        READ,
        SYNC_RUN,
        SYNC_PREPARE,
        STOP
    };

    class Assembly : public IAssembly, public ISyncRun {
    public:
        explicit Assembly(const std::string &addr) : addr(addr), masi(addr) {}

        ~Assembly() override { Assembly::disconnect(); }

        void setErrorCallback(const ErrorCallback &callback) override {
            errorCallback = callback;
        }

        void setAssemblyStatusCallback(const AssemblyStatusCallback &callback) override {
            assemblyStatusCallback = callback;
        }

        void setContextCallback(const ContextCallback &callback) override {
            contextHolder.getContext()->notify = callback;
        }

        void connect() override {
            if (isRunning) {
                return;
            }
            std::unique_lock<std::mutex> lock(mutex);
            if (wasRunning) {
                throw DriverException(ExceptionProducer::ASSEMBLY, ExceptionType::INVALID_ARGUMENT,
                                      "ASSEMBLY: " + addr + " Assembly cannot be started twice");
            }
            masi.commandConnect();
            isRunning = true;
            wasRunning = true;
            assemblyStatusCallback({addr, AssemblyStatus::ALIVE});
            filterThread = std::thread(&Assembly::reader, this);
            AssemblyParams cfg = getAssemblyParams();
            frameBuffer = std::make_shared<FrameBuffer>(cfg.assemblyInfo.numChips * cfg.assemblyInfo.pixelsPerChip,
                                                        DriverConfig::readBufferSize,
                                                        std::chrono::milliseconds(DriverConfig::timeout)
            );
            readerThread = std::thread(&Assembly::readFramesThread, this, cfg.assemblyInfo);
            contextHolder.initContext(cfg);
        }

        void disconnect() override {
            if (!isRunning) {
                return;
            }
            isRunning = false;
            filterMessageQueue.push({AssemblyReaderCommand::STOP, 0});
            filterThread.join();
            readerThread.join();
            std::unique_lock<std::mutex> lock(mutex);
            masi.disconnect();
            contextHolder.deInitFilters();
            assemblyStatusCallback({addr, AssemblyStatus::DISCONNECTED});
        }

        void setExposure(uint64_t micros) override {
            std::unique_lock<std::mutex> lock(mutex);
            masi.setExposure(micros);
        }

        void setDoubleExposure(uint32_t t1, uint16_t r1, uint32_t t2, uint16_t r2) override {
            std::unique_lock<std::mutex> lock(mutex);
            masi.setDoubleExposure(t2, r2, t1, r1); //костыль для совместимости c Атомом
        }

        void setMeasurement(const Measurement &config) override {
            Exposure exposure = config.exposure;
            if (!exposure.isDouble) {
                masi.setExposure(exposure.single);
            } else {
                masi.setDoubleExposure(exposure.dbl.t2, exposure.dbl.r2, exposure.dbl.t1,
                                       exposure.dbl.r1); //костыль для совместимости c Атомом
            }
            contextHolder.getContext()->measurement = config;
        }

        void setPipeFilter(const PipeFilterPtr &filter) override {
            contextHolder.getContext()->pipeFilter = filter;
        }

        void writeCr(bool o0, bool o1, bool o2) override {
            std::unique_lock<std::mutex> lock(mutex);
            masi.writeControlRegister((o0 << 0) | (o1 << 1) | (o2 << 2));
        }

        void read() override {
            filterMessageQueue.push(
                    {AssemblyReaderCommand::READ, contextHolder.getContext()->measurement.readFramesNum +
                                                           contextHolder.getContext()->measurement.skipFramesNum});
        }

        // after syncPrepare of other assemblies
        void syncRun() override {
            filterMessageQueue.push({AssemblyReaderCommand::SYNC_RUN,
                                     contextHolder.getContext()->measurement.readFramesNum +
                                     contextHolder.getContext()->measurement.skipFramesNum});
        }

        void syncPrepare() override {
            filterMessageQueue.push({AssemblyReaderCommand::SYNC_PREPARE,
                                     contextHolder.getContext()->measurement.readFramesNum +
                                     contextHolder.getContext()->measurement.skipFramesNum});
        }

        // can only be called mid-frame
        void cancelReading() override {
            filterMessageQueue.push({AssemblyReaderCommand::STOP, 0});
            readerMessageQueue.push({AssemblyReaderCommand::STOP, 0});
        }

        AssemblyStatus getAssemblyStatus() override {
            std::unique_lock<std::mutex> lock(mutex);
            return masi.getStatus();
        }

        Exposure getExposure() override {
            std::unique_lock<std::mutex> lock(mutex);
            return masi.readExposure().getExposure();
        }

        std::shared_ptr<AssemblyContext> getContext() override {
            return contextHolder.getContext();
        }

        AssemblyParams getAssemblyParams() override {
            AssemblyInfo info = masi.readAssemblyInfo();
            SwapFile swap = masi.readSwapFile();
            std::string serialNum = masi.readSerialNumber();
            return AssemblyParams(AssemblyId(addr), info, swap, serialNum);
        }

    private:
        std::string addr;
        MasiEthernet masi;
        ContextHolder contextHolder;
        std::mutex mutex;
        BlockingQueue<Message<AssemblyReaderCommand>> filterMessageQueue;
        BlockingQueue<Message<AssemblyReaderCommand>> readerMessageQueue;
        AssemblyStatusCallback assemblyStatusCallback = [](auto _) {};
        ErrorCallback errorCallback = [](auto _) {};
        std::shared_ptr<FrameBuffer> frameBuffer;

        std::thread filterThread;
        std::thread readerThread;

        volatile bool isRunning = false;
        volatile bool wasRunning = false;

        static void setHighThreadPriority() {
#ifdef WIN32
            if (!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL)) {
                Log::instance("Can not set thread priority\n");
            }
#else
            sched_param sch;
            sch.sched_priority = 127;
            if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &sch)) {
                Log::instance("Can not set thread priority\n");
            }
#endif
        }

        void reader() {
            //setHighThreadPriority(); TODO: Протестить на линуксе
            while (isRunning) {
                Message msg = filterMessageQueue.pop();
                try {
                    switch (msg.cmd) {
                        case AssemblyReaderCommand::READ: {
                            std::unique_lock<std::mutex> lock(mutex);
                            masi.startReadingUnicast(msg.payload);
                            assemblyStatusCallback({addr, AssemblyStatus::BYSY});
                            readFrames(msg.payload);
                            Log::instance("END SYNC READ\n");
                            break;
                        }
                        case AssemblyReaderCommand::SYNC_RUN: {
                            std::unique_lock<std::mutex> lock(mutex);
                            masi.startReadingBroadcast(msg.payload);
                            assemblyStatusCallback({addr, AssemblyStatus::BYSY});
                            readFrames(msg.payload);
                            break;
                        }
                        case AssemblyReaderCommand::SYNC_PREPARE: {
                            std::unique_lock<std::mutex> lock(mutex);
                            masi.initBroadcastReading(msg.payload);
                            assemblyStatusCallback({addr, AssemblyStatus::BYSY});
                            readFrames(msg.payload);
                            break;
                        }
                        case AssemblyReaderCommand::STOP: {
                            break;
                        }
                    }
                } catch (DriverException &e) {
                    for (auto &m: e.getMessages()) {
                        Log::instance(m + "\n");
                    }
                    errorCallback(e);
                } catch (std::exception &e) {
                    Log::instance(std::string(e.what()) + "\n");
                    auto ex = DriverException(ExceptionProducer::ASSEMBLY, ExceptionType::UNKNOWN_EXCEPTION, e.what());
                    errorCallback(ex);
                }
            }
        }

        void readFrames(uint64_t nTimes) {
            AssemblyParams cfg = getAssemblyParams();
            contextHolder.initFilters();
            uint64_t skipFramesNum = contextHolder.getContext()->measurement.skipFramesNum;
            for (int i = 0; i < nTimes; i++) {
                if (!filterMessageQueue.isEmpty()) {
                    auto innerCmd = filterMessageQueue.peak();
                    if (innerCmd.cmd == AssemblyReaderCommand::STOP) {
                        filterMessageQueue.pop();
                        return;
                    }
                }

                auto context = contextHolder.getContext();
                context->frameState.frameNumber = i + 1;
                context->rawFrame = frameBuffer->getReadPtr();
                if (i < skipFramesNum) {
                    continue;
                }
                std::transform(context->rawFrame->begin(), context->rawFrame->end(), context->floatFrame->begin(),
                    [](const int x) { return static_cast<double>(x);});
                context->pipeFilter->process(*context);
                frameBuffer->finishRead();
            }
        }

        void readFramesThread(AssemblyInfo info) {
            masi.frameConnect();
            auto fr = frameBuffer->getWritePtr();
            while (isRunning) {
                try {
                    if (!readerMessageQueue.isEmpty()) {
                        auto innerCmd = readerMessageQueue.peak();
                        if (innerCmd.cmd == AssemblyReaderCommand::STOP) {
                            readerMessageQueue.pop();
                            masi.stopReading();
                            continue;
                        }
                    }
                    if (masi.readFrame(info, reinterpret_cast<uint8_t *>(fr->data()))) {
                        frameBuffer->finishWrite();
                        fr = frameBuffer->getWritePtr();
                    }
                } catch (DriverException &e) {
                    for (auto &m: e.getMessages()) {
                        Log::instance(m + "\n");
                    }
                    errorCallback(e);
                } catch (std::exception &e) {
                    Log::instance(std::string(e.what()) + "\n");
                    auto ex = DriverException(ExceptionProducer::ASSEMBLY, ExceptionType::UNKNOWN_EXCEPTION, e.what());
                    errorCallback(ex);
                }
            }
        }
    };
}
