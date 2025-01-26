#!/usr/bin/env python3

import sys
import serial

def get_device(port):
    platform = sys.platform.lower()
    if platform.startswith('win32'):
        from serial.tools.list_ports_windows import comports
        serial_port = 'COM'
    elif platform.startswith('linux'):
        from serial.tools.list_ports_linux import comports
        serial_port = '/dev/ttyUSB'
    elif platform.startswith('darwin'):
        from serial.tools.list_ports_osx import comports
        serial_port = '/dev/ttyUSB'
    else:
        raise ImportError('Unsupported platform: ' + platform)

    ports = sorted(comports())
    if port.startswith(serial_port):
        # Looks like the full name of the serial device.
        serial_port = port
    elif port.isnumeric():
        # Looks like only the serial device number.
        serial_port += port
    else:
        serial_port = port
        for p in ports:
            if port in p.description:
                # Looks like part of the device description.
                serial_port = p.device
                break

    if serial_port in [p.device for p in ports]:
        return serial_port
    else:
        return None
