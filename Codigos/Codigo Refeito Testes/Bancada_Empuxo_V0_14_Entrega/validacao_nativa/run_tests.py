"""Executa simulacoes nativas; nao usa uma placa ESP32 ou portas seriais."""
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parent
CASES = ['identidade', 'fragmentos', 'antigos', 'malformados', 'lixo_apos_json', 'duplicadas', 'timeout_rx', 'limite_rx', 'nul_controle', 'crlf_burst', 'sessao_errada', 'esquema', 'dedup_inicio', 'conflito', 'expirado', 'pendente_retido', 'dedup_alternar', 'dedup_erro', 'status_snapshot', 'envio_conclusao', 'cancelamento', 'falha_envio', 'timeout_radio', 'falha_sensor', 'falha_flash', 'sem_lora', 'sem_flash', 'sem_hx', 'boot_responsivo', 'backpressure', 'controle_congestionado', 'amostra', 'configuracao', 'permutacao', 'fuzz', 'cal_padrao','cal_configurar','cal_um_explicito','cal_sinais_formula','cal_invalidos','cal_extremos_finitos','cal_offset_unidade','cal_dedup_parametro','cal_bloqueio_aquisicao','cal_tara_assincrona','cal_tara_repetida','cal_tara_sem_duplo_zero','cal_tara_ausente','cal_tara_parcial','cal_tara_bloqueia_config','cal_recuperar_sensor','cal_envio_tara','cal_fragmentos','cal_boot_ausente','cal_status_tamanho']

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
