"""Exercita o fonte do firmware com periféricos simulados, se g++ estiver disponível."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


@unittest.skipUnless(shutil.which('g++'), 'Teste C++ requer g++; os testes Python não requerem.')
class FirmwareTimingTests(unittest.TestCase):
    def test_duration_and_faults(self):
        fixtures = Path(__file__).parent / 'firmware'
        with tempfile.TemporaryDirectory() as folder:
            binary = Path(folder) / 'timing_check'
            subprocess.run(['g++', '-std=c++17', '-I', str(fixtures),
                            str(fixtures / 'timing_check.cpp'), '-o', str(binary)],
                           check=True, capture_output=True, text=True, timeout=60)
            result = subprocess.run([str(binary)], check=True, capture_output=True, text=True, timeout=30)
            self.assertIn('30 s com taxa irregular OK', result.stdout)

