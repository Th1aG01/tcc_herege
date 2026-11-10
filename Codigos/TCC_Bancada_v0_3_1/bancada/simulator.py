"""Fonte artificial determinística. Não modela um motor específico."""
import math
import random
from .core import Sample, Calibration, SimulationConfig

class Simulator:
    def __init__(self, config: SimulationConfig, calibration: Calibration):
        config.validate()
        self.config, self.calibration = config, calibration
        self.next_seq = 0
        self.total = math.floor(config.duration_s * config.rate_hz) + 1
        self.rng = random.Random(config.seed)

    @property
    def finished(self):
        return self.next_seq >= self.total

    def read_until(self, elapsed_s: float) -> list[Sample]:
        """Entrega amostras devidas no relógio simulado, mesmo após atraso da GUI."""
        if elapsed_s < 0:
            return []
        end = min(self.total, math.floor(elapsed_s * self.config.rate_hz + 1e-9) + 1)
        result = []
        while self.next_seq < end:
            seq = self.next_seq
            t = seq / self.config.rate_hz
            phase = t / self.config.duration_s
            if phase < 0.1 or phase >= 0.9:
                envelope = 0.0
            elif phase < 0.18:
                envelope = (phase - 0.1) / 0.08
            elif phase < 0.75:
                envelope = 0.82 + 0.18 * math.exp(-(phase - 0.18) * 12)
            else:
                envelope = 0.82 * (0.9 - phase) / 0.15
            force = self.config.peak_n * (envelope + self.rng.gauss(0, 0.004))
            raw = round(self.calibration.offset_counts + force / self.calibration.newtons_per_count)
            sample = Sample(seq, round(t * 1_000_000), raw)
            sample.validate()
            result.append(sample)
            self.next_seq += 1
        return result
