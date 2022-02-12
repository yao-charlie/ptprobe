import serial
import struct

class BadHeader(Exception):
    """An error in the packet header"""
    pass

class BadPacket(Exception):
    """A packet has a formatting error"""
    pass

class Controller:
    """A board controller for the PT Probe board using serial communication"""

    class PacketType:
        """Packet types in the upper two bits of the header byte"""
        RSVD = 0b00
        DATA = 0b01
        RESP = 0b10
        HALT = 0b11

    class ResponseType:
        """The type of response in RESP packet stored in bits 3-5 of the header byte"""
        RSVD     = 0b000
        ID       = 0b001
        T        = 0b010
        P        = 0b011
        TREF     = 0b100
        ADC      = 0b101
        STATUS_T = 0b110
        STATUS_P = 0b111

    def __init__(self, port, baudrate=115200):
        self.comm = serial.Serial()
        self.comm.port = port
        self.comm.baudrate = baudrate
    
    def board_id(self):
        with self.comm as ser:
            ser.write(bytes('AB\n','utf-8'))    # ask board ID
            hdr = ser.read(size=1)
            err_bit = self._validate_resp_hdr(hdr, 0, self.ResponseType.ID)
            if not err_bit:
                return struct.unpack('>I',ser.read(size=4))[0]
        return 0

    def temperature(self, ch):
        return self._ask_resp('T',ch,self.ResponseType.T)

    def pressure(self, ch):
        return self._ask_resp('P',ch,self.ResponseType.P)

    def ref_temperature(self, ch):
        return self._ask_resp('R',ch,self.ResponseType.TREF)
    
    def raw_adc(self, ch):
        return self._ask_resp('A',ch,self.ResponseType.ADC)

    def sensor_status_T(self, ch):
        status = {"channel":-1, "fault":0, "address":b'\x00'*8}
        with self.comm as ser:
            ser.write(bytes("AST{}\n".format(ch),"utf-8"))
            hdr = ser.read(size=1)
            err_bit = self._validate_resp_hdr(hdr, ch, self.ResponseType.STATUS_T)

            if err_bit:
                body = ser.read(size=1)
                if struct.unpack('>B',body)[0] != 0xFF:
                    raise BadPacket("Temperature status packet with error bit set contains bad body value")
            else:
                body = ser.read(size=10)
                status["channel"] = body[0]
                status["fault"] = body[1]
                status["address"] = body[2:]
        return status

    def sensor_status_P(self, ch):
        status = {"channel":-1, "ai":[0,0,0]}
        with self.comm as ser:
            ser.write(bytes("ASP{}\n".format(ch),"utf-8"))
            hdr = ser.read(size=1)
            err_bit = self._validate_resp_hdr(hdr, ch, self.ResponseType.STATUS_P)

            if err_bit:
                raise BadHeader("Unexpected error bit set in pressure status packet")
            else:
                status["channel"]=ser.read(size=1)[0]
                body = ser.read(size=12)
                status["ai"] = [struct.unpack('>f',body[4*i:4*(i+1)])[0] for i in range(3)]
        return status

    def run(self, num_samples):
        data = []
        with self.comm as ser:
            ser.write(bytes("R","utf-8"))
            ser.write(struct.pack('<I',num_samples))
            while len(data) <= num_samples:
                hdr = ser.read(size=1)
                if (hdr[0] & 0xC0) >> 6 == self.PacketType.DATA:
                    if hdr[0] & 0x3F != 55: # byte count
                        raise BadHeader("Unexpected byte count (data): 0x{:x}".format(hdr[0]))
                elif (hdr[0] & 0xC0) >> 6 == self.PacketType.HALT:
                    count = struct.unpack('>I',ser.read(size=4))[0]
                    return (count, data)
                else:
                    raise BadHeader("Unexpected header type (data): 0x{:x}".format(hdr[0]))
                
                timestamp = struct.unpack('>I',ser.read(size=4))[0]

                active_T = [False]*4
                fault_T = [0]*4
                temperature = [0]*4
                ref_temperature = [0]*4
                pressure = [0]*4

                t_hdr = ser.read(size=1)
                for ich in range(4):
                    val = ser.read(size=4)
                    if t_hdr[0] & (1 << (ich+4)):   # active
                        active_T[ich] = True
                        if t_hdr[0] & (1 << ich):   # error bit
                            fault_T[ich] = struct.unpack('>I',val)[0]
                        else: 
                            temperature[ich] = struct.unpack('>f',val)[0]
                
                p_hdr = ser.read(size=1)
                for ich in range(4):
                    val = ser.read(size=4)
                    if p_hdr[0] & (1 << (ich+4)):   # active
                        pressure[ich] = struct.unpack('>f',val)[0]

                tr_hdr = ser.read(size=1)
                for ich in range(4):
                    val = ser.read(size=4)
                    if tr_hdr[0] & (1 << (ich+4)):   # active
                        ref_temperature[ich] = struct.unpack('>f',val)[0]

                data.append([timestamp, active_T, fault_T, temperature, ref_temperature, pressure])

        return (-1, data)

    def set_debug_level(self, lvl):
        ilvl = int(lvl)
        if ilvl < 0 or ilvl > 2:
            raise ValueError("Debug level out of range")
        with self.comm as ser:
            ser.write(bytes("CD",'utf-8'))
            ser.write(struct.pack('<b',ilvl))

    def set_P_poly_coeffs(self, ch, ai):
        ich = self._validate_ch(ch)
        if len(ai) > 3:
            raise ValueError("Polynomial coefficient array size exceeded")
        with self.comm as ser:
            for ii, a in enumerate(ai):
                ser.write(bytes("CP","utf-8"))
                ser.write(struct.pack('<b',ich))
                ser.write(struct.pack('<b',ii))
                ser.write(struct.pack('<f',a))
    
    def set_board_id(self, board_id):
        with self.comm as ser:
            ser.write(bytes("CB","utf-8"))
            ser.write(struct.pack('<I',board_id))

    def store_board_config(self, confirm):
        if not confirm:
            raise ValueError("Confirmation must be supplied to write board config to flash")
        with self.comm as ser:
            ser.write(bytes("CW","utf-8"))

    def _ask_resp(self,lbl,ch,resp_type):
        with self.comm as ser:
            ser.write(bytes("A{}{}\n".format(lbl,ch),'utf-8'))  
            hdr = ser.read(size=1)
            err_bit = self._validate_resp_hdr(hdr, ch, resp_type)
            buf = ser.read(size=4)
            if err_bit:   
                return (-9999.9, struct.unpack('>I',buf)[0])
            else:
                return (struct.unpack('>f',buf)[0], 0)

    def _validate_resp_hdr(self, hdr, ch, resp_type):
        if (hdr[0] & 0xC0) >> 6 != self.PacketType.RESP:
            raise BadHeader("Unexpected header type: 0x{:x}".format(hdr[0]))
        if (hdr[0] & 0x38) >> 3 != resp_type:
            raise BadHeader("Unexpected response type: 0x{:x}".format(hdr[0]))
        if (hdr[0] & 0x06) >> 1 != ch:
            raise BadHeader("Channel {} mismatch): 0x{:x}".format(ch, hdr[0]))

        return (hdr[0] & 0x01) != 0

    def _validate_ch(self, ch):
        ich = int(ch)
        if ich < 0 or ich > 3:
            raise ValueError("Channel ID out of range")
        return ich
