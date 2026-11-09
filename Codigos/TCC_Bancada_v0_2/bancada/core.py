"""Tipos e conversão independentes da interface e do transporte."""
from dataclasses import dataclass
import math

KGF_TO_N = 9.80665

@dataclass(frozen=True)
class Calibration:
    identifier: str = "SIMULADA-001"
    offset_counts: int = 100000
    newtons_per_count: float = 0.01

    def force(self, raw: int) -> float:
        if not math.isfinite(self.newtons_per_count) or self.newtons_per_count == 0:
            raise ValueError("Fator de calibração inválido.")
        return (raw - self.offset_counts) * self.newtons_per_count

@dataclass(frozen=True)
class Sample:
    seq: int
    t_us: int
    raw_counts: int

    def validate(self):
        if self.seq < 0 or self.t_us < 0 or not -(2**23) <= self.raw_counts < 2**23:
            raise ValueError("Amostra fora dos limites do formato.")

@dataclass(frozen=True)
class SimulationConfig:
    duration_s: float = 12.0
    rate_hz: int = 80
    peak_n: float = 1000.0
    seed: int = 42

    def validate(self):
        if not math.isfinite(self.duration_s) or not 1 <= self.duration_s <= 120:
            raise ValueError("A duração deve ficar entre 1 e 120 segundos.")
        if self.rate_hz not in (10, 80):
            raise ValueError("Selecione 10 ou 80 amostras/s.")
        if not math.isfinite(self.peak_n) or not 1 <= self.peak_n <= 20000:
            raise ValueError("A amplitude simulada deve ficar entre 1 e 20.000 N.")
