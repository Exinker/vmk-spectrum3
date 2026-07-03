#pragma once

#include <string>
#include "tinyxml2.h"
#include "utils/DriverException.h"

namespace hwlib {
    class DriverConfig {
    public:

        static std::string broadcastAddr;
        static int udpPort;
        static int tcpPort;
        static bool logging;
        static int timeout;
        static int readBufferSize;

        static void parse(const std::string &path) {
            tinyxml2::XMLDocument xmlDocument;
            tinyxml2::XMLError eResult = xmlDocument.LoadFile(path.c_str());
            if (eResult != tinyxml2::XML_SUCCESS) {
                throw DriverException(ExceptionProducer::DEVICE, ExceptionType::CONFIG_EXCEPTION, "Config file not found");
            }
            tinyxml2::XMLElement *rootNode = xmlDocument.FirstChildElement();
            if (rootNode == nullptr)
                throw DriverException(ExceptionProducer::DEVICE, ExceptionType::CONFIG_EXCEPTION, "Config file read error");

            broadcastAddr = getField(rootNode, "BROADCAST");
            udpPort = std::stoi(getField(rootNode, "UDP_PORT"));
            tcpPort = std::stoi(getField(rootNode, "TCP_PORT"));
            logging = getField(rootNode, "LOGGING") == "TRUE";
            timeout = std::stoi(getField(rootNode, "TIMEOUT"));
            readBufferSize = std::stoi(getField(rootNode, "READ_BUFFER"));
        }

        static std::string getField(tinyxml2::XMLElement *rootNode, const std::string &name) {
            if (rootNode == nullptr) throw DriverException(ExceptionProducer::DEVICE, ExceptionType::CONFIG_EXCEPTION, "Config not initialized");
            tinyxml2::XMLElement *element = rootNode->FirstChildElement(name.c_str());
            if (element == nullptr)
                throw DriverException(ExceptionProducer::DEVICE, ExceptionType::CONFIG_EXCEPTION,
                                      "Config file read error, field " + name + " not found");
            const char *text = element->GetText();
            if (text == nullptr)
                throw DriverException(ExceptionProducer::DEVICE, ExceptionType::CONFIG_EXCEPTION,
                                      "Config file read error, field " + name + " not found");
            return text;
        }
    };
}


