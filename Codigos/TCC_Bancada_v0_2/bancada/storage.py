"""Registro incremental em CSV e metadados em JSON; nunca substitui um ensaio."""
import csv
import json
import math
import os
from pathlib import Path
from dataclasses import asdict
from datetime import datetime, timezone
from uuid import uuid4
from . import __version__
from .core import Calibration, Sample, SimulationConfig, KGF_TO_N

FIELDS = ['schema_version', 'session_id', 'source', 'seq', 't_us', 'raw_counts',
          'calibration_id', 'force_N', 'force_kgf']

def atomic_json(path, obj):
    temp = path.with_suffix('.json.tmp')
    with temp.open('w', encoding='utf-8') as f:
        json.dump(obj, f, ensure_ascii=False, indent=2, allow_nan=False)
        f.flush()
        os.fsync(f.fileno())
    os.replace(temp, path)

class SessionWriter:
    def __init__(self, root: Path, name: str, notes: str,
                 config: SimulationConfig, calibration: Calibration):
        config.validate()
        if not name.strip():
            raise ValueError('Informe um nome para o ensaio.')
        self.calibration = calibration
        session_id = uuid4().hex
        self.folder = root / (datetime.now().strftime('%Y%m%d_%H%M%S') + '_' + session_id[:8])
        self.folder.mkdir(parents=True, exist_ok=False)
        self.csv_path = self.folder / 'amostras_SIMULADAS.csv'
        self.meta_path = self.csv_path.with_suffix('.json')
        self.metadata = dict(schema_version=1, app_version=__version__, session_id=session_id,
                             source='SIMULATED', test_name=name.strip(), notes=notes,
                             started_at=datetime.now(timezone.utc).isoformat(),
                             status='recording', simulation=asdict(config),
                             calibration=asdict(calibration), sample_count=None)
        self.count = 0
        self.last_time = -1
        self.closed = False
        atomic_json(self.meta_path, self.metadata)
        self.file = self.csv_path.open('x', encoding='utf-8-sig', newline='')
        self.writer = csv.DictWriter(self.file, fieldnames=FIELDS, delimiter=';')
        self.writer.writeheader()
        self.file.flush()

    def append(self, samples):
        if self.closed:
            raise ValueError('O registro já foi encerrado.')
        for sample in samples:
            sample.validate()
            if sample.seq != self.count or sample.t_us <= self.last_time:
                raise ValueError('Sequência ou tempo de aquisição inconsistente.')
            force = self.calibration.force(sample.raw_counts)
            self.writer.writerow(dict(schema_version=1, session_id=self.metadata['session_id'],
                                      source='SIMULATED', seq=sample.seq, t_us=sample.t_us,
                                      raw_counts=sample.raw_counts, calibration_id=self.calibration.identifier,
                                      force_N=format(force, '.12g'), force_kgf=format(force/KGF_TO_N, '.12g')))
            self.count += 1
            self.last_time = sample.t_us
        self.file.flush()
        os.fsync(self.file.fileno())

    def finish(self, status='completed'):
        if self.closed:
            return
        try:
            self.file.flush()
            os.fsync(self.file.fileno())
        finally:
            self.file.close()
            self.closed = True
        self.metadata.update(status=status, sample_count=self.count,
                             ended_at=datetime.now(timezone.utc).isoformat())
        atomic_json(self.meta_path, self.metadata)

def load_session(csv_path: Path):
    """Recusa arquivos inválidos; não corrige nem interpola valores silenciosamente."""
    if csv_path.stat().st_size > 20_000_000:
        raise ValueError('Arquivo maior que o limite desta versão (20 MB).')
    with csv_path.with_suffix('.json').open(encoding='utf-8') as f:
        meta = json.load(f)
    if meta.get('schema_version') != 1 or meta.get('source') != 'SIMULATED':
        raise ValueError('Esta versão abre apenas ensaios SIMULADOS do formato 1.')
    calibration = Calibration(**meta['calibration'])
    calibration.force(calibration.offset_counts)
    samples = []
    previous_t = -1
    with csv_path.open(encoding='utf-8-sig', newline='') as f:
        reader = csv.DictReader(f, delimiter=';')
        if reader.fieldnames != FIELDS:
            raise ValueError('Cabeçalho CSV incompatível.')
        for row in reader:
            if len(samples) >= 100_000:
                raise ValueError('Excesso de amostras nesta versão.')
            if (row['source'] != 'SIMULATED' or row['schema_version'] != '1' or
                row['session_id'] != meta['session_id'] or row['calibration_id'] != calibration.identifier):
                raise ValueError('Identificação da amostra diverge dos metadados.')
            s = Sample(int(row['seq']), int(row['t_us']), int(row['raw_counts']))
            s.validate()
            if s.seq != len(samples) or s.t_us <= previous_t:
                raise ValueError('Há amostras ausentes, duplicadas ou tempos fora de ordem.')
            n, kgf = float(row['force_N']), float(row['force_kgf'])
            expected = calibration.force(s.raw_counts)
            if not (math.isfinite(n) and math.isfinite(kgf) and
                    math.isclose(n, expected, rel_tol=1e-9, abs_tol=1e-8) and
                    math.isclose(kgf, n/KGF_TO_N, rel_tol=1e-9, abs_tol=1e-8)):
                raise ValueError('Força inconsistente com a leitura bruta/calibração.')
            samples.append(s)
            previous_t = s.t_us
    if meta.get('status') not in ('recording', 'completed', 'stopped', 'error'):
        raise ValueError('Estado de sessão desconhecido.')
    if meta['status'] != 'recording' and meta.get('sample_count') != len(samples):
        raise ValueError('A quantidade de amostras não confere com o registro final.')
    return meta, calibration, samples
