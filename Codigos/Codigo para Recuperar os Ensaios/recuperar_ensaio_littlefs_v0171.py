#!/usr/bin/env python3
"""Recupera integralmente, somente por USB/JSON v2, ensaio formato 3.
Validação complementar da V0.17.1 R1. Não altera arquivos no ESP32.
Uso: py recuperar_ensaio_littlefs_v0171.py --porta COM3 \
         --ensaio 307EBCE0E5C7CE3F-1 --esperadas 117
Requer: py -m pip install pyserial
"""
from __future__ import annotations

import argparse
import csv
from datetime import datetime
import json
from pathlib import Path
import sys
import time

try:
    import serial
except ImportError:
    print("Dependência ausente: instale com 'py -m pip install pyserial'.", file=sys.stderr)
    raise SystemExit(2)

BAUD = 115200
LIMITE = 4


class FalhaValidacao(Exception):
    pass


def receber(ser, prazo: float, predicado):
    """Ignora linhas de boot, amostras assíncronas e eventos não relacionados."""
    fim = time.monotonic() + prazo
    while time.monotonic() < fim:
        linha = ser.readline()
        if not linha:
            continue
        try:
            mensagem = json.loads(linha.decode('utf-8').strip())
        except (ValueError, UnicodeError):
            continue
        if isinstance(mensagem, dict) and predicado(mensagem):
            return mensagem
    raise FalhaValidacao("Tempo esgotado aguardando resposta compatível da placa.")


def enviar(ser, pedido):
    pacote = json.dumps(pedido, ensure_ascii=False, separators=(',', ':')).encode('utf-8')
    if len(pacote) > 512:
        raise FalhaValidacao('Comando ultrapassou 512 bytes.')
    ser.write(pacote + b'\n')
    ser.flush()


def identificar(ser):
    pedido = {'protocolo': 2, 'tipo': 'comando', 'id': 0, 'comando': 'IDENTIFICAR'}
    for tentativa in range(3):
        enviar(ser, pedido)
        try:
            resposta = receber(ser, 6.0, lambda m: m.get('id') == 0 and
                               m.get('comando') == 'IDENTIFICAR' and
                               m.get('tipo') == 'resposta')
            if not resposta.get('ok') or resposta.get('firmware') != '0.17.1':
                raise FalhaValidacao('Resposta de identificação ou versão incompatível: ' + str(resposta))
            if resposta.get('protocolo') != 2 or resposta.get('formato_gravacao') != 3:
                raise FalhaValidacao('Protocolo/formato incompatíveis com esta ferramenta.')
            return resposta
        except FalhaValidacao:
            if tentativa == 2:
                raise
    raise FalhaValidacao('IDENTIFICAR não retornou resposta.')


def consultar(ser, sessao, id_comando, comando, **parametros):
    pedido = {'protocolo': 2, 'tipo': 'comando', 'sessao': sessao,
              'id': id_comando, 'comando': comando, **parametros}
    for tentativa in range(2):
        enviar(ser, pedido)
        try:
            resposta = receber(ser, 10.0, lambda m: m.get('id') == id_comando and
                               m.get('tipo') in ('resposta', 'erro') and
                               m.get('comando') == comando)
            if not resposta.get('ok', False):
                raise FalhaValidacao(f'{comando} id={id_comando}: ' + json.dumps(resposta, ensure_ascii=False))
            if resposta.get('sessao') != sessao or resposta.get('fase') != 'concluido':
                raise FalhaValidacao('Sessão ou fase incompatível: ' + str(resposta))
            return resposta
        except FalhaValidacao as exc:
            if 'Tempo esgotado' not in str(exc) or tentativa == 1:
                raise
            # Reenvio do mesmo ID e parâmetros: deduplicação nativa do protocolo v2.
    raise FalhaValidacao('Sem resposta.')


