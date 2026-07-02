#pragma once

#include "filters/CoreFilter.h"

namespace hwlib {
    class DarkSignalFilter : public CoreFilter {
    public:
        explicit DarkSignalFilter(const std::vector<AssemblyContext>& darkSignal)
            : darkSignal(darkSignal) {}


        CoreFilterPtr copy() override {
            return std::make_shared<DarkSignalFilter>(darkSignal);
        }

        void init(AssemblyContext &cfg) override {
            if (darkSignal.empty()) {
                darkSignalIdx = -1;
                return;
            }
            for (int i = 0; i < darkSignal.size(); i++) {
                if (cfg.assemblyParams.id == darkSignal[i].assemblyParams.id) {
                    darkSignalIdx = i;
                    return;
                }
            }
            for (int i = 0; i < darkSignal.size(); i++) {
                if (darkSignal[i].assemblyParams.id.empty()) {
                    darkSignalIdx = i;
                    return;
                }
            }
        }

        void deInit() override {

        }

        void process(AssemblyContext &context) override {
            if (darkSignalIdx == -1) {
                return;
            }
            auto darkSignalFrame = darkSignal[darkSignalIdx].result;
            for (size_t i = 0; i < context.floatFrame->size(); i += 1) {
                context.floatFrame->at(i) = context.floatFrame->at(i) - darkSignalFrame->at(i);
            }
        }

    private:
        std::vector<AssemblyContext> darkSignal;
        int darkSignalIdx = -1;
    };
}
