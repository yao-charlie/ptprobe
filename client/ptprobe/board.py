import serial
import struct

class Controller:
    def __init__(self, port, baudrate=115200):
        self.comm = serial.Serial()
        self.comm.port = port
        self.comm.baudrate = baudrate
    
    def board_id(self):
        with self.comm as ser:
            ser.write(b'AB\n')    # ask board ID
            buf = ser.read(size=5)
            return int.from_bytes(buf[1:], "big")

    def temperature(self, ch):
        with self.comm as ser:
            msg = "AT{}\n".format(ch)
            ser.write(bytes(msg,'utf-8'))  
            hdr = ser.read(size=1)
            if (hdr[0] & 0xC0) >> 6 != 0x02:
                raise Exception("Bad header (wrong header type): 0x{:x}".format(hdr[0]))
            if (hdr[0] & 0x38) >> 3 != 0x02:
                raise Exception("Bad header (wrong response type): 0x{:x}".format(hdr[0]))
            if (hdr[0] & 0x06) >> 1 != ch:
                raise Execption("Bad header (channel {} mismatch): 0x{:x}".format(ch, hdr[0]))
            err_flag = (hdr[0] & 0x01) != 0
            
            ec = 0
            T = -999.9
            buf = ser.read(size=4)
            #return (buf, err_flag)
            if err_flag:
                ec = struct.unpack('>I',buf)[0]
            else:
                T = struct.unpack('>f',buf)[0]

            return (T, ec)

    def set_debug_level(self, lvl):
        with self.comm as ser:
            msg = "CD{}\n".format(lvl)
            ser.write(bytes(msg,'utf-8'))


