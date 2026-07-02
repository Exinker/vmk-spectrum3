#include "utils/DriverConfig.h"

namespace hwlib {
    std::string DriverConfig::broadcastAddr = "10.116.220.255";
    int DriverConfig::udpPort = 555;
    int DriverConfig::tcpPort = 556;
    bool DriverConfig::logging = true;
    int DriverConfig::timeout = 5000;
    int DriverConfig::readBufferSize = 1000;
}