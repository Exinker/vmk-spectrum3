#include "assembly/MasiEthernet.h"
#include <cstring>
#include <cmath>
#include <iostream>
#include <chrono>

namespace hwlib {
    void MasiEthernet::commandConnect() {
        if (connected) {
            return;
        }
        connectUdp();
        int opSeqNum = send(&udpSock, UDP_DISCONNECT_SOCK, 0, true);
        try {
            receive(&udpSock, opSeqNum, 1, 100);
        } catch (DriverException) {
            throw DriverException(ExceptionProducer::ASSEMBLY, ExceptionType::NETWORK_EXCEPTION,
                                  "Assembly " + addr.to_string() + " not connect");
        }
        connected = true;
    }

    void MasiEthernet::disconnect() {
        tcpSock.close();
        udpSock.close();
        broadcastSock.close();
        connected = false;
    }

    bool MasiEthernet::isConnected() const {
        return connected;
    }

    template<typename T>
    std::vector<std::vector<uint8_t>>
    MasiEthernet::sendCommand(UdpOpcode opcode, const T &data, int response_packets, bool force) {
        int opSeqNum = send(&udpSock, opcode, data, force);
        std::vector<std::vector<uint8_t>> result = receive(&udpSock, opSeqNum, response_packets);
        return result;
    }

    template<typename T>
    int MasiEthernet::send(asio::ip::udp::socket *socket, UdpOpcode opcode, const T &data, bool force) {
        static_assert(sizeof(T) <= sizeof(UdpRequestPacket::data));
        if (!force) {
            assertConnected();
        }
        UdpRequestPacket packet = {opcode, seq_num++, 0};
        std::memcpy(packet.data, reinterpret_cast<const void *>(&data), sizeof(T));

        asio::error_code udp_error;

        socket->async_send(asio::buffer(&packet, sizeof(packet)),
                           [&](const asio::error_code &error, size_t s) {
                               udp_error = error;
                           });

        runUdpContext([&] {
            disconnect();
        });
        if (udp_error) {
            throw DriverException(ExceptionProducer::ASSEMBLY, ExceptionType::NETWORK_EXCEPTION,
                                  "Assembly: " + addr.to_string() + " Send command error: " +
                                  udp_error.message() + std::to_string(udp_error.value()));
        }

        return packet.seq_num;
    }

    std::vector<std::vector<uint8_t>>
    MasiEthernet::receive(asio::ip::udp::socket *socket, int opSeqNum, int response_packets, int timeout) {
        asio::error_code udp_error;
        std::vector<std::vector<uint8_t>> result(response_packets);
        for (int i = 0; i < response_packets; i++) {
            size_t packetSize = 0;
            asio::ip::udp::endpoint endpoint;

            socket->async_receive(asio::buffer(udpBuff, udpBuffSize),
                                  [&](const asio::error_code &error, size_t size) {
                                      udp_error = error;
                                      packetSize = size;
                                  });
            if (timeout == -1) {
                runUdpContext([&] {
                    disconnect();
                });
            } else {
                runUdpContext([&] {
                    disconnect();
                }, timeout);
            }
            if (udp_error) {
                throw DriverException(ExceptionProducer::ASSEMBLY, ExceptionType::NETWORK_EXCEPTION,
                                      "Assembly: " + addr.to_string() + " Receive packet error " + std::to_string(udp_error.value()));
            }

            auto *header = reinterpret_cast<UdpResponseHeader *>(udpBuff);
            auto *response_data = reinterpret_cast<uint8_t *>(udpBuff + sizeof(UdpResponseHeader));
            size_t data_size = packetSize - sizeof(UdpResponseHeader);

            if ((i == 0 && header->response_code != UDP_ACK) || (i > 0 && header->response_code != UDP_EXT)) {
                throw DriverException(ExceptionProducer::ASSEMBLY, ExceptionType::NETWORK_EXCEPTION, "Assembly: " + addr.to_string() +
                                                                   " Assembly does not support this command, update firmware");
            }
            if (header->seq_num != opSeqNum) {
                throw DriverException(ExceptionProducer::ASSEMBLY, ExceptionType::NETWORK_EXCEPTION, "Assembly: " + addr.to_string() +
                                                                   " The assembly returned an incorrect package number, please restart the application");
            }

            result[i].insert(result[i].begin(), response_data, response_data + data_size);
        }

        return result;
    }


