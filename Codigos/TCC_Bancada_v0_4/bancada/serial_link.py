"""Leitura em threads, filas limitadas e comandos apenas pela porta da bancada."""
import queue
import threading
import time
import serial
from serial.tools import list_ports

def ports():
    return [(p.device, p.description) for p in list_ports.comports()]

class SerialLink:
    def __init__(self, port, source, events):
        self.source, self.events = source, events
        self.stop_event = threading.Event()
        self.commands = queue.Queue(maxsize=32)
        self.dropped = 0
        self.port = serial.Serial(port=None, baudrate=115200, timeout=0.1, write_timeout=0.5,
                                  rtscts=False, dsrdtr=False)
        # Evita pedir reset por DTR/RTS; alguns drivers ainda podem pulsar ao abrir.
        self.port.dtr = False
        self.port.rts = False
        self.port.port = port
        self.port.open()
        self.thread = threading.Thread(target=self.run, daemon=True, name=f'serial-{source}')
        self.thread.start()

    def emit(self, kind, value):
        try:
            self.events.put_nowait((self.source, kind, value, time.monotonic()))
        except queue.Full:
            self.dropped += 1

    def send(self, command):
        if self.source != 'USB':
            raise ValueError('A estação BRAVO é somente receptora.')
        if '\n' in command or '\r' in command or len(command) > 100:
            raise ValueError('Comando inválido.')
        if not self.thread.is_alive():
            raise OSError('Porta da bancada desconectada.')
        self.commands.put_nowait(command)

    def run(self):
        pending = bytearray()
        discarding = False
        try:
            while not self.stop_event.is_set():
                try:
                    command = self.commands.get_nowait()
                except queue.Empty:
                    command = None
                if command is not None:
                    data = (command + '\n').encode('ascii')
                    if self.port.write(data) != len(data):
                        raise OSError('Comando serial enviado parcialmente.')
                chunk = self.port.read(min(self.port.in_waiting or 1, 4096))
                for byte in chunk:
                    if byte == 10:
                        if not discarding:
                            try:
                                self.emit('line', pending.rstrip(b'\r').decode('ascii'))
                            except UnicodeDecodeError:
                                self.emit('invalid', 'Linha serial não ASCII (possível mensagem de boot).')
                        pending.clear()
                        discarding = False
                    elif not discarding:
                        pending.append(byte)
                        if len(pending) > 512:
                            pending.clear()
                            discarding = True
                            self.emit('invalid', 'Linha serial maior que 512 bytes; descartada.')
        except (OSError, serial.SerialException) as exc:
            if not self.stop_event.is_set():
                self.emit('error', str(exc))
        finally:
            self.port.close()

    def close(self):
        self.stop_event.set()
        self.thread.join(timeout=1)
