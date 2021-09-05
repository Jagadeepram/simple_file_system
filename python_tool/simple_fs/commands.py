

from abc import ABCMeta
import enum


class CommandBase(enum.IntEnum):
    __metaclass__ = ABCMeta


class Command(CommandBase):

    CMD_DEFAULT = 0x0
    CMD_UART_TRANSFER = 0x1
    CMD_DEVICE_RESTART = 0x2

    """Command to write raw data into ext mem """
    COMMAND_EXT_MEM_WRITE = 0x0010
    """Command to read raw data into ext mem """
    COMMAND_EXT_MEM_READ = 0x0011
    """ External Memory page erase """
    COMMAND_EXT_MEM_PAGE_ERASE = 0x0012
    """ External Memory chip erase """
    COMMAND_EXT_MEM_CHIP_ERASE = 0x0013

    """ External Memory Write """
    COMMAND_SFS_WRITE = 0x0101
    """ External Memory Read """
    COMMAND_SFS_READ = 0x0100
    """ External Memory Write in parts """
    COMMAND_SFS_READ_IN_PARTS = 0x0102
    """ External Memory Read in parts """
    COMMAND_SFS_WRITE_IN_PARTS = 0x0103
    """ Last written file info """
    COMMAND_SFS_LAST_WRITTEN = 0x0104