    MasiIni MasiEthernet::readIni() {
        auto data = sendCommand(UDP_READ_INI, 0, 2);
        MasiIni result = *reinterpret_cast<MasiIni *>(data[1].data());
        return result;
    }

    std::string MasiEthernet::readSerialNumber() {
        auto data = sendCommand(UDP_GET_SERIAL_NUMBER, 0, 2);
        std::string num(reinterpret_cast<const char *>(data[1].data() + 2), data[1][1]);
        return num;
    }

    AssemblyInfo MasiEthernet::readAssemblyInfo() {
        auto ini = readIni();
        return AssemblyInfo{
                ini.chips_num,
                ini.chip_pixel_num,
                ini.chips_type,
                static_cast<int>(100 * ini.mtemb * pow(10, ini.mtenb)),
                ini.mtr0,
                ini.mui0,
                ini.pixel_number,
        };

    }

    AssemblyStatus MasiEthernet::getStatus() {
        try {
            commandConnect();
            int opSeqNum = send(&udpSock, UDP_READ_TIMER, 0, true);
            receive(&udpSock, opSeqNum, 1, 100);
        } catch (DriverException &e) {
            disconnect();
            return AssemblyStatus::DISCONNECTED;
        }
        return AssemblyStatus::ALIVE;
    }

    void MasiEthernet::setExposure(unsigned int microseconds) {
#pragma pack(push, 1)
        struct PacketSetTimer {
            uint16_t significand;
            uint8_t pad[2];
            uint16_t exponent;
        };
#pragma pack(pop)

        AssemblyInfo masiInfo = readAssemblyInfo();
        if (microseconds < masiInfo.minExposure) {
            throw DriverException(ExceptionProducer::ASSEMBLY, ExceptionType::INVALID_ARGUMENT,
                                  "Assembly: " + addr.to_string() + " Too low exposure");
        }
        auto data = convertExposure(microseconds);
        prevTimerState = readExposure();
        sendCommand(UDP_SET_TIMER, PacketSetTimer{data.significand, {0}, data.exponent}, 1);
    }

    void MasiEthernet::setDoubleExposure(unsigned int t1, uint16_t r1, unsigned int t2, uint16_t r2) {
#pragma pack(push, 1)
        struct PacketSetDoubleTimer {
            PacketSetDoubleTimer(
                    uint16_t s1,
                    uint16_t e1,
                    uint16_t s2,
                    uint16_t e2,
                    uint16_t r1,
                    uint16_t r2
            ) : s1(s1), e1(e1), s2(s2), e2(e2), r1(r1), r2(r2) {}

            uint16_t s1;
            uint16_t pad0 = 0;
            uint16_t e1;
            uint16_t pad1 = 0;
            uint16_t s2;
            uint16_t pad2 = 0;
            uint16_t e2;
            uint16_t pad3 = 0;
            uint16_t r1;
            uint16_t pad4 = 0;
            uint16_t r2;
            uint16_t pad5 = 0;
        };
#pragma pack(pop)

        AssemblyInfo masiInfo = readAssemblyInfo();
        if (t1 < masiInfo.minExposure || t2 < masiInfo.minExposure) {
            throw DriverException(ExceptionProducer::ASSEMBLY, ExceptionType::INVALID_ARGUMENT,
                                  "Assembly: " + addr.to_string() + " Too low exposure");
        }

        auto data1 = convertExposure(t1);
        auto data2 = convertExposure(t2);
        prevTimerState = readExposure();
        sendCommand(UDP_SET_DOUBLE_TIMER, PacketSetDoubleTimer{
                data1.significand,
                data1.exponent,
                data2.significand,
                data2.exponent,
                r1,
                r2
        }, 1);

    }

