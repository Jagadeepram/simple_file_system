# Copyright (c) 2020 Essity AB
#
# All rights are reserved.
# Proprietary and confidential.
# Unauthorized copying of this file, via any medium is strictly prohibited.
# Any use is subject to an appropriate license granted by Essity AB

import serial
import struct
import sys
import time
import math

from .commands import Command

DEBUG = 1

""" None: for infinite wait """
TIMEOUT = None

""" Do not change the comport speed (115200) """
COMSPEED = 115200

""" Wait time between responses in seconds """
TIME_BTN_RESP = 0.25

STX = 0x2
ETX = 0x3
DLE = 0x4

"Concat value of STX and ETX according to little endian"
STX_ETX = 0x0302

MESSAGE_ID = 0

MESSAGE_MASK = 0xFFFF

MEAS_FLAG_LEN = 4
MEAS_FLAG_MASK = 0xF
FOTA_COUNTER_LEN = 9
FOTA_COUNTER_MASK = 0x1FF


class Cmd_Data:

    crc: int = 0
    msg_id: int = 0
    cmd: Command = Command.CMD_DEFAULT
    nbr_arg: int = 0
    arg: list = []
    paylen: int = 0
    payload: list = []
    
    def clear(self):
        self.crc = 0
        self.msg_id = 0
        self.cmd = Command.CMD_DEFAULT
        self.nbr_arg = 0
        self.arg = []
        self.paylen = 0
        self.payload = []


def tohex(val, nbits):
    return ((val + (1 << nbits)) % (1 << nbits))


def data_reader(transport):
    while (1):
        transport.read_response_data()
        time.sleep(0.1)


