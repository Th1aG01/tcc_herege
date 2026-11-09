"""CSV brutos separados por transporte e metadados sem calibração fictícia."""
import csv
import json
import os
from pathlib import Path
from datetime import datetime, timezone
from .storage import atomic_json
from .protocol import Packet, Acquisition, parse_line

FIELDS = ['session_id', 'packet_seq', 'sample_seq', 't_us', 'raw_counts', 'status',
          'rssi_dbm', 'received_utc', 'reference_counts', 'delta_counts']

def now():
    return datetime.now(timezone.utc).isoformat()

class LiveWriter:
    def __init__(self, root, session, name, notes, reference, limit_s, ports, flash):
        if not name.strip():
            raise ValueError('Informe o nome do teste.')
        self.folder = Path(root) / (datetime.now().strftime('%Y%m%d_%H%M%S') + '_' + session)
        self.folder.mkdir(parents=True, exist_ok=False)
        self.meta_path = self.folder / 'ensaio.json'
        self.reference = reference
        self.closed = False
        self.dirty = False
        self.files = {}
        self.writers = {}
        self.meta = dict(schema_version=2, app_version='0.2.0', session_id=session,
                         source='HARDWARE', calibration=None, calibrated=False,
                         cell_nominal_capacity_kg=5, capacity_source='informada_pelo_usuario',
                         test_name=name.strip(), notes=notes, reference_counts=reference,
                         reference_is_calibration=False, max_duration_s=limit_s,
                         ports=ports, flash_requested=flash, started_at=now(),
                         state='start_requested', stop_confirmed=False,
                         radio=dict(frequency_hz=915000000, sync_word=145,
                                    telemetry='ultima_amostra_ate_2_pacotes_por_segundo'),
                         samples_are_force=False)
        try:
            atomic_json(self.meta_path, self.meta)
            for source in ('USB', 'LORA'):
                f = (self.folder / f'{source.lower()}.csv').open('x', encoding='utf-8-sig', newline='')
                self.files[source] = f
                self.writers[source] = csv.DictWriter(f, fieldnames=FIELDS, delimiter=';')
                self.writers[source].writeheader()
            self.events = (self.folder / 'eventos.jsonl').open('x', encoding='utf-8')
        except Exception:
            for f in self.files.values():
                f.close()
            raise

    def event(self, source, text):
        self.events.write(json.dumps(dict(utc=now(), source=source, message=text), ensure_ascii=False) + '\n')
        self.dirty = True

    def append(self, source, p):
        delta = '' if self.reference is None else p.raw - self.reference
        self.writers[source].writerow(dict(session_id=p.session, packet_seq='' if p.packet_seq is None else p.packet_seq,
            sample_seq=p.seq, t_us=p.t_us, raw_counts=p.raw, status=p.flag,
            rssi_dbm='' if p.rssi is None else p.rssi, received_utc=now(),
            reference_counts='' if self.reference is None else self.reference, delta_counts=delta))
        self.dirty = True

    def flush(self):
        if self.closed or not self.dirty:
            return
        for f in [*self.files.values(), self.events]:
            f.flush()
            os.fsync(f.fileno())
        self.dirty = False

    def update(self, **kwargs):
        self.meta.update(kwargs)
        atomic_json(self.meta_path, self.meta)

    def finish(self, state, stats, confirmed=False, reason=None, board_count=None):
        if self.closed:
            return
        try:
            self.flush()
        finally:
            for f in [*self.files.values(), self.events]:
                f.close()
            self.closed = True
        self.update(state=state, stop_confirmed=confirmed, reason=reason, ended_at=now(),
                    statistics=stats, board_sample_count=board_count)

def load_live(path):
    path = Path(path)
    meta_path = path if path.name == 'ensaio.json' else path.parent / 'ensaio.json'
    meta = json.loads(meta_path.read_text(encoding='utf-8'))
    if meta.get('schema_version') != 2 or meta.get('calibrated') is not False:
        raise ValueError('Selecione um ensaio bruto da versão 0.2.')
    acquisition = Acquisition(meta['session_id'])
    reference = meta.get('reference_counts')
    for source in ('USB', 'LORA'):
        csv_path = meta_path.parent / f'{source.lower()}.csv'
        if csv_path.stat().st_size > 100_000_000:
            raise ValueError('Arquivo maior que 100 MB.')
        with csv_path.open(encoding='utf-8-sig', newline='') as f:
            reader = csv.DictReader(f, fieldnames=None, delimiter=';')
            if reader.fieldnames != FIELDS:
                raise ValueError('Cabeçalho CSV incompatível.')
            for row in reader:
                p = Packet(row['session_id'], int(row['packet_seq']) if row['packet_seq'] else None,
                           int(row['sample_seq']), int(row['t_us']), int(row['raw_counts']),
                           row['status'], int(row['rssi_dbm']) if row['rssi_dbm'] else None)
                expected_delta = '' if reference is None else str(p.raw - reference)
                expected_reference = '' if reference is None else str(reference)
                if row['reference_counts'] != expected_reference or row['delta_counts'] != expected_delta:
                    raise ValueError('Referência ou diferença alterada no arquivo.')
                if not acquisition.add(source, p):
                    raise ValueError('Ensaio contém duplicatas, ordem inválida ou outra sessão.')
    if meta.get('statistics') and acquisition.stats() != {
        **meta['statistics'], 'foreign': acquisition.foreign,
        'usb': {**meta['statistics']['usb'], 'duplicates': 0, 'out_of_order': 0},
        'lora': {**meta['statistics']['lora'], 'duplicates': 0, 'out_of_order': 0}
    }:
        raise ValueError('Estatísticas e amostras salvas não correspondem.')
    return meta, acquisition

def load_flash(path):
    """Abre o registro do ESP32; exige CRC e sequência e sinaliza final ausente."""
    path = Path(path)
    if path.stat().st_size > 100_000_000:
        raise ValueError('Arquivo maior que 100 MB.')
    with path.open(encoding='ascii') as f:
        header = f.readline().rstrip('\r\n').split(';')
        if len(header) != 3 or header[:2] != ['META', 'TCC2']:
            raise ValueError('Cabeçalho do registro ESP32 inválido.')
        acquisition = Acquisition(header[2])
        end = None
        for line in f:
            if end is not None:
                raise ValueError('Conteúdo inesperado após encerramento.')
            if line.startswith('END;'):
                end = line.strip().split(';')
                if len(end) != 4 or end[1] != acquisition.session:
                    raise ValueError('Encerramento inválido.')
            else:
                packet = parse_line(line)
                if packet is None or not acquisition.add('USB', packet):
                    raise ValueError('Registro ESP32 corrompido ou fora de ordem.')
        trace = acquisition.traces['USB']
        if trace.gaps or (end is not None and int(end[3]) != len(trace.samples)):
            raise ValueError('Quantidade de amostras inconsistente no registro ESP32.')
    return dict(session_id=acquisition.session, test_name='Registro interno do ESP32',
                state='flash_complete' if end else 'flash_incomplete', stop_confirmed=end is not None,
                reason=end[2] if end else 'SEM_ENCERRAMENTO', reference_counts=None,
                calibrated=False, calibration=None), acquisition
