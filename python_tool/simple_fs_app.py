# Copyright (c) 2020 Essity AB
#
# All rights are reserved.
# Proprietary and confidential.
# Unauthorized copying of this file, via any medium is strictly prohibited.
# Any use is subject to an appropriate license granted by Essity AB

import os
import argparse
from simple_fs.simple_fs_functions import Simple_FS


def parse_arguments():

        parser = argparse.ArgumentParser(prog='gateway_app.py',
                                         description=('A tool to test UART commands, FOTA and DFU'))

        parser.add_argument('-p', '--port',
                            help='Serial port to read from. gateway_app -p COMXX',
                            action='store',
                            default=None,
                            required=True)

        return parser.parse_args()


def main():

    args = parse_arguments()

    app = Simple_FS(args.port)

    menu_options = {
        'a': ['data_transfer_test', "Uart Protocol tester"],
        '1': ['exit', "Exit"]
    }

    while True:
        os.system("cls")
        print("--------Simple FS Interface--------------")
        print()
        print("Options:")

        for (key, value) in sorted(menu_options.items()):
            print("%s: %s" % (key, value[1]))
        print()

        choice = input("Enter>> ")
        choice.lower()
        print()
        if(choice == '1'):
            app.close()
            exit()
        else:
            try:
                app.transport.clear_response_list()
                getattr(app, menu_options[choice][0])()
            except (AssertionError, KeyError):
                input("Error!")
                continue

        try:
            input("\nEnter to continue")
        except EOFError:
            os.system("cls")


if __name__ == '__main__':
    main()