class Transport(object):
    '''
    classdocs
    '''

    def __init__(self, port, logger):
        '''
        Constructor
        '''
        self.ack_list = []
        self.resp_list = []
        self.logger = logger
        self.serial = None
        self.port = port
        self.open()

    def open(self):
        try:
            ser = serial.Serial(
                port=self.port,
                baudrate=COMSPEED,
                timeout=TIMEOUT,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                bytesize=serial.SEVENBITS,
                rtscts=True
            )
            ser.isOpen()
            self.serial = ser
        except  (serial.SerialException):
            print("Comport %s not found" % self.port)
            sys.exit()

    def close(self):
        try:
            self.serial.close()
            self.serial = None
        except:
            None

    def get_port(self):
        return self.port

    def reset_buffers(self):
        self.serial.reset_input_buffer()
        self.serial.reset_output_buffer()

    def display_cmd(self, cmd_data, caller_info):
        if (DEBUG == 1):
            if (caller_info == "RX" and cmd_data.msg_id != 0):
                self.logger.info("%s:MsgID %d Rsp 0x%.04X nrb_arg %d paylen %d" % (caller_info, cmd_data.msg_id, cmd_data.cmd, cmd_data.nbr_arg, cmd_data.paylen))
            else:
                self.logger.info("%s:MsgID %d Cmd 0x%.04X nrb_arg %d paylen %d" % (caller_info, cmd_data.msg_id, cmd_data.cmd, cmd_data.nbr_arg, cmd_data.paylen))

            for i in range (cmd_data.nbr_arg):
                self.logger.info("    Arg[%d] %d" % (i, cmd_data.arg[i]))

            if (cmd_data.paylen):
                payload = [hex(i) for i in cmd_data.payload]
                self.logger.info("Payload: " + '{}'.format(','.join(x for x in payload)))

    def crc16(self, p_data):
        
        crc = 0xFFFF
        data_len = len(p_data)
        
        for i in range(data_len):
            crc = (0xFF) & (crc >> 8) | ((0xFFFF) & (crc << 8))
            crc ^= p_data[i]
            crc ^= (0xFF) & (crc & 0xFF) >> 4
            crc ^= (0xFFFF) & ((0xFFFF) & ((0xFFFF) & (crc << 8)) << 4)
            crc ^= (0xFFFF) & (((crc & 0xFF) << 4) << 1)

        return crc

    def read_response_data(self):

        cmd_data = Cmd_Data()
        
        data = bytes()
        
        negate_byte = 0
        
        while(1):
            try:
                temp = self.serial.read(1)
                temp_b = bytes(temp)[0]
                temp_n_b = ~temp_b
                temp_n_b = tohex(temp_n_b, 8)

                if ((temp_b == STX) and (negate_byte == 0)):
                    negate_byte = 0
                elif ((temp_b == ETX) and (negate_byte == 0)):
                    break
                elif ((temp_b == DLE) and (negate_byte == 0)):
                    negate_byte = 1
                else:
                    if (negate_byte == 1):
                        data += temp_n_b.to_bytes(1, 'little')
                        negate_byte = 0
                    else:
                        data += temp_b.to_bytes(1, 'little')

            except:
                "Close application and RX thread """
                exit()

        if len(data):

            offset = 0
            cmd_data.crc = struct.unpack('<H', (data[offset:offset + 2]))[0]
            offset += 2
            
            computed_crc = self.crc16(data[offset:])
            
            if (computed_crc != cmd_data.crc):
                print("CRC MISMATCH in received packet")
                self.logger.info("CRC MISMATCH in received packet")

            cmd_data.msg_id = struct.unpack('<H', (data[offset:offset + 2]))[0]
            offset += 2

            cmd_data.cmd = struct.unpack('<H', (data[offset:offset + 2]))[0]
            offset += 2

            cmd_data.nbr_arg = struct.unpack('<H', (data[offset:offset + 2]))[0]
            offset += 2

            cmd_data.arg = []
            for __ in range(cmd_data.nbr_arg):
                cmd_data.arg.append(struct.unpack('<I', (data[offset:offset + 4]))[0])
                offset += 4

            cmd_data.paylen = len(data[offset:])

            if (cmd_data.paylen != 0):
                cmd_data.payload = (data[offset:])

            if (cmd_data.cmd == Command.CMD_ADVERTISEMENT_PKTS):
                return cmd_data
            elif (cmd_data.cmd == Command.CMD_NRF_RESET_REASON):
                self.logger.info("Reset reason: %d", struct.unpack('<I', (cmd_data.payload[0: cmd_data.paylen]))[0])
            else:
                self.display_cmd(cmd_data, "RX")
                self.resp_list.append(cmd_data)
                return None
        else:
            """ Ack for alert packet received """
            self.ack_list.append(STX_ETX)
            return None

    def clear_response_list(self):
        self.resp_list.clear()

    def read_response(self, msg_id=0, cmd_id=0, timeout_s=0):

        start_time = time.time()

        while (1):
            for response in self.resp_list:
                if (msg_id != 0 and response.msg_id == msg_id):
                    cmd_data = response
                    self.resp_list.remove(response)
                    return cmd_data
                elif (response.cmd == cmd_id):
                    cmd_data = response
                    self.resp_list.remove(response)
                    return cmd_data

            time.sleep(TIME_BTN_RESP)

            if ((timeout_s != 0) and ((time.time() - start_time) > timeout_s)) :
                return None

    def write_cmd(self, cmd):

        global MESSAGE_ID
        MESSAGE_ID = (MESSAGE_MASK & (MESSAGE_ID + 1)) 
        MESSAGE_ID = MESSAGE_ID if (MESSAGE_ID) else 1

        cmd.msg_id = MESSAGE_ID
        msg_data = struct.pack('<H', MESSAGE_ID)
        msg_data += struct.pack('<H', cmd.cmd)

        cmd.nbr_arg = len(cmd.arg)
        msg_data += struct.pack('<H', cmd.nbr_arg)
        for i in cmd.arg:
            msg_data += struct.pack('<I', i)

        cmd.paylen = len(cmd.payload)

        try:
            msg_data += bytearray(cmd.payload)
        except TypeError:
            msg_data += bytearray(cmd.payload.encode('UTF-8'))

        data = struct.pack('<H', self.crc16(msg_data))
        data += msg_data

        if (DEBUG == 1):
            self.display_cmd(cmd, "TX")

        msg = struct.pack('<B', STX)
        self.serial.write(msg)

        for i in data:

            if ((i == STX) or (i == ETX) or (i == DLE)):
                msg = struct.pack('<B', DLE)
                self.serial.write(msg)
                temp = ~i
                temp = tohex(temp, 8)
                msg = struct.pack('<B', temp)
                self.serial.write(msg)
            else:
                msg = struct.pack('<B', i)
                self.serial.write(msg)

        msg = struct.pack('<B', ETX)
        self.serial.write(msg)

        return MESSAGE_ID
