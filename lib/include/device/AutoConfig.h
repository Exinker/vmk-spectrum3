#pragma once

#include "assembly/MasiEthernet.h"
#include "utils/ConfigFileParser.h"
#include "utils/DriverConfig.h"

namespace hwlib {
    class AutoConfig {
    public:
        explicit AutoConfig() {
            broadcastAddr = asio::ip::udp::endpoint(asio::ip::address::from_string(DriverConfig::broadcastAddr),
                                                    DriverConfig::udpPort);
            socket.open(asio::ip::udp::v4());
            socket.set_option(asio::socket_base::broadcast(true));
        }

        virtual ~AutoConfig() {
            socket.close();
        }

        DeviceConfigFile config() {
            send();
            std::vector<std::string> addresses = receive();
            DeviceConfigFile cfg;
            for (auto &addr: addresses) {
                cfg.assemblies.push_back(AssemblyConfigFile(addr));
            }
            return cfg;
        }

    private:
        asio::ip::udp::endpoint broadcastAddr;
        int timeout_milliseconds = 500;
        asio::io_context ioContext;
        asio::ip::udp::socket socket = asio::ip::udp::socket(ioContext);

        void runIoContext(const std::function<void()> &onTimeout) {
            ioContext.restart();
            ioContext.run_for(std::chrono::milliseconds(timeout_milliseconds));
            if (!ioContext.stopped()) {
                onTimeout();
            }
        }

        void send() {
            UdpRequestPacket packet = {UdpOpcode::UDP_READ_TIMER, 0, 0};
            asio::error_code udp_error;

            socket.async_send_to(asio::buffer(&packet, sizeof(packet)),
                                 broadcastAddr,
                                 [&](const asio::error_code &error, size_t s) {
                                     udp_error = error;
                                 });

            runIoContext([&] {});
            if (udp_error) {
                throw DriverException(ExceptionProducer::DEVICE, ExceptionType::NETWORK_EXCEPTION, "Search assemblies error");
            }
        };

        std::vector<std::string> receive() {
            asio::error_code udp_error;
            asio::ip::udp::endpoint endpoint;
            uint8_t udpBuff[1] = {};
            volatile bool endFlag = true;
            std::vector<std::string> addresses;
            while (endFlag) {
                socket.async_receive_from(asio::buffer(udpBuff, 1),
                                          endpoint,
                                          [&](const asio::error_code &error, size_t size) {
                                              udp_error = error;
                                          });
                runIoContext([&] { endFlag = false; });
                if (!endFlag) {
                    break;
                }
                if (udp_error) {
                    throw DriverException(ExceptionProducer::DEVICE, ExceptionType::NETWORK_EXCEPTION, "Search assemblies error");
                }
                if (std::count(addresses.begin(), addresses.end(), endpoint.address().to_string())) {
                    throw DriverException(ExceptionProducer::DEVICE, ExceptionType::NETWORK_EXCEPTION, "Found two assemblies with same IP adresses");
                }
                addresses.push_back(endpoint.address().to_string());
            }
            return addresses;
        }
    };
}