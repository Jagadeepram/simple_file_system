
from .transport_layer import Cmd_Data
from .transport_layer import Transport
from .transport_layer import data_reader
from .commands import Command
from subprocess import call
import random
import json
import time 
import os, fnmatch
import threading
import logging
import zipfile
import shutil
import struct
from tkinter import *
from tkinter import scrolledtext

""" File to log debug information """
DEBUG_FILE = 'debug_log.txt'


def setup_logger(name, log_file, level=logging.INFO):

    """To setup many loggers"""
    formatter = logging.Formatter('%(asctime)s %(message)s')
    handler = logging.FileHandler(log_file)
    handler.setFormatter(formatter)

    logger = logging.getLogger(name)
    logger.setLevel(level)
    logger.addHandler(handler)

    return logger


class Simple_FS(object):

    def __init__(self, port):

        debug_logger = setup_logger('debug_log', DEBUG_FILE)

        self.cmd_data = Cmd_Data()
        self.transport = Transport(port, debug_logger)
        self.rx_thread = threading.Thread(target=data_reader, args=(self.transport,))
        self.rx_thread.start()

    def stop_data_reception(self):
        try:
            self.rx_thread.daemon = False
        except RuntimeError:
            None

    def close(self):
        self.stop_data_reception()
        self.transport.close()
        print("Closing application")

    def data_transfer_test(self):

        nbr_test = 100
        for i in range(nbr_test):

            test_data_len = 500
            start = time.time()
            self.cmd_data.clear()
            self.cmd_data.cmd = Command.CMD_UART_TRANSFER
            payload = [(random.randint(65, 90)) for __ in range (test_data_len)]
            self.cmd_data.payload = payload
            arg = [100, 200, 100, 200]
            self.cmd_data.arg = arg
            msg_id = self.transport.write_cmd(self.cmd_data)
            read_cmd = self.transport.read_response(msg_id=msg_id)
            stop = time.time()
            temp = read_cmd.payload
            read_cmd.payload = [temp[j] for j in range (read_cmd.paylen)]

            if (read_cmd.payload != payload or read_cmd.arg != arg or read_cmd.msg_id != msg_id):
                if (read_cmd.payload != payload):
                    print("%d:Load mismatch: %d %d" % (i, len(payload), len(read_cmd.payload)))
                elif (read_cmd.arg != arg):
                    print("%d:Arg mismatch: %d %d" % (i, len(arg), len(read_cmd.arg)))
                elif (read_cmd.msg_id != msg_id):
                    print("%d:Message ID mismatch: %d %d" % (i, read_cmd.msg_id, msg_id))
            else:
                print("%d:Test success Round trip: %s seconds" % (i, (stop - start)))

    def test_ext_mem_driver(self):
        mem_address = 0x23000
        test_data_len = 50
#         start = time.time()
        self.cmd_data.clear()
        self.cmd_data.cmd = Command.COMMAND_EXT_MEM_READ
        payload = [(i + 20) for i in range (test_data_len)]
        self.cmd_data.payload = payload
        arg = [mem_address, len(payload)]
        self.cmd_data.arg = arg
        msg_id = self.transport.write_cmd(self.cmd_data)
        read_cmd = self.transport.read_response(msg_id=msg_id)
