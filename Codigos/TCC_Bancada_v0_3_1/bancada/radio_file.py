"""Transferência de registro completo por rádio unidirecional, em blocos verificados."""
from dataclasses import dataclass
from pathlib import Path
import base64
import binascii
import csv
import json
import math
import os
import re
import time
import zlib
from .protocol import SESSION, BRAVO
from .storage import atomic_json
from .live_storage import load_flash, now

BLOCK_SIZE = 120
MAX_FILE_SIZE = 16 * 1024 * 1024

@dataclass(frozen=True)
class FileFrame:
    session: str
    kind: str
    index: int
    blocks: int
    size: int
    checksum: int
    data: bytes = b''
    rssi: int | None = None

    @property
    def key(self):
        return f'{self.session}_{self.checksum:08x}_{self.size}'

    def validate(self):
        if not SESSION.fullmatch(self.session) or self.kind not in ('S', 'B', 'E'):
            raise ValueError('Identificação de arquivo inválida.')
        if not 1 <= self.size <= MAX_FILE_SIZE or self.blocks != math.ceil(self.size / BLOCK_SIZE):
            raise ValueError('Tamanho ou total de blocos inválido.')
        if not 0 <= self.checksum <= 0xffffffff:
            raise ValueError('CRC32 do arquivo inválido.')
        if self.rssi is not None and not -200 <= self.rssi <= 20:
            raise ValueError('RSSI inválido.')
        if self.kind == 'B':
            if not 0 <= self.index < self.blocks:
                raise ValueError('Índice de bloco inválido.')
            expected = min(BLOCK_SIZE, self.size - self.index * BLOCK_SIZE)
            if len(self.data) != expected:
                raise ValueError('Comprimento do bloco inválido.')
        elif self.index != 0 or self.data:
            raise ValueError('Marcador de transferência inválido.')

def encode_frame(frame):
    """Também usado em testes; formato idêntico ao gerado pelo ESP32."""
    frame.validate()
    encoded = base64.b64encode(frame.data).decode('ascii')
    body = f'TCCF3;{frame.session};{frame.kind};{frame.index};{frame.blocks};{frame.size};{frame.checksum:08X};{encoded}'
    return f'{body};{binascii.crc_hqx(body.encode("ascii"), 0xffff):04X}'

def parse_frame(line):
    line = line.strip()
    rssi = None
    if line.startswith("Received packet '"):
        match = BRAVO.fullmatch(line)
        if not match: raise ValueError('Saída BRAVO incompleta.')
        line, rssi = match.group(1), int(match.group(2))
    if not line.startswith('TCCF3;'): return None
    if len(line) > 255: raise ValueError('Quadro LoRa excede 255 bytes.')
    fields = line.split(';')
    if len(fields) != 9 or not re.fullmatch(r'[0-9a-fA-F]{4}', fields[-1]):
        raise ValueError('Quadro de arquivo incompleto.')
    body = ';'.join(fields[:-1])
    if binascii.crc_hqx(body.encode('ascii'), 0xffff) != int(fields[-1], 16):
        raise ValueError('CRC16 do quadro inválido.')
    if not re.fullmatch(r'[0-9a-fA-F]{8}', fields[6]):
        raise ValueError('CRC32 do arquivo inválido.')
    try: data = base64.b64decode(fields[7], validate=True)
    except binascii.Error as exc: raise ValueError('Conteúdo Base64 inválido.') from exc
    frame = FileFrame(fields[1], fields[2], int(fields[3]), int(fields[4]), int(fields[5]),
                      int(fields[6], 16), data, rssi)
    frame.validate()
    return frame

def file_crc(path):
    crc = 0
    with Path(path).open('rb') as f:
        for block in iter(lambda: f.read(65536), b''): crc = zlib.crc32(block, crc)
    return crc & 0xffffffff