    TimerState MasiEthernet::readExposure() {
#pragma pack(push, 1)
        struct PacketGetTimerResponse {
            uint16_t timerVersion;
            uint16_t pad0 = 0;
            uint16_t timerMode; // больше 0 при двойной экспозиции (не работает)
            uint16_t pad1 = 0;
            uint16_t s1;
            uint16_t pad2 = 0;
            uint16_t e1;
            uint16_t pad3 = 0;
            uint16_t s2;
            uint16_t pad4 = 0;
            uint16_t e2;
            uint16_t pad5 = 0;
            uint16_t r1;
            uint16_t pad6 = 0;
            uint16_t r2;
            uint16_t pad7 = 0;
        };
#pragma pack(pop)

        auto data = sendCommand(UDP_READ_TIMER_ENCHANCED, 0, 1);
        PacketGetTimerResponse resp = *reinterpret_cast<PacketGetTimerResponse *>(data[0].data());
        if (resp.timerMode <= 2) { // должно быть 0, но устройство выдает иногда 2 при одиночной экспозиции
            return TimerState(convertExposureBack(resp.s1, resp.e1));
        } else {
            return TimerState(DoubleTimer(
                    convertExposureBack(resp.s1, resp.e1), resp.r1,
                    convertExposureBack(resp.s2, resp.e2), resp.r2
            ));
        }
    }

    void MasiEthernet::setChipPixelNumber(uint32_t pixelsPerChip, uint16_t numChips, uint16_t synchroSize) {
#pragma pack(push, 1)
        struct PacketSetChipPixelNumber {
            uint32_t pixelsPerChip;
            uint16_t numChips;
            uint16_t synchroSize;
        };
#pragma pack(pop)

        sendCommand(UDP_SET_CHIP_PIXEL_NUMBER, PacketSetChipPixelNumber{pixelsPerChip, numChips, synchroSize}, 1);
    }

    SwapFile MasiEthernet::readSwapFile() {
        auto data = sendCommand(UDP_READ_SWAP_FILE, 0, 2);
        SwapFile sf;
        sf.length = data[1][1];
        std::copy(data[1].data() + 2, data[1].data() + 2 + sf.length, sf.file);
        return sf;
    }

    bool MasiEthernet::readFrame(AssemblyInfo masiInfo, uint8_t *buffer) {
        size_t totalUsefulPixels = masiInfo.numChips * masiInfo.pixelsPerChip;
        static const size_t pixelSize = 2;
        size_t synchroSize = (masiInfo.numPixels - totalUsefulPixels) * pixelSize;
        auto *synchroBuff = new uint8_t[synchroSize];

        std::array<asio::mutable_buffer, 2> readBuffers = {
                asio::buffer(synchroBuff, synchroSize),
                asio::buffer(buffer, totalUsefulPixels * pixelSize),
        };
        asio::error_code tcpError;
        asio::read(tcpSock, readBuffers, tcpError);
        delete[] synchroBuff;

        if (tcpError) {
            if (tcpError == asio::error::timed_out) {
                return false;
            }
            throw DriverException(ExceptionProducer::ASSEMBLY, ExceptionType::NETWORK_EXCEPTION,
                                  "Assembly: " + addr.to_string() + " receive packet error " + std::to_string(tcpError.value()));
        }

        // TODO: check synchro signal, check actual size of received data
        for (int i = 0; i < totalUsefulPixels; i++) {
            ((uint16_t *) buffer)[i] ^= (1 << 15);
        }
        return true;
    }

