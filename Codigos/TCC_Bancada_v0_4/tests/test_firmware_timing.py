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
            from bancada.protocol import parse_line
            import json
            from bancada.live_storage import load_flash
            line = next(x[len('JSON_SAMPLE '):] for x in result.stdout.splitlines() if x.startswith('JSON_SAMPLE '))
            packet = parse_line(line)
            self.assertEqual(packet.raw, -123)
            self.assertLessEqual(len(line.encode()), 255)
            obj=json.loads(line)
            # Registro de uma amostra, sequência inicial zero, CRC recalculado.
            from bancada.protocol import Packet, encode
            obj['n']=0
            obj['c']=encode(Packet(packet.session,None,0,packet.t_us,packet.raw)).split(';')[-1]
            path=Path(folder)/'registro.log'
            path.write_text(json.dumps(dict(type='meta',v=4,s=packet.session))+'\n'+json.dumps(obj)+'\n'+json.dumps(dict(type='end',s=packet.session,reason='LIMIT',count=1))+'\n')
            meta, acquisition=load_flash(path)
            self.assertTrue(meta['stop_confirmed'])
            self.assertEqual(acquisition.traces['USB'].samples[0].raw,-123)

