#!/usr/bin/env python3
import argparse
import sys
import interface
import time
import serial
import port
import curses
import os
import oyaml as yaml

MIN_PYTHON = (3, 6)
if sys.version_info < (MIN_PYTHON):
    sys.exit("Python %s.%s or later is required.\n" % MIN_PYTHON)

parser = argparse.ArgumentParser()
parser.add_argument('port', help='Serial port')
args = parser.parse_args()

serial_port = port.get_device(args.port)

if serial_port == None:
    print('Serial port not found: ' + args.port)
    sys.exit()

print('Using serial port:', serial_port)

dut = interface.Monitor(
    baudrate=115200, timeout=0.05, port=serial_port,
    plaintext_callback=lambda x: line_history.insert(0, x))

commands = {
    'q': ('Quit', None),
    'r': ('Reset', lambda: dut.reset()),
    's': ('Standby', lambda: dut.standby()),
    'e': ('Erase', lambda: dut.erase()),
    'w': ('Watchdog', lambda: dut.watchdog()),
    'c': ('Clear log', lambda: line_history.clear()),
}

dirname = os.path.dirname(os.path.realpath(__file__))
with open(os.path.join(dirname, 'ui.yaml')) as file:
    ui_structure = yaml.load(file, Loader=yaml.FullLoader)
num_cols = len(ui_structure['sections'])
num_rows = max((len(fields) for (_, fields) in ui_structure['sections'].items()))

line_history = list()

def main(stdscr):
    curses.noecho()
    curses.cbreak()
    curses.curs_set(False)
    stdscr.nodelay(True)

    while True:
        stdscr.clear()
        scr_height, scr_width = stdscr.getmaxyx()

        def addstr(y, x, string, attr=0):
            if x < scr_width and y < scr_height:
                length = min(len(string), scr_width - x)
                stdscr.addstr(y, x, string[:length], attr)

        try:
            state = dut.query()

            c = 0
            for section, members in ui_structure['sections'].items():
                name_width = 0
                for r, (name, info) in enumerate(members.items()):
                    name_width = max(name_width, len(name))
                col_width = 0
                for r, (name, info) in enumerate(members.items()):
                    value = state[info['field']]
                    fmt = info.get('format', 'd')
                    fmt = '{:<' + str(name_width) + 's} {:' + fmt + '}'
                    text = fmt.format(name, value)
                    col_width = max(col_width, len(text))
                    addstr(r + 1, c, text)
                for r in range(num_rows):
                    addstr(r + 1, c + col_width + 1, ' ', curses.A_REVERSE)
                col_width += 3
                fmt = '{:<' + str(col_width) + 's}'
                addstr(0, c, fmt.format(section), curses.A_REVERSE)
                c += col_width

            command_list = list(commands.items())
            nc = c // 16
            for i in range(0, len(commands), nc):
                row = command_list[i:i+nc]
                addstr(num_rows + 1 + i//nc, 0, ' ' * c, curses.A_REVERSE)
                for n, (key, (command, code)) in enumerate(row):
                    addstr(num_rows + 1 + i//nc, n * 16, '{:<16s}'
                        .format(key + ':' + command), curses.A_REVERSE)

            y_start = num_rows + 2 + (len(commands) - 1) // nc

        except interface.InterfaceError as e:
            addstr(0, 0, 'Connecting...', curses.A_REVERSE)
            y_start = 1
        finally:
            lines = line_history[:scr_height - y_start]
            for y, line in enumerate(reversed(lines), start=y_start):
                addstr(y, 0, line)
            stdscr.refresh()
            time.sleep(0.05)

        try:
            key = stdscr.getkey()
            command, callback = commands[key]
            callback()
        except (curses.error, KeyError):
            pass
        except TypeError:
            break

try:
    curses.wrapper(main)
except serial.SerialException as e:
    pass
except Exception as e:
    raise e
finally:
    dut.close()
