#pragma once

#include "AssemblyContext.h"

namespace hwlib {
    struct BaseFilter {

        virtual ~BaseFilter() {};

        virtual void init(AssemblyContext &context) = 0;

        virtual void process(AssemblyContext &context) = 0;

        virtual void deInit() = 0;
    };
}