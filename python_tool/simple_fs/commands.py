# Copyright (c) 2020 Essity AB
#
# All rights are reserved.
# Proprietary and confidential.
# Unauthorized copying of this file, via any medium is strictly prohibited.
# Any use is subject to an appropriate license granted by Essity AB

from abc import ABCMeta
import enum


class CommandBase(enum.IntEnum):
    __metaclass__ = ABCMeta


class FotaFile(CommandBase):

    """ No File requested by nRF chip """
    NO_FILE_REQUESTED = 0
    """ Requesting Bootloader and SD DAT file """
    BL_SD_DAT = 1
    """ Requesting Bootloader and SD BIN file """
    BL_SD_BIN = 2
    """ Requesting Application DAT file """
    APP_DAT = 3
    """ Requesting Application BIN file """
    APP_BIN = 4


class FotaStatus(CommandBase):

    """ No transmitter connected and ready to perform FOTA """
    READY = 0
    """ Prepare for FOTA by reading BL_SD_DAT file and its length """
    BL_SD_INIT = 1
    """ Connection requested """
    BL_SD_CONN_REQ = 2
    """ Connection accepted and update in progress """
    BL_SD_PROGRESS = 3
    """ Bootloader and Softdevice update successful """
    BL_SD_SUCCESS = 4
    """ Bootloader and Softdevice update failed """
    BL_SD_FAIL = 5
    """ Prepare for FOTA by reading APP_DAT file and its length """
    APP_INIT = 6
    """ Connection requested """
    APP_CONN_REQ = 7
    """ Connection accepted and update in progress """
    APP_PROGRESS = 8
    """ Application update successful """
    APP_SUCCESS = 9
    """ Application update failed """
    APP_FAIL = 10
    """ Bootloader exists """
    BL_SD_EXISTS = 11
    """ Application exists """
    APP_EXISTS = 12
    """ Connection rejected """
    CONN_REJECTED = 13


class Command(CommandBase):

    CMD_DEFAULT = 0x0
    CMD_UART_TRANSFER = 0x1
    CMD_DEVICE_RESTART = 0x2

    """ Advertisement packets """
    CMD_ADVERTISEMENT_PKTS = 0x2001
    """ Gateway reset reason """
    CMD_NRF_RESET_REASON = 0x2002
    """ Initiate FOTA """
    CMD_INITIATE_FW_TRANSFER = 0x1001
    """ Transfer DAT file """
    CMD_TRANSFER_FW = 0x1002
    """ UPDATE BLE SCAN STATUS """
    CMD_UPDATE_BLE_SCAN_STATUS = 0x1003
    """ Fetch software version number """
    CMD_GET_GATEWAY_INFO = 0x1004
    """ Format External memory """
    CMD_FORMAT_EXT_MEM = 0x1005
    """ Is firmware available """
    CMD_IS_FW_AVAILABLE = 0x1006
    """ Notify Transmitter """
    CMD_NOTIFY_TRANSMITTER = 0x1009
    """ Connection request """
    CMD_CONNECT_TRANSMITTER = 0x100B
    """ Connection status """
    CMD_CONNECTION_STATUS = 0x100C
    """ Command to send list of Firmware stored """
    CMD_SEND_FW_LIST_STORED = 0x100D
    """ Gateway nRF DFU init """
    CMD_NRF_DFU_INIT = 0x3001

