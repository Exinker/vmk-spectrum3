#pragma once

#include "BaseFilter.h"

namespace hwlib {
    struct CoreFilter : public BaseFilter {
        virtual CoreFilterPtr copy() = 0;
    };
}