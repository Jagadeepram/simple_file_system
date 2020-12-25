# Copyright (c) 2020 Essity AB
#
# All rights are reserved.
# Proprietary and confidential.
# Unauthorized copying of this file, via any medium is strictly prohibited.
# Any use is subject to an appropriate license granted by Essity AB

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

FILE = 0
FOLDER = 1

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

        nbr_test = 5
        for i in range(nbr_test):

            test_data_len = 50
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

