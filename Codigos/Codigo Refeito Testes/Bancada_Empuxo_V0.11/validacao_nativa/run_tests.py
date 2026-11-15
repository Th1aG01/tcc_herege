"""Executa simulacoes nativas; nao usa uma placa ESP32 ou portas seriais."""
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parent
CASES = [
    'configuracao', 'enquadramento', 'limites', 'medicao', 'boot_sem_hx',
    'tara_incompleta', 'timeout_hx', 'sem_lora', 'sem_flash', 'atomicidade',
    'falha_abertura', 'falha_gravacao', 'sem_ensaio', 'retransmissao',
    'falha_leitura', 'falha_seek', 'falha_lora', 'leds',
    'coeficiente_invalido', 'tamanho_amostra',
]

env = os.environ.copy()
# O ambiente de execucao usa ptrace, que impede o LeakSanitizer.
# AddressSanitizer e UndefinedBehaviorSanitizer continuam ativos.
env['ASAN_OPTIONS'] = 'detect_leaks=0:halt_on_error=1'
env['UBSAN_OPTIONS'] = 'halt_on_error=1'
reports = []
for case in CASES:
    result = subprocess.run([str(ROOT / 'test_firmware'), case],
                            capture_output=True, text=True, env=env)
    reports.append({'cenario': case, 'exit_code': result.returncode,
                    'saida': result.stderr.strip()})
    print(case, 'OK' if result.returncode == 0 else result.stderr)
    (ROOT / (case + '.jsonl')).write_text(result.stdout, encoding='utf-8')
(ROOT / 'resultados.json').write_text(
    json.dumps(reports, ensure_ascii=False, indent=2), encoding='utf-8')
passed = sum(report['exit_code'] == 0 for report in reports)
print(f'TOTAL {passed}/{len(reports)}')
raise SystemExit(0 if passed == len(reports) else 1)