class FileReceiver:
    """Mantém blocos em disco; repetição do mesmo arquivo preenche as lacunas.

    Não envia comandos ou ACK pelo rádio. Não inventa os blocos que faltam.
    """
    def __init__(self, root, frame):
        frame.validate()
        self.identity = (frame.session, frame.blocks, frame.size, frame.checksum)
        self.folder = Path(root) / ('radio_' + frame.key)
        self.folder.mkdir(parents=True, exist_ok=True)
        self.meta_path = self.folder / 'recepcao.json'
        self.partial_path = self.folder / 'registro.log.partial'
        self.path = self.folder / 'registro.log'
        self.csv_path = self.folder / 'amostras.csv'
        self.file = None
        self.received = set()
        self.duplicates = 0
        self.last_checkpoint = 0
        self.complete = False
        self.ended_pass = False
        self.last_rssi = None
        self.record_status = None
        self.validation_error = None
        self.meta = dict(protocol='TCCF3', session_id=frame.session, blocks=frame.blocks,
                         size_bytes=frame.size, crc32=frame.checksum, source='LORA_FILE',
                         calibrated=False, created_at=now(), state='incomplete', received=[])
        if self.meta_path.exists():
            saved = json.loads(self.meta_path.read_text(encoding='utf-8'))
            identity = (saved['session_id'], saved['blocks'], saved['size_bytes'], saved['crc32'])
            if identity != self.identity or saved.get('protocol') != 'TCCF3':
                raise ValueError('Metadados locais não correspondem ao arquivo recebido.')
            self.meta = saved
            self.validation_error = saved.get('validation_error')
            self.received = set(saved.get('received', []))
            if any(type(index) is not int or not 0 <= index < frame.blocks for index in self.received):
                raise ValueError('Índices inválidos no registro de transferência.')
            self.duplicates = int(saved.get('duplicates', 0))
        if self.path.exists():
            if self.path.stat().st_size != frame.size or file_crc(self.path) != frame.checksum:
                raise ValueError('O arquivo concluído existente foi alterado. Preserve-o e selecione outra pasta.')
            self.received = set(range(frame.blocks))
            self.record_status = self.export_csv()
            self.complete = True
            self.validation_error = None
            self.checkpoint()
            return
        if self.received and not self.partial_path.exists():
            raise ValueError('O arquivo parcial foi removido. Preserve os metadados e selecione outra pasta.')
        self.file = self.partial_path.open('r+b' if self.partial_path.exists() else 'x+b')
        self.checkpoint()
        # Pode ter ocorrido encerramento do aplicativo depois do último bloco e antes do rename.
        if len(self.received) == frame.blocks and not self.validation_error: self.finish()

    @property
    def blocks(self): return self.identity[1]

    @property
    def missing(self): return self.blocks - len(self.received)

    def missing_indices(self, limit=30):
        result = []
        for i in range(self.blocks):
            if i not in self.received:
                result.append(i)
                if len(result) >= limit: break
        return result

    def accept(self, frame):
        frame.validate()
        if (frame.session, frame.blocks, frame.size, frame.checksum) != self.identity:
            raise ValueError('Bloco pertence a outro arquivo.')
        self.last_rssi = frame.rssi
        if frame.kind != 'B':
            self.ended_pass = frame.kind == 'E'
            if frame.kind == 'E': self.checkpoint()
            return False
        if self.complete:
            self.duplicates += 1
            return False
        if frame.index in self.received:
            self.file.seek(frame.index * BLOCK_SIZE)
            if self.file.read(len(frame.data)) != frame.data:
                raise ValueError('Bloco repetido diverge da cópia local. Preserve a pasta e inicie outra recepção.')
            self.duplicates += 1
            return False
        self.file.seek(frame.index * BLOCK_SIZE)
        if self.file.write(frame.data) != len(frame.data): raise OSError('Bloco salvo parcialmente.')
        self.ended_pass = False
        self.received.add(frame.index)
        if len(self.received) == self.blocks:
            self.finish()
        elif time.monotonic() - self.last_checkpoint > 1:
            self.checkpoint()
        return True

    def checkpoint(self):
        # Persiste os bytes antes de declarar os índices como recebidos.
        if self.file and not self.file.closed:
            self.file.flush(); os.fsync(self.file.fileno())
        self.meta.update(received=sorted(self.received), missing=self.missing,
                         duplicates=self.duplicates, last_rssi=self.last_rssi,
                         updated_at=now(), state='complete' if self.complete else ('invalid' if self.validation_error else 'incomplete'),
                         record_status=self.record_status, validation_error=self.validation_error)
        atomic_json(self.meta_path, self.meta)
        self.last_checkpoint = time.monotonic()

    def finish(self):
        try:
            self.finish_checked()
        except ValueError as exc:
            self.validation_error = str(exc)
            self.checkpoint()
            raise

    def finish_checked(self):
        self.checkpoint()
        if self.partial_path.stat().st_size != self.identity[2] or file_crc(self.partial_path) != self.identity[3]:
            raise ValueError('CRC32 ou tamanho do arquivo não confere. Cópia parcial preservada.')
        # Validação interna: CRC por amostra, sequência e marcador final do registro.
        record, acquisition = load_flash(self.partial_path)
        if record['session_id'] != self.identity[0]: raise ValueError('Sessão interna do arquivo não confere.')
        self.file.close()
        self.file = None
        os.replace(self.partial_path, self.path)
        self.record_status = self.export_csv(record, acquisition)
        self.complete = True
        self.validation_error = None
        self.checkpoint()

    def export_csv(self, record=None, acquisition=None):
        if record is None: record, acquisition = load_flash(self.path)
        temp = self.csv_path.with_suffix('.csv.tmp')
        with temp.open('w', encoding='utf-8-sig', newline='') as f:
            writer = csv.writer(f, delimiter=';')
            writer.writerow(['session_id', 'sample_seq', 't_us', 'raw_counts', 'status'])
            for p in acquisition.traces['USB'].samples:
                writer.writerow([p.session, p.seq, p.t_us, p.raw, p.flag])
            f.flush(); os.fsync(f.fileno())
        os.replace(temp, self.csv_path)
        return dict(state=record['state'], stop_confirmed=record['stop_confirmed'],
                    reason=record.get('reason'), sample_count=len(acquisition.traces['USB'].samples))

    def close(self):
        try: self.checkpoint()
        finally:
            if self.file: self.file.close(); self.file = None
