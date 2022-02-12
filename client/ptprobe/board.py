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
        """Construct a Controller with a specified port

        :param port: The serial port
        :type port: str
        :param baudrate: The baudrate for the serial connection 
            (default 115200 specified in firmware)
        :type baudrate: int
        """
        self.comm = serial.Serial()
        self.comm.port = port
        self.comm.baudrate = baudrate
    
    def board_id(self):
        """Get the ID of the connected board

        :returns: integer board ID
        """
        with self.comm as ser:
            ser.write(bytes('AB\n','utf-8'))    # ask board ID
            hdr = ser.read(size=1)
            err_bit = self._validate_resp_hdr(hdr, 0, self.ResponseType.ID)
            if not err_bit:
                return struct.unpack('>I',ser.read(size=4))[0]
        return 0

    def temperature(self, ch):
        """Request a one-shot temperature sample on the specified channel

        :param ch: The channel (0-3)
        :type ch: int
        :returns: The temperature (C)
        """
        return self._ask_resp('T',ch,self.ResponseType.T)

    def pressure(self, ch):
        """Request a one-shot pressure sample on the specified channel

        :param ch: The channel (0-3)
        :type ch: int
        :returns: The pressure (units defined by polynomial coefficients)
        """
        return self._ask_resp('P',ch,self.ResponseType.P)

    def ref_temperature(self, ch):
        """Request a one-shot cold-junction reference temperature 
            sample on the specified channel

        :param ch: The channel (0-3)
        :type ch: int
        :returns: The cold-junction reference temperature (C) as measured by
            the IC on the board surface
        """
        return self._ask_resp('R',ch,self.ResponseType.TREF)
    
    def raw_adc(self, ch):
        """Request a one-shot pressure sample on the specified channel (raw value)

        :param ch: The channel (0-3)
        :type ch: int
        :returns: The raw value from the ADC scaled from 0 to 1
        """
        return self._ask_resp('A',ch,self.ResponseType.ADC)

    def sensor_status_T(self, ch):
        """Request a thermocouple sensor status report

        :param ch: The channel (0-3)
        :type ch: int
        :returns: The thermocouple sensor IC status as a map including
            - 'channel': the channel ID (matching the input parameter 'ch')
            - 'fault': integer fault code
            - 'address': 8-byte ROM address
        """
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
        """Request a pressure sensor ADC status report


        :param ch: The channel (0-3)
        :type ch: int
        :returns: The pressure sensor ADC status as a map including
            - 'channel': the channel ID (matching the input parameter 'ch')
            - 'ai': the polynomial coefficients for the conversion formula
                P = a0 + a1*x + a2*x^2 where x is the raw ADC value.
        """
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
        """Start the free-running collection of temperature and pressure samples

        :param num_samples: The maximum number of samples to collect. The sample
            rate for the board is approximately 5Hz.
        :type num_samples: int
        :returns: A tuple (number of samples, sample data)

        The sample data is stored as a list. Each entry in the list is composed of
            - a timestamp (ms)
            - active flag for each thermocouple (boolean*4)
            - fault code for each thermocouple (int*4)
            - thermocouple temperature by channel (float*4), 0 if fault code is set
            - cold-junction reference temperature by channel (float*4)
            - converted pressure by channel (float*4)
        """
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
        """Set the board debug level.

        :param lvl: The debug level, 0=off, 1=on (default 0)
        :type lvl: int
        
        .. warning::
            Debug levels > 0 will enable string messages sent over the serial port.
        """
        ilvl = int(lvl)
        if ilvl < 0 or ilvl > 2:
            raise ValueError("Debug level out of range")
        with self.comm as ser:
            ser.write(bytes("CD",'utf-8'))
            ser.write(struct.pack('<b',ilvl))

    def set_P_poly_coeffs(self, ch, ai):
        """Set the polynomial coefficients for the pressure conversion on a channel

        :param ch: The channel ID (0-3)
        :type ch: int
        :param ai: The list of polynomial coefficients [a0, a1, a2]
        :type ai: list

        The pressure conversion is calculated as P = a0 + x*(a1 + x*a2) where
        'x' is the raw ADC value.

        .. note::
            Use the :py:meth:`store_board_config` method to save these coefficients
            to Flash storage and reload them if the board is reset.
        """
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
        """Set the board identifier

        :param board_id: The integer ID
        :type board_id: int

        .. note::
            Use the :py:meth:`store_board_config` method to save the board ID
            to Flash storage such that it will persist if the board is reset.
        """
        with self.comm as ser:
            ser.write(bytes("CB","utf-8"))
            ser.write(struct.pack('<I',board_id))

    def store_board_config(self, confirm):
        """Store the current configuration (board ID, debug level, polynomial coefficients) to Flash

        :param confirm: boolean flag to indicate confirmation. Flash will not be 
            written without this flag set.
        :type confirm: boolean

        .. warning::
             Excessive writes to Flash may result in loss of storage functionality.
             Board configuration should only be written during calibration.
        """
        if not confirm:
            raise ValueError("Confirmation must be supplied to write board config to flash")
        with self.comm as ser:
            ser.write(bytes("CW","utf-8"))

    def _ask_resp(self,lbl,ch,resp_type):
        """[Internal] Send a packet asking for a response (T, ref T, P, etc.)

        :param lbl: The resposne type label ('T'emperature, 'R'ef temperature,
            'A'DC value, 'P'ressure value)
        :type lbl: str
        :param ch: The channel (0-3)
        :type ch: int
        :param resp_type: The response type code from :py:class:`ResponseType`
        :type resp_type: :py:class:`ResponseType` value
        :returns: a tuple (value, error code). The value will be -9999.9 if
            the error code is non-zero
        """
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
        """[Internal] Validate the header byte for a response packet

        :param hdr: The header byte
        :type hdr: byte
        :param ch: The channel ID
        :type ch: int
        :param resp_type: The response type code from :py:class:`ResponseType`
        :type resp_type: :py:class:`ResponseType` value
        :raises BadHeader: If the header contains an unexpected pattern
        :returns: an error flag, True if the error bit is set in the header
        """
        if (hdr[0] & 0xC0) >> 6 != self.PacketType.RESP:
            raise BadHeader("Unexpected header type: 0x{:x}".format(hdr[0]))
        if (hdr[0] & 0x38) >> 3 != resp_type:
            raise BadHeader("Unexpected response type: 0x{:x}".format(hdr[0]))
        if (hdr[0] & 0x06) >> 1 != ch:
            raise BadHeader("Channel {} mismatch): 0x{:x}".format(ch, hdr[0]))

        return (hdr[0] & 0x01) != 0

    def _validate_ch(self, ch):
        """[Internal] Check for channel ID in range

        :param ch: The channel ID
        :type ch: int
        :raises ValueError: if the channel ID is out of range (0-3)
        :returns: an integer channel ID
        """

        ich = int(ch)
        if ich < 0 or ich > 3:
            raise ValueError("Channel ID out of range")
        return ich
