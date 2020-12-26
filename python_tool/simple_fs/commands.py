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


class Command(CommandBase):

    CMD_DEFAULT = 0x0
    CMD_UART_TRANSFER = 0x1
    CMD_DEVICE_RESTART = 0x2

    """ External Memory Write """
    COMMAND_EXT_MEM_WRITE = 0x0010
    """ External Memory Read """
    COMMAND_EXT_MEM_READ = 0x0011
    """ External Memory page erase """
    COMMAND_EXT_MEM_PAGE_ERASE = 0x0012
    """ External Memory chip erase """
    COMMAND_EXT_MEM_CHIP_ERASE = 0x0013
