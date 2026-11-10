import csv
import json
from pathlib import Path
import tempfile
import unittest

from bancada.core import Calibration, SimulationConfig, KGF_TO_N
from bancada.simulator import Simulator
from bancada.storage import SessionWriter, load_session, FIELDS


class PipelineTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.config = SimulationConfig()
        self.cal = Calibration()

    def session(self, end=12, status='completed'):
        samples = Simulator(self.config, self.cal).read_until(end)
        writer = SessionWriter(self.root, 'Teste', 'Observação', self.config, self.cal)
        writer.append(samples)
        writer.finish(status)
        return writer, samples

    def test_units(self):
        self.assertEqual(self.cal.force(100000), 0)
        self.assertAlmostEqual(self.cal.force(101000), 10)
        self.assertAlmostEqual(KGF_TO_N / KGF_TO_N, 1)

    def test_simulation_independent_of_gui_delay(self):
        expected = Simulator(self.config, self.cal).read_until(12)
        sim = Simulator(self.config, self.cal)
        actual = []
        for t in (0, 0.123, 2.81, 5, 11.999, 12, 13):
            actual.extend(sim.read_until(t))
        self.assertEqual(actual, expected)
        self.assertEqual(len(actual), 961)
        self.assertEqual(actual[-1].t_us, 12000000)
        self.assertTrue(sim.finished)

    def test_complete_roundtrip_and_unique_folders(self):
        writer, expected = self.session()
        meta, cal, actual = load_session(writer.csv_path)
        self.assertEqual(actual, expected)
        self.assertEqual(cal, self.cal)
        self.assertEqual(meta['sample_count'], 961)
        self.assertEqual(meta['status'], 'completed')
        other, _ = self.session()
        self.assertNotEqual(writer.folder, other.folder)

    def test_user_stop_preserves_prefix(self):
        writer, expected = self.session(0.7, 'stopped')
        meta, _, actual = load_session(writer.csv_path)
        self.assertEqual(actual, expected)
        self.assertEqual(meta['status'], 'stopped')
        self.assertEqual(len(actual), 57)

    def test_unfinished_record_can_be_recovered(self):
        writer = SessionWriter(self.root, 'Interrompido', '', self.config, self.cal)
        samples = Simulator(self.config, self.cal).read_until(0.3)
        writer.append(samples)
        writer.file.close()  # Simula ausência de encerramento, após flush.
        meta, _, actual = load_session(writer.csv_path)
        self.assertEqual(meta['status'], 'recording')
        self.assertEqual(actual, samples)

    def test_corrupted_rows_are_rejected(self):
        for field, value in [('seq', '5'), ('t_us', '0'), ('force_N', 'nan'),
                             ('force_kgf', '10000'), ('source', 'HARDWARE')]:
            with self.subTest(field=field):
                writer, _ = self.session()
                with writer.csv_path.open(encoding='utf-8-sig', newline='') as f:
                    rows = list(csv.DictReader(f, delimiter=';'))
                rows[1][field] = value
                with writer.csv_path.open('w', encoding='utf-8-sig', newline='') as f:
                    out = csv.DictWriter(f, fieldnames=FIELDS, delimiter=';')
                    out.writeheader()
                    out.writerows(rows)
                with self.assertRaises(ValueError):
                    load_session(writer.csv_path)

    def test_missing_final_row_is_rejected(self):
        writer, _ = self.session()
        lines = writer.csv_path.read_text(encoding='utf-8-sig').splitlines()
        writer.csv_path.write_text('\n'.join(lines[:-1])+'\n', encoding='utf-8-sig')
        with self.assertRaises(ValueError):
            load_session(writer.csv_path)

    def test_invalid_configuration(self):
        for cfg in (SimulationConfig(duration_s=0), SimulationConfig(rate_hz=20),
                    SimulationConfig(peak_n=float('nan'))):
            with self.assertRaises(ValueError):
                cfg.validate()


if __name__ == '__main__':
    unittest.main()