def executar(porta: str, ensaio: str, esperadas: int, diretorio: Path):
    if esperadas < 1:
        raise FalhaValidacao('Quantidade esperada deve ser positiva.')
    try:
        with serial.Serial(port=porta, baudrate=BAUD, timeout=0.5, write_timeout=2) as ser:
            print(f'Porta {porta} aberta; identificando sessão...')
            identidade = identificar(ser)
            sessao = identidade['sessao']
            id_comando = int(identidade['maior_id']) + 1
            print('Sessão:', sessao, '| Próximo ID:', id_comando)
            meta = consultar(ser, sessao, id_comando, 'CONSULTAR_ENSAIO', ensaio_id=ensaio)['ensaio']
            id_comando += 1
            if meta.get('ensaio_id') != ensaio or meta.get('formato') != 3:
                raise FalhaValidacao('Ensaio ou formato inesperado.')
            if meta.get('condicao') != 'concluido' or meta.get('fechamento_confirmado') is not True:
                raise FalhaValidacao('O ensaio solicitado não possui fechamento confirmado.')
            total = meta.get('amostras_disponiveis')
            if total != esperadas or meta.get('amostras_confirmadas_no_fim') != esperadas:
                raise FalhaValidacao(f'Contagem inconsistente: disponível={total}, esperada={esperadas}.')
            print(f'Ensaio {ensaio}: {total} amostras, {meta.get("bytes")} bytes, fechamento confirmado.')
            amostras = []
            anterior = -1
            for inicio in range(1, esperadas + 1, LIMITE):
                quantidade = min(LIMITE, esperadas - inicio + 1)
                bloco = consultar(ser, sessao, id_comando, 'LER_AMOSTRAS',
                                 ensaio_id=ensaio, inicio=inicio, limite=quantidade)
                id_comando += 1
                if (bloco.get('ensaio_id') != ensaio or bloco.get('inicio') != inicio or
                        bloco.get('bloco_id') != f'{ensaio}:{inicio}:{quantidade}' or
                        bloco.get('total_disponivel') != esperadas or
                        bloco.get('quantidade') != quantidade or
                        bloco.get('erros') != 0 or
                        bloco.get('proximo') != inicio + quantidade or
                        bloco.get('fim') is not (inicio + quantidade > esperadas)):
                    raise FalhaValidacao(f'Bloco inconsistente: início {inicio}: {bloco}')
                if len(bloco.get('amostras', [])) != quantidade:
                    raise FalhaValidacao(f'Quantidade errada no bloco {inicio}.')
                for j, item in enumerate(bloco['amostras']):
                    if item.get('indice') != inicio + j or item.get('valida') is not True:
                        raise FalhaValidacao(f'Índice/CRC inválido no bloco {inicio}: {item}')
                    t = item.get('tempo_ms')
                    valor = item.get('empuxo_kgf')
                    if not isinstance(t, int) or t < anterior or not isinstance(valor, (int, float)):
                        raise FalhaValidacao(f'Dados temporais/numéricos inválidos em {inicio+j}.')
                    anterior = t
                    amostras.append({'indice': item['indice'], 'tempo_ms': t,
                                     'valor_protocolar_nao_calibrado': valor})
                print(f'  Recuperadas {len(amostras)}/{esperadas}', end='\r', flush=True)
            print()
    except serial.SerialException as exc:
        raise FalhaValidacao(f'Não foi possível acessar {porta}. Feche o Monitor Serial: {exc}') from exc

    if len(amostras) != esperadas or [a['indice'] for a in amostras] != list(range(1, esperadas + 1)):
        raise FalhaValidacao('Recuperação não cobriu todos os índices.')
    diretorio.mkdir(parents=True, exist_ok=True)
    # Nunca substituir arquivos anteriores do computador.
    nome = f'RECUPERACAO_{ensaio}_{datetime.now().strftime("%Y%m%d_%H%M%S")}'
    destino_csv = diretorio / (nome + '.csv')
    destino_json = diretorio / (nome + '.json')
    if destino_csv.exists() or destino_json.exists():
        raise FalhaValidacao('Arquivos de saída já existem; tente novamente.')
    with destino_csv.open('x', newline='', encoding='utf-8-sig') as f:
        escrita = csv.DictWriter(f, fieldnames=['indice', 'tempo_ms', 'valor_protocolar_nao_calibrado'])
        escrita.writeheader()
        escrita.writerows(amostras)
    with destino_json.open('x', encoding='utf-8') as f:
        json.dump({'versao_ferramenta': '1.0', 'firmware': '0.17.1', 'sessao_recuperacao': sessao,
                   'ensaio': meta, 'total_validado': esperadas, 'amostras': amostras,
                   'nota': 'CRC foi verificado e reportado pelo firmware; coeficiente 1 não calibra kgf.'},
                  f, ensure_ascii=False, indent=2)
    print(f'SUCESSO: {len(amostras)} amostras sequenciais, todas com valida=true, erros=0.')
    print('CSV:', destino_csv.resolve())
    print('JSON:', destino_json.resolve())


def main():
    p = argparse.ArgumentParser(description='Recuperação read-only de ensaio LittleFS formato 3.')
    p.add_argument('--porta', default='COM3', help='Porta serial (padrão COM3).')
    p.add_argument('--ensaio', required=True, help='ensaio_id completo.')
    p.add_argument('--esperadas', required=True, type=int, help='Quantidade esperada.')
    p.add_argument('--destino', default='.', type=Path, help='Pasta de saída.')
    a = p.parse_args()
    try:
        executar(a.porta, a.ensaio, a.esperadas, a.destino)
    except (FalhaValidacao, OSError, ValueError) as exc:
        print('FALHA: ' + str(exc), file=sys.stderr)
        raise SystemExit(1)


if __name__ == '__main__':
    main()
