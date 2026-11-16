"""Driver real + LittleFS real; SPI/HX711/radio/tempo simulados, sem hardware."""
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent
CASES = ["jedec_ausente", "jedec_incorreto", "busy", "boot_vazio",
         "boot_desconhecido", "provisionamento_protegido", "driver_paginas",
         "driver_programa_falha", "driver_erase_falha", "driver_wel",
         "driver_protegido", "driver_limites", "driver_flush", "somente_leitura",
         "persistencia_gravar", "persistencia_ler", "multiplos_crc_exclusao",
         "lora_spi", "usb_ausente", "flush_falha", "capacidade_cheia", "colisao",
         "corrupcao_amostra", "recuperar_superbloco", "corte_preparar"]

reports = []
with tempfile.TemporaryDirectory(prefix="bancada_nor_") as disk:
    env = os.environ.copy()
    env.update(NOR_DISK=str(Path(disk)/"nor.bin"), NOR_BASE=str(Path(disk)/"base.bin"),
               ASAN_OPTIONS="detect_leaks=0:halt_on_error=1", UBSAN_OPTIONS="halt_on_error=1")

    def run(case, *args, executable="test_flash"):
        name = "_".join((case, *map(str, args)))
        r = subprocess.run([str(ROOT/executable), case, *map(str, args)],
                           env=env, text=True, capture_output=True, timeout=90)
        reports.append(dict(cenario=name, exit_code=r.returncode, saida=r.stderr.strip()))
        (ROOT/(name+".jsonl")).write_text(r.stdout, encoding="utf-8")
        print(name, "OK" if r.returncode == 0 else r.stderr, flush=True)

    for case in CASES:
        run(case)
    # Cada corte usa a MESMA imagem inicial, nova memoria de processo e remount separado.
    for cut in range(1, 25):
        run("corte_escrever", cut)
        run("corte_ler", cut)
    run("diagnostico", executable="test_diagnostico")
    run("vazio", executable="test_manutencao")
    run("conteudo", executable="test_manutencao")

(ROOT/"resultados.json").write_text(json.dumps(reports, ensure_ascii=False, indent=2), encoding="utf-8")
passed = sum(r["exit_code"] == 0 for r in reports)
print(f"TOTAL {passed}/{len(reports)}")
raise SystemExit(0 if passed == len(reports) else 1)