    void MasiEthernet::waitLastExposure() const {
        std::chrono::milliseconds elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now() - prevTimerState.stateTime);
        if (!prevTimerState.isDouble) {
            auto msTimer = std::chrono::milliseconds(prevTimerState.single / 1000);
            if (msTimer > elapsed_ms) {
                std::this_thread::sleep_for(msTimer - elapsed_ms);
            }
        } else {
            auto msTimer = std::chrono::milliseconds((prevTimerState.dbl.t1 * prevTimerState.dbl.r1) / 1000 +
                                                     (prevTimerState.dbl.t2 * prevTimerState.dbl.r2) / 1000);
            if (msTimer > elapsed_ms) {
                std::this_thread::sleep_for(msTimer - elapsed_ms);
            }
        }
    }

    void MasiEthernet::startReadingUnicast(uint32_t nFrames) {
#pragma pack(push, 1)
        struct PacketReadMultiline {
            uint16_t cr;
            uint8_t pad[2];
            uint32_t numLines;
        };
#pragma pack(pop)

        waitLastExposure(); // нужно подождать чтобы быть уверенным, что экспозиция переключилась
        AssemblyInfo masiInfo = readAssemblyInfo();
        setChipPixelNumber(masiInfo.pixelsPerChip, masiInfo.numChips,
                           masiInfo.numPixels - (masiInfo.pixelsPerChip * masiInfo.numChips));
        sendCommand(UDP_READ_MULTILINE, PacketReadMultiline{cr, {0, 0}, nFrames}, 1);
    }

    void MasiEthernet::setFrameNumber(uint32_t frameNumber) {
#pragma pack(push, 1)
        struct PacketSetLineNumber {
            uint8_t pad[4];
            uint32_t numLines;
        };
#pragma pack(pop)

        sendCommand(UDP_SET_LINE_NUMBER, PacketSetLineNumber{{0, 0, 0, 0}, frameNumber}, 1);
    }

    void MasiEthernet::initBroadcastReading(uint32_t nFrames) {
        AssemblyInfo masiInfo = readAssemblyInfo();
        setChipPixelNumber(masiInfo.pixelsPerChip, masiInfo.numChips,
                           masiInfo.numPixels - (masiInfo.pixelsPerChip * masiInfo.numChips));
        setFrameNumber(nFrames);
    }

    void MasiEthernet::startReadingBroadcast(uint32_t nFrames) {
#pragma pack(push, 1)
        struct PacketReadMultilineSync {
            uint16_t cr;
            uint8_t pad[6];
            uint16_t phasenSync;
        };
#pragma pack(pop)

        initBroadcastReading(nFrames);
        send(&broadcastSock, UDP_READ_MULTILINE_SYNC, PacketReadMultilineSync{cr, {0, 0, 0, 0, 0, 0}, 0});
        // чтение с broadcast сокета
    }

    void MasiEthernet::stopReading() {
        // Send command to stop reading frame
        sendCommand(UDP_STOP, 0, 1);

        tcpContext.stop();
        // reconnect to the tcp socket
        tcpSock.close();
        frameConnect();
        tcpContext.restart();
    }

    void MasiEthernet::writeControlRegister(uint8_t bits) {
#pragma pack(push, 1)
        struct PacketWriteCr {
            uint16_t bits;
        };
#pragma pack(pop)

        cr = bits;
        sendCommand(UDP_WRITE_CR, PacketWriteCr{bits}, 1);
    }

    void MasiEthernet::frameConnect() {
        tcpSock.open(asio::ip::tcp::v4());
        tcpSock.set_option(asio::detail::socket_option::integer<SOL_SOCKET, SO_RCVTIMEO>{100});
        asio::error_code tcp_error;
        tcpSock.async_connect(
                asio::ip::tcp::endpoint(addr, DriverConfig::tcpPort),
                [&](const asio::error_code &error) {
                    tcp_error = error;
                });
        runTcpContext([&] {
            tcpSock.close();
        });

        if (tcp_error) {
            throw DriverException(ExceptionProducer::ASSEMBLY, ExceptionType::NETWORK_EXCEPTION,
                                  "Assembly: " + addr.to_string() + " Connection error (TCP)");
        }
    }

    void MasiEthernet::connectUdp() {
        asio::error_code udp_error;
        udpSock.async_connect(
                asio::ip::udp::endpoint(addr, DriverConfig::udpPort),
                [&](const asio::error_code &error) {
                    udp_error = error;
                });
        runUdpContext([&] {
            udpSock.close();
        });
        if (udp_error) {
            throw DriverException(ExceptionProducer::ASSEMBLY, ExceptionType::NETWORK_EXCEPTION,
                                  "Assembly: " + addr.to_string() + " Connection error (UDP)");
        }
        broadcastSock.open(asio::ip::udp::v4());
        broadcastSock.set_option(asio::socket_base::broadcast(true));
        broadcastSock.async_connect(asio::ip::udp::endpoint(broadcastAddr, DriverConfig::udpPort),
                                    [&](const asio::error_code &error) {
                                        udp_error = error;
                                    });
        runUdpContext([&] {
            udpSock.close();
            broadcastSock.close();
        });
        if (udp_error) {
            throw DriverException(ExceptionProducer::ASSEMBLY, ExceptionType::NETWORK_EXCEPTION,
                                  "Assembly: " + addr.to_string() + " Connection error (BROADCAST)");
        }
    }
}