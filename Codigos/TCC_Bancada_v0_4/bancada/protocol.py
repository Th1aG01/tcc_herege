"""Protocolo de aquisição bruta; independente de Tk, serial e calibração."""
from dataclasses import dataclass, replace
import binascii
import re
import json

SESSION = re.compile(r"[0-9a-f]{16}\Z")
BRAVO = re.compile(r"Received packet '(.*)' with RSSI (-?\d+)\Z")

@dataclass(frozen=True)
class Packet:
    session: str
    packet_seq: int | None
    seq: int
    t_us: int
    raw: int
    flag: str = 'OK'
    rssi: int | None = None

    def validate(self):
        if not SESSION.fullmatch(self.session):
            raise ValueError('Identificador de ensaio inválido.')
        if self.packet_seq is not None and not 0 <= self.packet_seq <= 0xffffffff:
            raise ValueError('Número do pacote inválido.')
        if not 0 <= self.seq <= 0xffffffff or not 0 <= self.t_us < 2**63:
            raise ValueError('Sequência ou tempo inválido.')
        if not -(2**23) <= self.raw < 2**23:
            raise ValueError('Valor fora dos 24 bits do HX711.')
        expected = 'SAT' if self.raw in (-8388608, 8388607) else 'OK'
        if self.flag != expected:
            raise ValueError('Indicador de saturação inconsistente.')
        if self.rssi is not None and not -200 <= self.rssi <= 20:
            raise ValueError('RSSI inválido.')

def encode(packet):
    packet.validate()
    number = '-' if packet.packet_seq is None else str(packet.packet_seq)
    body = f'TCC2;{packet.session};{number};{packet.seq};{packet.t_us};{packet.raw};{packet.flag}'
    return f'{body};{binascii.crc_hqx(body.encode("ascii"), 0xffff):04X}'

def parse_line(line):
    """Aceita a linha USB direta ou o invólucro EXATO do receptor BRAVO.

    None identifica mensagens de diagnóstico, não uma amostra de valor zero.
    ValueError identifica um pacote que se anuncia TCC2 mas é inválido.
    """
    line = line.strip()
    rssi = None
    if line.startswith("Received packet '"):
        match = BRAVO.fullmatch(line)
        if not match:
            raise ValueError('Linha do receptor BRAVO incompleta.')
        line, rssi = match.group(1), int(match.group(2))
    if line.startswith('{'):
        try:
            obj = json.loads(line)
            if set(obj) != {'v', 's', 'p', 'n', 't', 'r', 'f', 'c'} or obj['v'] != 4:
                raise ValueError('Payload JSON incompatível.')
            for key in ('n', 't', 'r'):
                if type(obj[key]) is not int: raise ValueError('Campo inteiro inválido.')
            if obj['p'] is not None and type(obj['p']) is not int:
                raise ValueError('Contador inválido.')
            number = '-' if obj['p'] is None else str(obj['p'])
            line = f"TCC2;{obj['s']};{number};{obj['n']};{obj['t']};{obj['r']};{obj['f']};{obj['c']}"
        except (json.JSONDecodeError, TypeError, KeyError) as exc:
            raise ValueError('Payload JSON inválido.') from exc
    if not line.startswith('TCC2;'):
        return None
    parts = line.split(';')
    if len(parts) != 8 or not re.fullmatch(r'[0-9A-Fa-f]{4}', parts[-1]):
        raise ValueError('Pacote TCC2 incompleto.')
    body = ';'.join(parts[:-1])
    try:
        crc = binascii.crc_hqx(body.encode('ascii'), 0xffff)
    except UnicodeEncodeError as exc:
        raise ValueError('Pacote não ASCII.') from exc
    if crc != int(parts[-1], 16):
        raise ValueError('CRC inválido: pacote descartado, sem inventar amostra.')
    packet = Packet(parts[1], None if parts[2] == '-' else int(parts[2]),
                    int(parts[3]), int(parts[4]), int(parts[5]), parts[6], rssi)
    packet.validate()
    if rssi is not None and packet.packet_seq is None:
        raise ValueError('Pacote de rádio sem contador de transmissão.')
    return packet

class Trace:
    """Mantém apenas amostras válidas, com contadores separados para cada caminho."""
    def __init__(self, source):
        self.source = source
        self.samples = []
        self.by_seq = {}
        self.gaps = 0
        self.duplicates = 0
        self.out_of_order = 0
        self.saturated = 0

    def add(self, packet):
        packet.validate()
        if (packet.packet_seq is None) != (self.source == 'USB'):
            raise ValueError('Pacote recebido pela porta errada.')
        previous = self.by_seq.get(packet.seq)
        if previous:
            if (previous.t_us, previous.raw, previous.flag) != (packet.t_us, packet.raw, packet.flag):
                raise ValueError('A mesma amostra chegou com valores diferentes.')
            self.duplicates += 1
            return False
        index = packet.seq if self.source == 'USB' else packet.packet_seq
        last = self.samples[-1] if self.samples else None
        last_index = (last.seq if self.source == 'USB' else last.packet_seq) if last else -1
        if last and (packet.seq <= last.seq or packet.t_us <= last.t_us or index <= last_index):
            self.out_of_order += 1
            return False
        self.gaps += index - last_index - 1
        self.samples.append(packet)
        self.by_seq[packet.seq] = packet
        self.saturated += packet.flag == 'SAT'
        return True

    def stats(self):
        return dict(count=len(self.samples), gaps=self.gaps, duplicates=self.duplicates,
                    out_of_order=self.out_of_order, saturated=self.saturated)

class Acquisition:
    def __init__(self, session):
        if not SESSION.fullmatch(session):
            raise ValueError('Identificador inválido.')
        self.session = session
        self.traces = {'USB': Trace('USB'), 'LORA': Trace('LORA')}
        self.foreign = 0
        self.verified = set()
        self.mismatches = set()

    def add(self, source, packet):
        if packet.session != self.session:
            self.foreign += 1
            return False
        if not self.traces[source].add(packet):
            return False
        usb = self.traces['USB'].by_seq.get(packet.seq)
        radio = self.traces['LORA'].by_seq.get(packet.seq)
        if usb and radio:
            if (usb.t_us, usb.raw, usb.flag) == (radio.t_us, radio.raw, radio.flag):
                self.verified.add(packet.seq)
            else:
                self.mismatches.add(packet.seq)
        return True

    def stats(self):
        return dict(usb=self.traces['USB'].stats(), lora=self.traces['LORA'].stats(),
                    verified=len(self.verified), mismatches=len(self.mismatches), foreign=self.foreign)
