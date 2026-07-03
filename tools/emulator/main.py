import socket
import struct
import time
from enum import IntEnum, Enum
from dataclasses import dataclass
from typing import Any, Callable
from queue import Queue
from threading import Thread
from cache import unknown_op_cache
import numpy as np


class UdpOpcode(IntEnum):
    WRITE_CR = 0x0001
    READ_CR = 0x8001
    READ_SENSORS = 0x800A
    READ_INI = 0x800B
    READ_SWAP = 0x8013
    SET_TIMER = 0x0002
    SET_LINE_LENGTH = 0x000C
    READ_MULTILINE = 0x0005
    DISCONNECT_SOCK = 0x00FD
    UDP_SET_LINE_NUMBER = 0x0010
    UDP_READ_MULTILINE_SYNC = 0x0011
    UDP_SET_DOUBLE_TIMER = 0x0022
    UDP_STOP = 0x00FE
    UDP_READ_TIMER_ENCHANCED = 0x8022


class UdpRespcode(IntEnum):
    UDP_ACK = 0x0001
    UDP_EXT = 0x0002
    UDP_UNK = 0x0081


@dataclass()
class UdpRequestPacket:
    opcode: int
    seqnum: int
    data: bytes
    host_addr: Any


class TcpServerCmd(Enum):
    STOP_FRAME = 1
    DISCONNECT = 2
    START_FRAME = 3



