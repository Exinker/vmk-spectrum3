#pragma once

#include "asio.hpp"
#include <string>
#include <cstdint>
#include <vector>
#include "utils/DriverException.h"
#include "utils/DriverConfig.h"
#include "assembly/IAssembly.h"

namespace hwlib {
    enum UdpOpcode : uint16_t {
        UDP_READ_INI = 0x800B,
        UDP_READ_SWAP_FILE = 0x8013,
        UDP_GET_SERIAL_NUMBER = 0x8014,
        UDP_SET_TIMER = 0x0002,
        UDP_READ_TIMER = 0x8002,
        UDP_SET_CHIP_PIXEL_NUMBER = 0x002C,
        UDP_READ_MULTILINE = 0x0005,
        UDP_SET_LINE_NUMBER = 0x0010,
        UDP_READ_MULTILINE_SYNC = 0x0011,
        UDP_SET_DOUBLE_TIMER = 0x0022,
        UDP_READ_TIMER_ENCHANCED = 0x8022,
        UDP_DISCONNECT_SOCK = 0x00FD,
        UDP_STOP = 0x00FE,
        UDP_WRITE_CR = 0x0001,
    };
    enum UdpRespCode : uint16_t {
        UDP_ACK = 0x0001,
        UDP_EXT = 0x0002,
        UDP_UNK = 0x0081,
        UDP_ERR = 0x0080,
        UDP_BSY = 0x0083,
    };

#pragma pack(push, 1)
    struct UdpRequestPacket {
        UdpOpcode opcode;
        uint16_t seq_num;
        uint8_t data[60];
    };

    struct UdpResponseHeader {
        UdpRespCode response_code;
        uint16_t padding;
        UdpOpcode opcode;
        uint16_t seq_num;
    };

    struct MasiIni {
        uint8_t chips_num;
        uint8_t pad0[3];
        uint16_t chip_pixel_num;
        ChipType chips_type;
        uint8_t ADC_rate;
        uint8_t configBits;
        uint8_t assembly_type;
        uint8_t pad1;
        uint16_t mtemb;
        uint16_t mtenb;
        uint32_t pixel_number;
        uint8_t dia_present;
        uint8_t thermostat_en;
        float mtr0;
        float mui0;
    };
#pragma pack(pop)


    class MasiEthernet {
    public:
        explicit MasiEthernet(const std::string &addr) : addr(asio::ip::make_address_v4(addr)),
                                                         broadcastAddr(asio::ip::make_address_v4(
                                                                 DriverConfig::broadcastAddr)) {}

        ~MasiEthernet() { disconnect(); };

        // connection management
        void commandConnect();

        void frameConnect();

        void disconnect();

        [[nodiscard]] bool isConnected() const;

        // Public masi commands

        void setExposure(unsigned int microseconds);

        void setDoubleExposure(
                unsigned int t1,
                uint16_t r1,
                unsigned int t2,
                uint16_t r2
        );

        bool readFrame(AssemblyInfo masiInfo, uint8_t *buffer);

        void writeControlRegister(uint8_t bits);

        void startReadingUnicast(uint32_t nFrames);

        void startReadingBroadcast(uint32_t nFrames);

        void initBroadcastReading(uint32_t nFrames);

        AssemblyStatus getStatus();

        AssemblyInfo readAssemblyInfo();

        TimerState readExposure();

        SwapFile readSwapFile();

        std::string readSerialNumber();

        void stopReading();


    private:
        uint16_t cr = 0;
        TimerState prevTimerState;
        static const size_t udpBuffSize = 128;
        uint8_t udpBuff[udpBuffSize] = {};
        uint16_t seq_num = 1;
        bool connected = false;
        asio::ip::address_v4 addr;
        asio::ip::address_v4 broadcastAddr;
        asio::io_context udpContext;
        asio::io_context tcpContext;
        asio::ip::tcp::socket tcpSock = asio::ip::tcp::socket(tcpContext);
        asio::ip::udp::socket udpSock = asio::ip::udp::socket(udpContext);
        asio::ip::udp::socket broadcastSock = asio::ip::udp::socket(udpContext);

        struct DeviceTimerValue {
            uint16_t significand;
            uint16_t exponent;
        };

        static DeviceTimerValue convertExposure(unsigned int microseconds) {
            unsigned int m = microseconds / 100;
            unsigned int exponent = 0;
            while (m >= (1 << 10)) {
                exponent++;
                m /= 10;
            }
            return DeviceTimerValue{static_cast<uint16_t>(m), static_cast<uint16_t>(exponent)};
        }

        static uint32_t convertExposureBack(uint16_t mant, uint16_t exp) {
            uint32_t m = mant * 100;
            while (exp > 0) {
                m *= 10;
                exp--;
            }
            return m;
        }

        inline void assertConnected() const {
            if (!connected) {
                throw DriverException(ExceptionProducer::ASSEMBLY, ExceptionType::NETWORK_EXCEPTION,
                                      "Assembly: " + addr.to_string() + " not connected");
            }
        }

        template<typename T>
        std::vector<std::vector<uint8_t>>
        sendCommand(UdpOpcode opcode, const T &data, int response_packets, bool force = false);

        template<typename T>
        int send(asio::ip::udp::socket *socket, UdpOpcode opcode, const T &data, bool force = false);

        std::vector<std::vector<uint8_t>>
        receive(asio::ip::udp::socket *socket, int opSeqNum, int response_packets, int timeout = -1);

        // Private masi commands
        MasiIni readIni();

        void setFrameNumber(uint32_t frameNumber);

        void setChipPixelNumber(uint32_t pixelsPerChip, uint16_t numChips, uint16_t synchroSize);

        void waitLastExposure() const;

        void runUdpContext(const std::function<void()> &onTimeout, int timeout = DriverConfig::timeout) {
            udpContext.restart();
            udpContext.run_for(std::chrono::milliseconds(timeout));
            if (!udpContext.stopped()) {
                onTimeout();
                throw DriverException(ExceptionProducer::ASSEMBLY, ExceptionType::NETWORK_EXCEPTION, "Assembly: " + addr.to_string() + " Timeout");
            }
        }

        void runTcpContext(const std::function<void()> &onTimeout, int timeout = DriverConfig::timeout) {
            tcpContext.restart();
            tcpContext.run_for(std::chrono::milliseconds(timeout));
            if (!tcpContext.stopped()) {
                onTimeout();
            }
        }

        void connectUdp();
    };
}