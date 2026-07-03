import sys
import binascii
import struct
import signal

hostmap = {
    '10.116.220.1': 'lin',
    '10.116.220.2': 'dev',
    '10.116.220.3': 'win',
}

devip = '10.116.220.2'

opcodes = {
    0x0001: 'WriteCr',
    0x0002: 'SetTimer',
    0x0005: 'ReadMultiLine',
    0x000c: 'SetLineLength',
    0x0010: 'SetLineNumber',
    0x0011: 'ReadMultiLineSync',
    0x0020: 'WriteRelePrgMem',
    0x00fd: 'DisconnectDataSoc',
    0x00fe: 'StopKadr',
    0x8001: 'ReadCr',
    0x8002: 'ReadTimer',
    0x800a: 'ReadSensors',
    0x800b: 'ReadIniFile',
    0x8013: 'ReadSwaps',
}
respcodes = {
    0x0001: 'ACK',
    0x0002: 'EXT',
    0x0081: 'UNK',
    0x0080: 'ERR',
    0x0083: 'BSY',
}

# opcode_max_len = max(map(lambda a: len(a), opcodes.values()))

def pp_hex(kind, v) -> str:
    max_len = max(map(lambda a: len(a), kind.values()))
    if v in kind.keys():
        s = kind[v]
    else:
        s = ''
    s = s.ljust(max_len)
    return f'{v:04X} ({s})'

operation_cache: dict[tuple[int, bytes], list[bytes]] = dict()
current_request = None

def parse_request(data: bytes) -> str:
    global current_request
    try:
        opcode, seqnum = struct.unpack('<HH', data[:4])
        payload = data[4:]
        current_request = (opcode, payload)
        return f'request  seq={seqnum:04X}, {" "*17}cmd={pp_hex(opcodes, opcode)}, payload={binascii.hexlify(payload)}'
    except Exception:
        return f'request  invalid, data={binascii.hexlify(data)}'
    
def parse_response(data: bytes) -> str:
    global current_request
    try:
        resp_code, opcode, seqnum = struct.unpack('<H2xHH', data[:8])
        payload = data[8:]
        if resp_code == 1 and current_request is not None and current_request:
            if current_request in operation_cache.keys():
                current_request = None
            else:
                operation_cache[current_request] = [payload]
        if resp_code == 2 and current_request is not None:
            operation_cache[current_request].append(payload)
        return f'response seq={seqnum:04X}, code={pp_hex(respcodes, resp_code)}, cmd={pp_hex(opcodes, opcode)}, payload={binascii.hexlify(payload)}'
    except Exception:
        return f'response invalid, data={binascii.hexlify(data)}'

def stop(a, b):
    pass

signal.signal(signal.SIGINT, stop)



for line in sys.stdin:
    src, dst, data = line.split('\t')
    data = data.strip()
    # print(data)
    print(f'{hostmap[src]} -> {hostmap[dst]} ', end='')
    data = binascii.unhexlify(data)
    if src == devip:
        print(parse_response(data))
    else:
        print(parse_request(data))

print('--- Command Cache ---')
print(operation_cache)
