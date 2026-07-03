#pragma once

#include "tinyxml2.h"
#include <string>
#include <utility>
#include <vector>
#include "DriverException.h"

namespace hwlib {
    enum AssemblyType {
        MASI_ETHERNET = 0,
    };

    struct AssemblyConfigFile {
        std::string addr;
        AssemblyType type = AssemblyType::MASI_ETHERNET;
    };

    struct DeviceConfigFile {
        std::vector<AssemblyConfigFile> assemblies;
    };


    class ConfigFileParser {
    public:

        explicit ConfigFileParser(const std::string path) : path(path) {}

        DeviceConfigFile readFile() {
            tinyxml2::XMLDocument xmlDoc;
            tinyxml2::XMLError eResult = xmlDoc.LoadFile(path.c_str());
            checkError(eResult);
            tinyxml2::XMLNode *pRoot = xmlDoc.FirstChild();
            if (pRoot == nullptr)
                throw DriverException(ExceptionProducer::DEVICE, ExceptionType::CONFIG_EXCEPTION, "Error reading device configuration");

            return parseDeviceNode(pRoot);
        }

        void writeFile(const DeviceConfigFile &device) {
            tinyxml2::XMLDocument xmlDoc;
            tinyxml2::XMLNode *pRoot = xmlDoc.NewElement("Device");
            xmlDoc.InsertFirstChild(pRoot);

            for (const auto &item: device.assemblies) {
                tinyxml2::XMLElement *pAssembly = xmlDoc.NewElement("Assembly");

                tinyxml2::XMLElement *pAddr = xmlDoc.NewElement("Addr");
                pAddr->SetText(item.addr.c_str());
                pAssembly->InsertEndChild(pAddr);

                tinyxml2::XMLElement *pType = xmlDoc.NewElement("Type");
                pType->SetText(item.type);
                pAssembly->InsertEndChild(pType);

                pRoot->InsertEndChild(pAssembly);
            }
            tinyxml2::XMLError eResult = xmlDoc.SaveFile(path.c_str());
            checkError(eResult);
        }

    private:
        std::string path;

        static DeviceConfigFile parseDeviceNode(tinyxml2::XMLNode *pDevice) {
            DeviceConfigFile device;
            tinyxml2::XMLElement *pAssembly = pDevice->FirstChildElement("Assembly");
            while (pAssembly != nullptr) {
                device.assemblies.push_back(parceAssemblyNode(pAssembly));
                pAssembly = pAssembly->NextSiblingElement("Assembly");
            }
            return device;
        }

        static AssemblyConfigFile parceAssemblyNode(tinyxml2::XMLElement *pAssembly) {
            AssemblyConfigFile assembly;
            tinyxml2::XMLElement *pAddr = pAssembly->FirstChildElement("Addr");
            if (pAddr == nullptr)
                throw DriverException(ExceptionProducer::DEVICE, ExceptionType::CONFIG_EXCEPTION, "Error reading device configuration");
            const char *addr = pAddr->GetText();
            if (addr == nullptr)
                throw DriverException(ExceptionProducer::DEVICE, ExceptionType::CONFIG_EXCEPTION, "Error reading device configuration");
            assembly.addr = addr;

            tinyxml2::XMLElement *pType = pAssembly->FirstChildElement("Type");
            if (pType == nullptr)
                throw DriverException(ExceptionProducer::DEVICE, ExceptionType::CONFIG_EXCEPTION, "Error reading device configuration");
            int type;
            tinyxml2::XMLError eResult = pType->QueryIntText(&type);
            checkError(eResult);
            assembly.type = static_cast<AssemblyType>(type);
            return assembly;
        }

        static inline void checkError(tinyxml2::XMLError error) {
            if (error != tinyxml2::XML_SUCCESS) {
                throw DriverException(ExceptionProducer::DEVICE, ExceptionType::CONFIG_EXCEPTION, "Error reading device configuration");
            }
        }

    };
}
