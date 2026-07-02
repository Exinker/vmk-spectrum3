#pragma once

#include <utility>

#include "BaseFilter.h"
#include "CoreFilter.h"

namespace hwlib {
    struct AssemblyContext;

    class PipeFilter : public BaseFilter {
    public:

        explicit PipeFilter(const std::vector<CoreFilterPtr> &filters) : filters(filters) {}


        [[nodiscard]] bool isInitialised() const {
            return initialized;
        }


        std::shared_ptr<PipeFilter> copy() {
            std::vector<CoreFilterPtr> filtersCopy;
            for (auto &f: filters) {
                filtersCopy.push_back(f->copy());
            }
            return std::make_shared<PipeFilter>(filtersCopy);
        }

        void init(AssemblyContext &cfg) override {
            for (auto &f: filters) {
                f->init(cfg);
            }
            initialized = true;
        }

        void process(AssemblyContext &context) override {
            for (auto &f: filters) {
                f->process(context);
            }
        }

        std::vector<CoreFilterPtr> &getFilters() {
            return filters;
        }

        void deInit() override {
            for (auto &f: filters) {
                f->deInit();
            }
            initialized = false;
        }

    private:
        std::vector<CoreFilterPtr> filters;
        bool initialized = false;
    };
}
