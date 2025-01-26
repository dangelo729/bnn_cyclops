import serial
import struct
import base64
import os
import oyaml as yaml
import time

class InterfaceError(Exception): pass
class BadAckError(InterfaceError): pass
class BadDataError(InterfaceError): pass
class ExhaustedRetriesError(InterfaceError): pass
class InterfaceTimeoutError(InterfaceError): pass
class AckTimeoutError(InterfaceTimeoutError): pass
class DataTimeoutError(InterfaceTimeoutError): pass

class Interface(serial.Serial):
    def __init__(self, tries=3, plaintext_callback=None, **kwds):
        self._tries = tries
        self._plaintext_callback = plaintext_callback
        super().__init__(**kwds)

    def reset_interface(self):
        self.write(self.RESET_SEQUENCE)
        try:
            self._get_ack()
        except InterfaceError:
            pass
        self.reset_input_buffer()

    def _get_ack(self):
        try:
            reply = self._get_message()
        except InterfaceTimeoutError:
            raise AckTimeoutError()
        if reply == self.ACK:
            return True
        elif reply == self.NAK:
            return False
        else:
            raise BadAckError()

    def _get_message(self):
        try:
            while True:
                line = b''
                while not line.endswith(b'\n'):
                    line += bytes([self.read(1)[0]])
                line = line.rstrip()
                if line.startswith(self.HEADER):
                    break
                if self._plaintext_callback is not None:
                    self._plaintext_callback(line.decode('ascii'))
            return line[1:]
        except IndexError:
            raise DataTimeoutError()
        except UnicodeDecodeError:
            raise BadDataError(line)

    def _get_packet(self):
        message = self._get_message()
        try:
            assert len(message) > 0, message
            message = base64.a85decode(message)
            size = message[0]
            checksum = message[1]
            data = message[2:]
            assert len(data) == size, (size, len(data), data)
            assert (sum(data) & 0xFF) == checksum, (checksum, sum(data) & 0xFF, data)
            return data
        except (IndexError, AssertionError):
            raise BadDataError()

    def _send_raw(self, data):
        tries = self._tries
        while tries > 0:
            self.write(data)
            try:
                if self._get_ack():
                    return
            except InterfaceError:
                self.reset_interface()
            tries -= 1
        raise ExhaustedRetriesError()

    def _send_packet(self, data):
        data = struct.pack('BB', len(data), sum(data) & 0xFF) + data
        data = base64.a85encode(data, pad=False)
        data = b'\xff' + data + b'\n'
        return self._send_raw(data)

class PacketStructure(dict):
    def __init__(self, *args, **kwds):
        super().__init__(*args, **kwds)
        self._format = '<'
        self._bitfields = dict()
        for (field, fmt) in self.items():
            if isinstance(fmt, dict):
                # A subobject specifies packed booleans, but the packed
                # format may vary.
                assert field not in self._bitfields, field
                self._format += fmt['format']
                self._bitfields[field] = tuple(fmt['fields'])
            else:
                assert fmt in 'xcbB?hHiIlLqQefdP', fmt
                self._format += fmt
        self._size = struct.calcsize(self._format)
        self.parse(b'\x00')

    def parse(self, packed_data):
        try:
            packed_data += b'\x00' * (self._size - len(packed_data))
            values = struct.unpack_from(self._format, packed_data)
        except struct.error:
            raise BadDataError()
        else:
            self.update(zip(self.keys(), values))
            for (name, keys) in self._bitfields.items():
                values = ((self[name] & (1 << bit)) != 0 for bit in range(len(keys)))
                self.update(zip(keys, values))

class Monitor(Interface):
    HEADER = b'\xff'
    ACK = b'ack'
    NAK = b'nak'
    RESET_SEQUENCE = b'\n'

    COMMAND_PING = b'p'
    COMMAND_RESET = b'r'
    COMMAND_QUERY = b'q'
    COMMAND_STANDBY = b's'
    COMMAND_ERASE = b'e'
    COMMAND_WATCHDOG = b'w'

    def __init__(self, *args, **kwds):
        super().__init__(*args, **kwds)
        self._load_structures()

    def _load_structures(self):
        dirname = os.path.dirname(os.path.realpath(__file__))
        with open(os.path.join(dirname, 'packet.yaml')) as file:
            structure = yaml.load(file, Loader=yaml.FullLoader)
            self._device_state = PacketStructure(structure['device_state'])

    def _send_command(self, data):
        self._send_packet(data)

    def query(self):
        self._send_command(self.COMMAND_QUERY)
        data = self._get_packet()
        self._device_state.parse(data)
        return self._device_state

    def ping(self):
        self._send_command(self.COMMAND_PING)

    def reset(self):
        self._send_command(self.COMMAND_RESET)

    def standby(self):
        self._send_command(self.COMMAND_STANDBY)

    def erase(self):
        self._send_command(self.COMMAND_ERASE)

    def watchdog(self):
        self._send_command(self.COMMAND_WATCHDOG)
