#pragma once

#include "filters/CoreFilter.h"

namespace hwlib {
    class IntegratorFilter : public CoreFilter {
    public:
        explicit IntegratorFilter(uint64_t integrCount) : integrCount(integrCount) {}


        CoreFilterPtr copy() override {
            return std::make_shared<IntegratorFilter>(integrCount);
        }

        void init(AssemblyContext &cfg) override {

        }

        void deInit() override {

        }

        void process(AssemblyContext &context) override {
            if (integrCount == 1) {
                for (size_t i = 0; i < context.floatFrame->size(); i += 1) {
                    context.result->at(i) = context.floatFrame->at(i);
                }
                return;
            }
            if (context.frameState.frameNumber % integrCount == 1) {
                for (size_t i = 0; i < context.result->size(); i += 1) {
                    context.result->at(i) = 0;
                }
            }
            for (size_t i = 0; i < context.floatFrame->size(); i += 1) {
                context.result->at(i) += context.floatFrame->at(i);
            }
            if (context.frameState.frameNumber % integrCount == 0) {
                for (size_t i = 0; i < context.result->size(); i += 1) {
                    context.result->at(i) /= static_cast<double>(integrCount);
                }
            }
        }

    private:
        uint64_t integrCount = 1;
    };
}
