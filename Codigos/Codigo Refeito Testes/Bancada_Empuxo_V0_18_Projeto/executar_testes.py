"""Executar na raiz: python3 executar_testes.py. Requer gcc, g++ e Python 3."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent
os.chdir(ROOT)
EVID = ROOT / "evidencias"
EVID.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix="bancada_build_") as tmp:
    tmp = Path(tmp)
    log = []

    def run(args):
        r = subprocess.run(list(map(str, args)), text=True, capture_output=True)
        log.append("$ " + " ".join(map(str, args)) + "\n" + r.stdout + r.stderr)
        (EVID / "build_nativo.log").write_text("\n".join(log), encoding="utf-8")
        if r.returncode:
            print(r.stdout + r.stderr)
            raise SystemExit(r.returncode)

    run(["g++", "--version"])
    run(["gcc", "--version"])
    flags = ["-Wall", "-Wextra", "-Werror", "-pedantic", "-fsanitize=address,undefined",
             "-fno-omit-frame-pointer", "-g"]
    inc = ["-Ivalidacao_nativa/mocks", "-Ivalidacao_flash", "-Iterceiros/ArduinoJson/src"]
    for name in ("lfs", "lfs_util"):
        run(["gcc", "-std=c11", *flags, "-c",
             f"Bancada_Empuxo_V0_18/src/littlefs/{name}.c", "-o", tmp/(name+".o")])
    run(["g++", "-std=c++17", *flags, "-no-pie", *inc,
         "validacao_nativa/test_firmware.cpp", "-o", "validacao_nativa/test_firmware"])
    run(["g++", "-std=c++17", *flags, "-no-pie", *inc,
         "validacao_flash/test_flash.cpp", tmp/"lfs.o", tmp/"lfs_util.o",
         "-o", "validacao_flash/test_flash"])
    for name, option in (("test_diagnostico", ["-DTEST_DIAGNOSTICO"]), ("test_manutencao", [])):
        run(["g++", "-std=c++17", *flags, "-no-pie", *inc, *option,
             "validacao_flash/test_manutencao.cpp", tmp/"lfs.o", tmp/"lfs_util.o",
             "-o", f"validacao_flash/{name}"])
    for folder, output in (("validacao_nativa", "regressoes_141.log"),
                           ("validacao_flash", "flash_externa.log")):
        with (EVID/output).open("w", encoding="utf-8") as log_file:
            r = subprocess.run([os.sys.executable, f"{folder}/run_tests.py"],
                               stdout=log_file, stderr=subprocess.STDOUT)
        print((EVID/output).read_text().splitlines()[-1])
        if r.returncode:
            raise SystemExit(r.returncode)
