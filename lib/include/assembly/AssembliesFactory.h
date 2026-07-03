#pragma once

#include "IAssembly.h"
#include "Assembly.h"
#include "utils/ConfigFileParser.h"
#include "utils/DriverConfig.h"
#include <unordered_map>
#include <stdexcept>

namespace hwlib {
    class AssembliesFactory {
    public:
        static std::vector<std::shared_ptr<IAssembly>> createAssemblies(const DeviceConfigFile &cfg) {
            std::vector<std::shared_ptr<IAssembly>> assemblies;
            for (auto &a: cfg.assemblies) {
                assemblies.push_back(createAssembly(a));
            }
            return assemblies;
        }

        static std::shared_ptr<IAssembly> createAssembly(const AssemblyConfigFile &cfg) {
            if (cfg.type == AssemblyType::MASI_ETHERNET) {
                return std::make_shared<Assembly>(cfg.addr);
            }
            throw std::runtime_error("Not supported type");
        }
    };
}
