#pragma once

#include <mutex>
#include <condition_variable>
#include "DriverException.h"
#include "Log.h"

namespace hwlib {
    class FrameBuffer {
    public:

        FrameBuffer(int32_t frameSize, int32_t bufferSize, std::chrono::milliseconds timeout) : frameSize(frameSize),
                                                                                                bufferSize(bufferSize),
                                                                                                timeout(timeout) {
            for (int32_t i = 0; i < bufferSize; i++) {
                buffer.push_back(std::make_shared<std::vector<uint16_t>>(frameSize, 0));
            }
        }

        RawFrame getWritePtr() {
            std::unique_lock lock(mutex);
            if (full) {
                throw DriverException(ExceptionProducer::ASSEMBLY, ExceptionType::BUFFER_OVERFLOW, "Read buffer owerflow. Please increase READ_BUFFER");
            }
            return buffer[writeIndex];
        }

        void finishWrite() {
            {
                std::unique_lock lock(mutex);
                writeIndex = (writeIndex + 1) % bufferSize;
                if (writeIndex == readIndex) {
                    full = true;
                }
            }
            condition.notify_one();
        }

        RawFrame getReadPtr() {
            std::unique_lock lock(mutex);
            if (!condition.wait_for(lock, timeout, [=, this] { return writeIndex != readIndex || full; })) {
                throw DriverException(ExceptionProducer::ASSEMBLY, ExceptionType::NETWORK_EXCEPTION, "Timeout");
            }
            return buffer[readIndex];
        }

        void finishRead() {
            {
                std::unique_lock lock(mutex);
                readIndex = (readIndex + 1) % bufferSize;
                full = false;
            }
            condition.notify_one();
        }

    private:
        std::mutex mutex;
        std::condition_variable condition;
        std::vector<RawFrame> buffer;
        uint32_t frameSize;
        uint32_t bufferSize;
        std::chrono::milliseconds timeout;
        volatile uint32_t writeIndex = 0;
        volatile uint32_t readIndex = 0;
        bool full = false;
    };
}
