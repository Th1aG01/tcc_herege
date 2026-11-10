"""Tempo informado pelo ESP32, independente das amostras e do relógio do Windows."""
from dataclasses import dataclass


@dataclass
class AcquisitionTiming:
    session: str
    requested_s: int
    elapsed_us: int | None = None
    limit_us: int | None = None
    phase: str | None = None
    received_at: float | None = None

    def accept(self, line: str, received_at: float) -> bool:
        parts = line.split(';')
        if len(parts) != 5 or parts[0] != 'CLOCK':
            raise ValueError('Mensagem de tempo inválida.')
        if parts[1] != self.session:
            return False
        _, _, phase, elapsed, limit = parts
        if phase not in ('RECORDING', 'STOPPED') or not elapsed.isdecimal() or not limit.isdecimal():
            raise ValueError('Estado ou tempo do ESP32 inválido.')
        elapsed, limit = int(elapsed), int(limit)
        if not 0 <= elapsed <= 2**63 - 1 or limit != self.requested_s * 1_000_000:
            raise ValueError('A duração aceita pelo ESP32 difere da duração solicitada.')
        if self.elapsed_us is not None and elapsed < self.elapsed_us:
            raise ValueError('O relógio do ensaio retrocedeu.')
        if self.phase == 'STOPPED' and (phase != 'STOPPED' or elapsed != self.elapsed_us):
            raise ValueError('Relógio alterado após encerramento do ensaio.')
        self.elapsed_us, self.limit_us = elapsed, limit
        self.phase, self.received_at = phase, received_at
        return True

    def snapshot(self, stop_confirmed=False):
        final = bool(stop_confirmed and self.phase == 'STOPPED')
        return dict(source='esp32_monotonic' if self.elapsed_us is not None else None,
                    requested_duration_s=self.requested_s,
                    accepted_duration_us=self.limit_us,
                    last_elapsed_us=self.elapsed_us, phase=self.phase,
                    duration_confirmed=final,
                    actual_duration_us=self.elapsed_us if final else None)

    @classmethod
    def from_metadata(cls, session, meta):
        data = meta.get('timing')
        if not data or data.get('last_elapsed_us') is None:
            return None
        result = cls(session, int(data['requested_duration_s']))
        result.accept(f'CLOCK;{session};{data["phase"]};{data["last_elapsed_us"]};{data["accepted_duration_us"]}', 0)
        return result
