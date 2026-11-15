"""Executa simulacoes nativas; nao usa uma placa ESP32 ou portas seriais."""
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parent
CASES = ['identidade', 'fragmentos', 'antigos', 'malformados', 'lixo_apos_json', 'duplicadas', 'timeout_rx', 'limite_rx', 'nul_controle', 'crlf_burst', 'sessao_errada', 'esquema', 'dedup_inicio', 'conflito', 'expirado', 'pendente_retido', 'dedup_alternar', 'dedup_erro', 'status_snapshot', 'envio_conclusao', 'cancelamento', 'falha_envio', 'timeout_radio', 'falha_sensor', 'falha_flash', 'sem_lora', 'sem_flash', 'sem_hx', 'boot_responsivo', 'backpressure', 'controle_congestionado', 'amostra', 'configuracao', 'permutacao', 'fuzz', 'cal_padrao','cal_configurar','cal_um_explicito','cal_sinais_formula','cal_invalidos','cal_extremos_finitos','cal_offset_unidade','cal_dedup_parametro','cal_bloqueio_aquisicao','cal_tara_assincrona','cal_tara_repetida','cal_tara_sem_duplo_zero','cal_tara_ausente','cal_tara_parcial','cal_tara_bloqueia_config','cal_recuperar_sensor','cal_envio_tara','cal_fragmentos','cal_boot_ausente','cal_status_tamanho']

CASES += ['aq_inicio_real', 'aq_ids_distintos', 'aq_mesma_medicao', 'aq_frequencias_independentes', 'aq_nao_reenvia_ultima', 'aq_finalizacao', 'aq_espera_timeout', 'aq_cancelar_preparacao', 'aq_flash_sem_espaco', 'aq_cabecalho_falha', 'aq_amostra_parcial', 'aq_rodape_falha', 'aq_flush_falha', 'aq_sensor_registro_falho', 'aq_reconectar', 'aq_recuperacao_boot', 'aq_arquivo_antigo', 'aq_espaco_esgota', 'aq_corrupcao_amostra', 'aq_limites_json']

CASES += ['aq_radio_falha_continua','aq_falha_reabertura','aq_sensor_antigo_inicio']

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
