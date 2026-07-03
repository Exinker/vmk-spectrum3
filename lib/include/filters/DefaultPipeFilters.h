#pragma once

#include "PipeFilter.h"
#include "SwapFilter.h"
#include "NotifyFilter.h"
#include "CopyNotifyFilter.h"
#include "IntegratorFilter.h"
#include "DarkSignalFilter.h"

namespace hwlib {
    class DefaultPipeFilter {
    public:

        static std::shared_ptr<PipeFilter> instance(int integrCount = 1, const std::vector<AssemblyContext>& darkSignal = {}) {
            return std::make_shared<PipeFilter>(std::vector<CoreFilterPtr>{std::make_shared<SwapFilter>(),
                std::make_shared<DarkSignalFilter>(darkSignal), std::make_shared<IntegratorFilter>(integrCount), std::make_shared<NotifyFilter>(integrCount)});

        }
    };

    class DefaultCopyPipeFilter {
    public:

        static std::shared_ptr<PipeFilter> instance(int integrCount = 1, const std::vector<AssemblyContext>& darkSignal = {}) {
            return std::make_shared<PipeFilter>(std::vector<CoreFilterPtr>{std::make_shared<SwapFilter>(),
                std::make_shared<DarkSignalFilter>(darkSignal), std::make_shared<IntegratorFilter>(integrCount), std::make_shared<CopyNotifyFilter>(integrCount)});
        }
    };
}
