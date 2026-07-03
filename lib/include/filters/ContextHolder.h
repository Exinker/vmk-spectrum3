#pragma once

#include <cstdint>
#include <utility>
#include "AssemblyContext.h"

namespace hwlib {
    class ContextHolder {
    public:

        ContextHolder() : assemblyContext(std::make_shared<AssemblyContext>()) {}

        void initContext(const AssemblyParams &cfg) {
            int frameLength = cfg.assemblyInfo.numChips * cfg.assemblyInfo.pixelsPerChip;
            assemblyContext->assemblyParams = cfg;
            assemblyContext->rawFrame = std::make_shared<std::vector<uint16_t>>(frameLength, 0);
            assemblyContext->floatFrame = std::make_shared<std::vector<float>>(frameLength, 0);
                    assemblyContext->result = std::make_shared<std::vector<double>>(frameLength, 0);
        }

        void initFilters() {
            if (!assemblyContext->pipeFilter->isInitialised()) {
                assemblyContext->pipeFilter->init(*assemblyContext);
            }
        }

        void deInitFilters() {
            if (assemblyContext->pipeFilter->isInitialised()) {
                assemblyContext->pipeFilter->deInit();
            }
        }

        std::shared_ptr<AssemblyContext> getContext() {
            return assemblyContext;
        }

    private:
        std::shared_ptr<AssemblyContext> assemblyContext;
    };
}