#         stop = time.time()
        temp = read_cmd.payload
        read_cmd.payload = [temp[j] for j in range (read_cmd.paylen)]
        if (read_cmd.payload != payload):
            """ Data does not exists, write it """

            """ Perform page erase """
            self.cmd_data.cmd = Command.COMMAND_EXT_MEM_PAGE_ERASE
            msg_id = self.transport.write_cmd(self.cmd_data)
            self.transport.read_response(msg_id=msg_id)
            """ Write the data """
            self.cmd_data.cmd = Command.COMMAND_EXT_MEM_WRITE
            msg_id = self.transport.write_cmd(self.cmd_data)
            self.transport.read_response(msg_id=msg_id)
            """ Read it again """
            self.cmd_data.cmd = Command.COMMAND_EXT_MEM_READ
            msg_id = self.transport.write_cmd(self.cmd_data)
            read_cmd = self.transport.read_response(msg_id=msg_id)
            temp = read_cmd.payload
            read_cmd.payload = [temp[j] for j in range (read_cmd.paylen)]
            if (read_cmd.payload == payload):
                print("Data written at 0x%X" % mem_address)
            else:
                print("Data write error ")
                print(read_cmd.payload)
                print(payload)
        else:
            print("Data verified at 0x%X" % mem_address)

    def external_memory_erase(self):
        self.cmd_data.clear()
        self.cmd_data.cmd = Command.COMMAND_EXT_MEM_CHIP_ERASE
        msg_id = self.transport.write_cmd(self.cmd_data)
        self.transport.read_response(msg_id=msg_id)
        print("Memory chip erased")

    def address_check(self):
        self.cmd_data.clear()
        self.cmd_data.cmd = Command.COMMAND_EXT_MEM_READ
        address = 0x2ffdc
        len = 500
        self.cmd_data.arg = [address, len]
        msg_id = self.transport.write_cmd(self.cmd_data)
        read_cmd = self.transport.read_response(msg_id=msg_id)
        temp = read_cmd.payload
        read_cmd.payload = [temp[j] for j in range (read_cmd.paylen)]
        print(read_cmd.payload)

    def file_test(self):
        for i in range (300):
            file_id = 0x10001
            file_len = 2000
            _, w_start_address, w_end_address = self.file_write(file_id, file_len + i)
            #time.sleep(1)
            #print(data)
            _, r_start_address, r_end_address = self.file_read(file_id, file_len + i)
            #print(read_data)
            #time.sleep(1)
            #if (data != read_data):
            #self.address_check()
            if (w_start_address != r_start_address or w_end_address != r_end_address):
                print("0x%x"%w_start_address)
                print("0x%x"%r_start_address)
                print("0x%x"%w_end_address)
                print("0x%x"%r_end_address)
                print("mismatch")
                #print(data)
                #print(read_data)

    def file_read(self, file_id,  file_len):
        self.cmd_data.clear()
        self.cmd_data.cmd = Command.COMMAND_SFS_READ
        self.cmd_data.arg = [file_id]
        msg_id = self.transport.write_cmd(self.cmd_data)
        read_cmd = self.transport.read_response(msg_id=msg_id)
        temp = read_cmd.payload

        if (file_len != read_cmd.paylen):
            print("File len mismatch")

        read_cmd.payload = [temp[j] for j in range (read_cmd.paylen)]
        if (read_cmd.cmd != 0):
            print("File not found " + str(read_cmd.cmd))

        return read_cmd.payload, read_cmd.arg[1], read_cmd.arg[5]

    def file_write(self, file_id, file_len):
        self.cmd_data.clear()
        self.cmd_data.cmd = Command.COMMAND_SFS_WRITE
        self.cmd_data.payload = [(random.randint(65, 90)) for __ in range (file_len)]
        self.cmd_data.arg = [file_id]
        msg_id = self.transport.write_cmd(self.cmd_data)
        cmd_data = self.transport.read_response(msg_id=msg_id)
        if (len(cmd_data.arg) == 7):
            print("address 0x%x file ID %d file len %d status 0x%x next 0x%x %dms"%(cmd_data.arg[1], cmd_data.arg[2], cmd_data.arg[3], cmd_data.arg[4], cmd_data.arg[5], cmd_data.arg[6]))
            if (cmd_data.arg[0] != file_id):
                print("File write mismatch W file ID %d R file ID %d"%(file_id, cmd_data.arg[0]))
        else:
            print("Write error")
        temp = cmd_data.payload
        cmd_data.payload = [temp[j] for j in range (cmd_data.paylen)]
        return cmd_data.payload, cmd_data.arg[1], cmd_data.arg[5]

    def file_test_in_parts(self):
        for i in range (300):
            file_id = 0x10001
            file_len = 10000
            data, w_start, w_stop = self.file_write_in_parts(file_id, file_len + i)
            read_data, r_start, r_stop = self.file_read_in_parts(file_id, file_len + i)
            if (data != read_data 
                or w_start != r_start 
                or  w_stop!= r_stop):
                print("mismatch")
                print("0x%x"%w_start)
                print("0x%x"%w_stop)
                print("0x%x"%r_start)
                print("0x%x"%r_stop)
            else:
                print("Test ok File ID %d File len %d start 0x%x stop 0x%x "%(file_id, file_len + i, w_start, w_stop))

    def file_read_in_parts(self, file_id, file_len):
        file = []
        MAX_LEN_PER_TX = 1000
        rem_len = file_len
        while (rem_len > 0):
            if (rem_len >= MAX_LEN_PER_TX):
                data_len = MAX_LEN_PER_TX
            else:
                data_len = rem_len

            self.cmd_data.clear()
            self.cmd_data.cmd = Command.COMMAND_SFS_READ_IN_PARTS
            self.cmd_data.arg = [file_id, rem_len, data_len]
            msg_id = self.transport.write_cmd(self.cmd_data)
            read_cmd = self.transport.read_response(msg_id=msg_id)

            temp = read_cmd.payload
            read_cmd.payload = [temp[j] for j in range (read_cmd.paylen)]

            file += read_cmd.payload
            rem_len = rem_len - data_len
            if (read_cmd.cmd != 0):
                print("File not found " + str(read_cmd.cmd))
                return file

        self.cmd_data.clear()
        self.cmd_data.cmd = Command.COMMAND_SFS_LAST_WRITTEN
        self.cmd_data.arg = [file_id]
        msg_id = self.transport.write_cmd(self.cmd_data)
        resp = self.transport.read_response(msg_id=msg_id)
        return file, resp.arg[1], resp.arg[5]

    def file_write_in_parts(self, file_id, file_len):

        file = [(random.randint(65, 90)) for __ in range (file_len)]
        MAX_LEN_PER_TX = 2000
        rem_len = file_len
        index = 0

        while (rem_len > 0):
            if (rem_len >= MAX_LEN_PER_TX):
                data_len = MAX_LEN_PER_TX
            else:
                data_len = rem_len

            self.cmd_data.clear()
            self.cmd_data.cmd = Command.COMMAND_SFS_WRITE_IN_PARTS
            self.cmd_data.arg = [file_id, rem_len]
            self.cmd_data.payload = file[index: index + data_len]

            msg_id = self.transport.write_cmd(self.cmd_data)
            self.transport.read_response(msg_id=msg_id)

            index = index + data_len
            rem_len = rem_len - data_len
        
        self.cmd_data.clear()
        self.cmd_data.cmd = Command.COMMAND_SFS_LAST_WRITTEN
        self.cmd_data.arg = [file_id]
        msg_id = self.transport.write_cmd(self.cmd_data)
        resp = self.transport.read_response(msg_id=msg_id)
        return file, resp.arg[1], resp.arg[5]

    def GUI_app(self):
        window = Tk()
        window.title("Welcome to LikeGeeks app")
        window.geometry('350x400')
        txt = scrolledtext.ScrolledText(window, width=60, height=60)
        txt.grid(column=0, row=0)

        self.cmd_data.clear()
        self.cmd_data.cmd = Command.COMMAND_EXT_MEM_READ
        mem_address = 0x1000 * 10
        data_len = 0x1000
        self.cmd_data.arg = [mem_address, data_len]
        msg_id = self.transport.write_cmd(self.cmd_data)
        read_cmd = self.transport.read_response(msg_id=msg_id)
        temp = read_cmd.payload
        read_cmd.payload = [hex(temp[j]) for j in range (read_cmd.paylen)]
#         if (read_cmd.cmd == 0):
#             print(read_cmd.payload)
#         else:
#             print("File not found " + str(read_cmd.cmd))
        txt.insert(INSERT, read_cmd.payload)

        window.mainloop()
