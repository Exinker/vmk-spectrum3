#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <functional>
#include <utility>

namespace hwlib {
    struct AssemblyContext;

    class BaseFilter;

    class CoreFilter;

    class PipeFilter;

    using CoreFilterPtr = std::shared_ptr<CoreFilter>;

    typedef std::function<void(AssemblyContext&)> ContextCallback;

    enum class ChipType : uint16_t {
        BLPP1 = 0x0101,
        BLPP12 = 0x0202,
        BLPP369 = 0x0301,
        MT001 = 0x0401,
        BLPP2000 = 0x0501,
        BLPP4000 = 0x0601
    };

    using RawFrame = std::shared_ptr<std::vector<uint16_t>>;

    using  FloatFrame = std::shared_ptr<std::vector<float>>;

    using Result = std::shared_ptr<std::vector<double>>;


    struct DoubleTimer {
        uint32_t t1;
        uint16_t r1;
        uint32_t t2;
        uint16_t r2;
    };

    struct Exposure {

        Exposure() {}

        explicit Exposure(uint64_t single) : single(single), isDouble(false) {}

        explicit Exposure(const DoubleTimer &dbl) : dbl(dbl), isDouble(true) {}

        uint64_t single = 0;
        bool isDouble = false;
        DoubleTimer dbl = {0, 0, 0, 0};

        [[nodiscard]] bool valid() const {
            if (!isDouble) return single != 0;
            return dbl.r1 >= 1 && dbl.r2 >= 1 && dbl.t1 > 0 && dbl.t2 > 0;
        }
    };

    struct TimerState {

        TimerState() : stateTime(std::chrono::system_clock::now()) {}

        explicit TimerState(uint64_t single) : single(single),
                                               isDouble(false),
                                               stateTime(std::chrono::system_clock::now()) {}

        explicit TimerState(const DoubleTimer &dbl) : dbl(dbl),
                                                      isDouble(true),
                                                      stateTime(std::chrono::system_clock::now()) {}

        uint64_t single = 0;
        bool isDouble = false;
        DoubleTimer dbl = {0, 0, 0, 0};
        std::chrono::system_clock::time_point stateTime;

        [[nodiscard]] Exposure getExposure() const {
            if (isDouble) {
                return Exposure(dbl);
            }
            return Exposure(single);
        }

        [[nodiscard]] bool valid() const {
            if (!isDouble) return single != 0;
            return dbl.r1 >= 1 && dbl.r2 >= 1 && dbl.t1 > 0 && dbl.t2 > 0;
        }
    };

    struct FrameState {
        int64_t frameNumber = 0;
    };

    struct AssemblyInfo {
        uint8_t numChips;
        uint16_t pixelsPerChip;
        ChipType chipsType;
        int minExposure; // in microseconds
        float mTr0;
        float mUi0;
        uint32_t numPixels; // Including synchro signal
    };

    struct SwapFile {
        int8_t file[256] = {};
        uint8_t length;
    };

    using AssemblyId = std::string; // ip addr for masi ethernet

    struct AssemblyParams {
        AssemblyId id;
        AssemblyInfo assemblyInfo;
        SwapFile swapFile;
        std::string serialNumber;
    };

    struct Measurement {
        Exposure exposure;
        uint64_t readFramesNum = 0;
        uint64_t skipFramesNum = 0;

        [[nodiscard]] bool valid() const {
            return exposure.valid() && readFramesNum > 0 && skipFramesNum >= 0;
        }
    };

    struct AssemblyContext {

        [[nodiscard]] AssemblyContext deepCopy() const {
            auto raw = std::make_shared<std::vector<uint16_t>>(*rawFrame);
            auto f = std::make_shared<std::vector<float>>(*floatFrame);
            auto res = std::make_shared<std::vector<double>>(*result);
            return AssemblyContext(measurement, pipeFilter, assemblyParams, raw, f, res, frameState, notify);
        }

        Measurement measurement;
        std::shared_ptr<PipeFilter> pipeFilter;
        AssemblyParams assemblyParams;
        RawFrame rawFrame;
        FloatFrame floatFrame;
        Result result;
        FrameState frameState;
        ContextCallback notify = [](auto _) {};
    };
}