class TcpServer(Thread):
    def __init__(self, addr: str, synchro: bytes, frame_size: int):
        super().__init__(daemon=True)
        self.socket = None
        self.addr = addr
        self.cmd_queue = Queue()
        self.frame_size = frame_size
        self.synchro = synchro
        self.cntr = 0
        self.client_running = False


    def generate_frame(self):
        fr = []
        for i in range(1, 9):
            a = np.random.randint(700, 1000, size=(self.frame_size // 8))
            a *= i
            a = a.astype(np.uint16, order='C')
            fr.append(a)
        result = np.concatenate(fr, axis=0)
        result = result.astype(np.uint16, order='C')
        result = np.bitwise_xor(result, 1 << 15)
        return result

    def run(self) -> None:
        while True:
            if not self.client_running:
                self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                self.socket.bind((self.addr, 556))
            self.socket.listen()
            print('Accepting.....')
            client, _ = self.socket.accept()
            print('Accepted client')
            self.client_running = True
            while self.client_running:
                cmd = self.cmd_queue.get()
                print(f'TCP: got cmd {cmd}')
                if cmd == TcpServerCmd.START_FRAME:
                    break_flag = False
                    n_times = self.cmd_queue.get()
                    print(f'TCP: {n_times=}')
                    exposure = self.cmd_queue.get()
                    print(f'TCP: {exposure=}')
                    for i in range(n_times):
                        frame = self.synchro + self.generate_frame().tobytes()
                        time.sleep(exposure)
                        if not self.cmd_queue.empty():
                            new_cmd = self.cmd_queue.get_nowait()
                            if new_cmd == TcpServerCmd.STOP_FRAME:
                                client.close()
                                break_flag = True
                                break
                            elif new_cmd == TcpServerCmd.DISCONNECT:
                                client.close()
                                break_flag = True
                                break
                        try:
                            print(i)
                            client.send(frame)
                        except Exception:
                            print('Client disconnected')
                            self.socket.close()
                            break
                    if break_flag:
                        break

                elif cmd == TcpServerCmd.DISCONNECT:
                    client.close()
                    break
                elif cmd == TcpServerCmd.STOP_FRAME:
                    client.close()
                    break
                print('TCP: finished sending')










class SpectrometerEmulator:
    handlers: dict[int, Callable[[UdpRequestPacket], list[bytes]]] = {}
    def __init__(self, addr: str):
        self.addr = addr
        self.exposure = 0.001
        self.exp_m = 1
        self.exp_e = 1
        self.line_number = 0
        self.udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.handlers = {
            # UdpOpcode.READ_CR: self.handle_read_cr,
            # UdpOpcode.READ_SENSORS: self.handle_read_sensors,
            # UdpOpcode.READ_INI: self.handle_read_ini_file,
            UdpOpcode.SET_LINE_LENGTH: self.handle_set_line_length,
            UdpOpcode.READ_MULTILINE: self.handle_read_multiline,
            UdpOpcode.SET_TIMER: self.handle_set_timer,
            UdpOpcode.WRITE_CR: self.handle_write_cr,
            UdpOpcode.UDP_SET_LINE_NUMBER: self.handle_set_line_number,
            UdpOpcode.UDP_READ_MULTILINE_SYNC: self.handle_read_multiline_sync,
            UdpOpcode.UDP_SET_DOUBLE_TIMER: self.handle_set_double_timer,
            UdpOpcode.DISCONNECT_SOCK: self.handle_disconnect_sock,
            UdpOpcode.UDP_STOP: self.handle_cansel_reading,
            UdpOpcode.UDP_READ_TIMER_ENCHANCED: self.handle_read_timer
        }
        self.tcp_server = TcpServer(addr, struct.pack('<HHHHHH', 0, 0, 0x8000, 0x8000, 0xabab, 0xabab), 2048)
        self.tcp_server.start()

    def handle_read_cr(self, data: UdpRequestPacket)->list[bytes]:
        return [struct.pack('<H', 0)]

    def handle_read_sensors(self, data: UdpRequestPacket):
        return [struct.pack('<4xH', 0xFFFF)]

    def handle_read_ini_file(self, data: UdpRequestPacket):
        # chips_num, chip_pixel_num, chip_type, adc_rate, config_bits, assembly_type, min_exposure_value, min_exposure_exponent, pixel_number, dia_present, termostat_en, temp0, v0
        ini = struct.pack('<B3xHHBBxBHHIBBff', 2, 2048, 0x0501, 0x00, 0x00, 0x04, 7, 0, 2054, 0xAB, 0x00, 10.0, 0)
        return [ini]

    def handle_read_swap_file(self, data: UdpRequestPacket):
        pass

    def handle_write_cr(self, data: UdpRequestPacket):
        return [b'']

    def handle_cansel_reading(self, data: UdpRequestPacket):
        print('handle_cansel_reading')
        self.tcp_server.cmd_queue.put(TcpServerCmd.STOP_FRAME)
        return [b'']

    def handle_set_timer(self, data: UdpRequestPacket):
        m, e = struct.unpack('<H2xH', data.data[:6])
        self.exp_m = m
        self.exp_e = e
        self.exposure = 0.000_1 * m * (10**e)
        print(f'Set timer to {self.exposure}')
        return [b'']

    def handle_disconnect_sock(self, data: UdpRequestPacket):
        if self.tcp_server.client_running:
            self.tcp_server.cmd_queue.put(TcpServerCmd.DISCONNECT)
        return [b'']

    def handle_set_double_timer(self, data: UdpRequestPacket):
        m1, e1, m2, e2, n1, n2 = struct.unpack('<H2xH2xH2xH2xH2xH2x', data.data[:24])
        exp1 = 0.000_1 * m1 * (10**e1)
        exp2 = 0.000_1 * m2 * (10**e2)
        print(f"{exp1=}, {exp2=}")
        print(f"{n1=}, {n2=}")
        return [b'']

    def handle_set_line_length(self, data: UdpRequestPacket):
        pixels_num, lines_num = struct.unpack('<IH', data.data[:6])
        print(f'{pixels_num=}, {lines_num=}')
        return [b'']

    def handle_set_line_number(self, data: UdpRequestPacket):
        self.line_number = struct.unpack('<4xI', data.data[:8])[0]
        print(f'Set line number to {self.line_number=}')
        return [b'']

    def handle_read_multiline(self, data: UdpRequestPacket):
        # TODO: handle cr
        cr, n_times = struct.unpack('<H2xI', data.data[:8])
        print(f'Reading {n_times=}')
        self.tcp_server.cmd_queue.put(TcpServerCmd.START_FRAME)
        self.tcp_server.cmd_queue.put(n_times)
        self.tcp_server.cmd_queue.put(self.exposure)
        return [b'']

    def handle_read_timer(self, data: UdpRequestPacket):
        timer = struct.pack('<4xH2xH2xH18x', 0, self.exp_m, self.exp_e)
        return [timer]

    def handle_read_multiline_sync(self, data: UdpRequestPacket):
        # TODO: handle cr
        cr, ps = struct.unpack('<H6xH', data.data[:10])
        print(f'Reading {self.line_number=}')
        self.tcp_server.cmd_queue.put(TcpServerCmd.START_FRAME)
        self.tcp_server.cmd_queue.put(self.line_number)
        self.tcp_server.cmd_queue.put(self.exposure)
        return [b'']


    def decode_udp_request(self, data: bytes, addr: Any) -> UdpRequestPacket:
        opcode, seqnum = struct.unpack('<HH', data[:4])
        return UdpRequestPacket(opcode, seqnum, data[4:], addr)

    def send_udp_response(self, request: UdpRequestPacket, respcode: UdpRespcode, data: bytes):
        packet = struct.pack('<H2xHH', respcode, request.opcode, request.seqnum) + data
        self.udp_socket.sendto(packet, request.host_addr)

    def run(self):
        self.udp_socket.bind((self.addr, 555))
        while True:
            packet, host_addr = self.udp_socket.recvfrom(1024)
            request = self.decode_udp_request(packet, host_addr)
            if request.opcode in self.handlers.keys():
                print(f'Got known cmd {request.opcode:04x} ({UdpOpcode(request.opcode).name})')
                resp = self.handlers[request.opcode](request)
                for i, b in enumerate(resp):
                    respcode = UdpRespcode.UDP_ACK if i == 0 else UdpRespcode.UDP_EXT
                    self.send_udp_response(request, respcode, b)
            elif (key := (request.opcode, request.data)) in unknown_op_cache.keys():
                # print(f'Got CACHED cmd {request.opcode:04x}')
                resp = unknown_op_cache[key]
                for i, b in enumerate(resp):
                    respcode = UdpRespcode.UDP_ACK if i == 0 else UdpRespcode.UDP_EXT
                    self.send_udp_response(request, respcode, b)
            else:
                print(f'Got UNKNOWN cmd {request.opcode} {request.data}')
                self.send_udp_response(request, UdpRespcode.UDP_UNK, b'')


if __name__ == '__main__':
    e = SpectrometerEmulator('127.0.0.1')
    e.run()
