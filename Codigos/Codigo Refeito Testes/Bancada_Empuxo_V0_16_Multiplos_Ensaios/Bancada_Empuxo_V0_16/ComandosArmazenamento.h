#pragma once
// Comandos USB do catalogo. Operacoes extensas avancam cooperativamente no loop.
struct OperacaoCatalogo {
  enum Tipo { NENHUMA, LISTAR, EXCLUIR_TUDO } tipo = NENHUMA;
  PaginaCatalogo pagina;
  uint32_t solicitacao = 0, revisao = 0, excluidos = 0, protegidos = 0, falhas = 0, resultados = 0;
  char protecaoGravacao[32] = {}, protecaoEnvio[32] = {}, protecaoPacote[32] = {};
  uint8_t indiceExclusao = 0;
};
OperacaoCatalogo operacaoCatalogo;

bool exclusaoTotalAtiva() { return operacaoCatalogo.tipo == OperacaoCatalogo::EXCLUIR_TUDO; }

bool arquivoProtegido(const char *id) {
  return (ensaioAtivo && !strcmp(id, resumo.id)) || (enviandoGravado && !strcmp(id, idTransmissao)) ||
         (loraTransmitindo && !strcmp(id, idPacoteLoRa));
}

bool exigirMemoria() {
  if (memoriaPronta) return true;
  emitirErro("FLASH_INDISPONIVEL", "Flash", "LittleFS indisponivel; nenhum arquivo sera formatado ou removido.");
  return false;
}

InformacaoArquivo consultarArquivo(const char *id) {
  if (ensaioAtivo && !strcmp(id, resumo.id)) {
    InformacaoArquivo info;
    info.existe = info.cabecalhoOk = true; info.formato = FORMATO_GRAVACAO;
    info.cab = cabecalhoGravacao; info.slots = amostrasGravadas;
    info.ultimoTempoValido = amostrasGravadas > 0; info.ultimoTempo = payload.tempo_ms;
    info.bytes = sizeof(CabecalhoEnsaio) + (size_t)amostrasGravadas * sizeof(RegistroAmostra);
    info.condicao = "em_andamento"; info.diagnostico = aquisicaoIniciada ? "ADQUIRINDO" : "PREPARANDO";
    return info;
  }
  return inspecionarArquivo(id);
}

void prepararRespostaCatalogo(JsonDocument &doc) {
  prepararMensagem(doc, "resposta"); doc["ok"] = true; doc["fase"] = "concluido";
}

void adicionarDetalhes(JsonObject item, const char *id, const InformacaoArquivo &info, bool completo) {
  item["ensaio_id"] = id; item["formato"] = info.formato;
  item["condicao"] = info.existe ? info.condicao : "indisponivel";
  item["em_uso"] = arquivoProtegido(id); item["bytes"] = info.bytes;
  item["nome"] = JsonString(info.cabecalhoOk ? info.cab.nome : ""); // copia: info pode ser temporario na listagem
  item["amostras_disponiveis"] = info.slots; // slots completos; CRC verificado na recuperacao
  if (!completo) return;
  item["diagnostico"] = info.diagnostico;
  item["leitura_suportada"] = info.cabecalhoOk;
  item["fechamento_confirmado"] = info.fechado;
  item["integridade_amostras"] = "verificada_por_bloco";
  item["duracao_observada_ms"] = nullptr;
  if (info.ultimoTempoValido) item["duracao_observada_ms"] = info.ultimoTempo;
  item["duracao_ms"] = nullptr; item["amostras_confirmadas_no_fim"] = nullptr;
  item["motivo"] = info.fechado ? info.fim.motivo : info.diagnostico;
  if (info.fechado) {
    item["duracao_ms"] = info.fim.duracao_ms;
    item["amostras_confirmadas_no_fim"] = info.fim.amostras;
  } else if (ensaioAtivo && !strcmp(id, resumo.id)) item["duracao_ms"] = duracaoEnsaio();
  if (info.cabecalhoOk) {
    item["data"] = info.cab.data; item["observacoes"] = info.cab.observacoes;
    item["celula_id"] = info.cab.celulaId; item["perfil_id"] = info.cab.perfilId;
    item["coeficiente"] = info.cab.coeficiente; item["tara_interna"] = info.cab.taraInterna;
    item["unidade"] = "kgf";
  }
}

void consultarEspaco() {
  if (!exigirMemoria()) return;
  size_t total = LittleFS.totalBytes(), usado = LittleFS.usedBytes();
  if (usado > total) { emitirErro("FLASH_ESPACO", "Flash", "Contabilidade do sistema de arquivos inconsistente."); return; }
  JsonDocument doc; prepararRespostaCatalogo(doc);
  doc["total_bytes"] = total; doc["usado_bytes"] = usado; doc["livre_bytes"] = total - usado;
  doc["reserva_bytes"] = RESERVA_FLASH;
  doc["disponivel_estimado_bytes"] = total - usado > RESERVA_FLASH ? total - usado - RESERVA_FLASH : 0;
  doc["backend"] = "LittleFS_interno"; doc["catalogo_revisao"] = revisaoCatalogo;
  publicar(doc);
}

void consultarDetalhes(const char *id) {
  if (!exigirMemoria()) return;
  InformacaoArquivo info = consultarArquivo(id);
  if (!info.existe) { emitirErro("ENSAIO_INDISPONIVEL", "Flash", "Ensaio inexistente ou falha ao abrir arquivo."); return; }
  JsonDocument doc; prepararRespostaCatalogo(doc);
  adicionarDetalhes(doc["ensaio"].to<JsonObject>(), id, info, true);
  publicar(doc);
}

void recuperarBloco(const char *id, uint32_t inicio, uint8_t limite) {
  if (!exigirMemoria()) return;
  if (arquivoProtegido(id)) { emitirErro("ENSAIO_PROTEGIDO", "Flash", "Aguarde gravacao ou transmissao terminar para recuperar blocos."); return; }
  InformacaoArquivo info = inspecionarArquivo(id);
  if (!info.existe) { emitirErro("ENSAIO_INDISPONIVEL", "Flash", "Ensaio inexistente ou falha de abertura."); return; }
  if (!info.cabecalhoOk) { emitirErro("FORMATO_NAO_SUPORTADO", "Flash", "Cabecalho corrompido, legado ou formato nao suportado; arquivo preservado."); return; }
  if (inicio > (uint64_t)info.slots + 1) { emitirErro("BLOCO_FORA_LIMITE", "Flash", "inicio ultrapassa a ultima amostra disponivel mais um."); return; }
  char caminho[64]; caminhoEnsaio(id, caminho, sizeof(caminho));
  File f = LittleFS.open(caminho, FILE_READ);
  if (!f) { emitirErro("FLASH_LEITURA", "Flash", "Nao foi possivel abrir as amostras."); return; }
  JsonDocument doc; prepararRespostaCatalogo(doc);
  char bloco[64]; snprintf(bloco, sizeof(bloco), "%s:%lu:%u", id, (unsigned long)inicio, limite);
  doc["ensaio_id"] = id; doc["bloco_id"] = bloco; doc["inicio"] = inicio;
  doc["condicao"] = info.condicao; doc["total_disponivel"] = info.slots;
  JsonArray amostras = doc["amostras"].to<JsonArray>();
  uint32_t proximo = inicio; uint8_t erros = 0;
  for (uint8_t n = 0; n < limite && proximo <= info.slots; n++, proximo++) {
    Payload p = {}; JsonObject item = amostras.add<JsonObject>();
    item["indice"] = proximo;
    if (!lerAmostra(f, info.cab, proximo, p)) {
      erros++; item["valida"] = false; item["codigo"] = "FLASH_AMOSTRA_INVALIDA";
      item["origem"] = "Flash"; item["descricao"] = "Falha de leitura, CRC ou identidade.";
    } else {
      item["valida"] = true; item["tempo_ms"] = p.tempo_ms; item["empuxo_kgf"] = p.empuxo;
    }
  }
  f.close();
  doc["quantidade"] = amostras.size(); doc["proximo"] = proximo;
  doc["fim"] = proximo > info.slots; doc["erros"] = erros;
  publicar(doc);
}

const char *excluirArquivo(const char *id, bool protegidoInicial = false) {
  if (protegidoInicial || arquivoProtegido(id)) return "ENSAIO_PROTEGIDO";
  char caminho[64]; caminhoEnsaio(id, caminho, sizeof(caminho));
  if (!LittleFS.exists(caminho)) return "NAO_ENCONTRADO";
  if (!LittleFS.remove(caminho) || LittleFS.exists(caminho)) return "FLASH_EXCLUSAO";
  revisaoCatalogo++;
  if (!strcmp(id, resumo.id)) resumo.resultado = "EXCLUIDO";
  return "EXCLUIDO";
}

void resultadoExclusao(JsonObject item, const char *id, const char *codigo) {
  item["ensaio_id"] = id; item["ok"] = !strcmp(codigo, "EXCLUIDO");
  item["codigo"] = codigo; item["origem"] = "Flash";
  item["descricao"] = !strcmp(codigo, "EXCLUIDO") ? "Arquivo excluido." :
    !strcmp(codigo, "ENSAIO_PROTEGIDO") ? "Arquivo em uso preservado." :
    !strcmp(codigo, "NAO_ENCONTRADO") ? "Arquivo nao encontrado." : "Falha ao excluir; confira o catalogo.";
}

void excluirSelecionados(JsonArrayConst ids) {
  if (!exigirMemoria()) return;
  if (exclusaoTotalAtiva()) { emitirErro("CATALOGO_OCUPADO", "Flash", "Exclusao total em andamento."); return; }
  JsonDocument doc; prepararRespostaCatalogo(doc);
  JsonArray resultados = doc["resultados"].to<JsonArray>();
  bool todos = true;
  for (JsonVariantConst id : ids) {
    const char *codigo = excluirArquivo(id.as<const char *>());
    if (strcmp(codigo, "EXCLUIDO")) todos = false;
    resultadoExclusao(resultados.add<JsonObject>(), id.as<const char *>(), codigo);
  }
  doc["todos_excluidos"] = todos; doc["catalogo_revisao"] = revisaoCatalogo;
  publicar(doc);
}

bool iniciarCatalogo(bool excluir, const char *apos, uint8_t limite, uint32_t revisao) {
  if (!exigirMemoria()) return false;
  if (operacaoCatalogo.tipo != OperacaoCatalogo::NENHUMA) {
    emitirErro("CATALOGO_OCUPADO", "Flash", "Aguarde a operacao de catalogo terminar."); return false;
  }
  if (revisao && revisao != revisaoCatalogo) {
    emitirErro("CATALOGO_ALTERADO", "Flash", "Catalogo mudou; reinicie a listagem com cursor vazio."); return false;
  }
  if (!operacaoCatalogo.pagina.iniciar(apos, limite)) {
    emitirErro("FLASH_DIRETORIO", "Flash", "Falha ao abrir diretorio de ensaios."); return false;
  }
  operacaoCatalogo.tipo = excluir ? OperacaoCatalogo::EXCLUIR_TUDO : OperacaoCatalogo::LISTAR;
  operacaoCatalogo.solicitacao = idAtual; operacaoCatalogo.revisao = revisaoCatalogo;
  operacaoCatalogo.excluidos = operacaoCatalogo.protegidos = operacaoCatalogo.falhas = operacaoCatalogo.resultados = 0;
  operacaoCatalogo.indiceExclusao = 0;
  strcpy(operacaoCatalogo.protecaoGravacao, ensaioAtivo ? resumo.id : "");
  strcpy(operacaoCatalogo.protecaoEnvio, enviandoGravado ? idTransmissao : "");
  strcpy(operacaoCatalogo.protecaoPacote, loraTransmitindo ? idPacoteLoRa : "");
  return true;
}

void concluirCatalogo(JsonDocument &doc) {
  doc["id"] = operacaoCatalogo.solicitacao;
  int r = buscarRegistro(operacaoCatalogo.solicitacao);
  if (r >= 0) {
    registros[r].pendente = false;
    if (!doc.overflowed() && measureJson(doc) < RESPOSTA_MAX)
      serializeJson(doc, registros[r].conclusao, sizeof(registros[r].conclusao));
  }
  operacaoCatalogo.pagina.diretorio.close();
  operacaoCatalogo.tipo = OperacaoCatalogo::NENHUMA;
  publicar(doc);
}

void atualizarCatalogo() {
  if (operacaoCatalogo.tipo == OperacaoCatalogo::NENHUMA || espacoTX() < 4096) return;
  PaginaCatalogo &pagina = operacaoCatalogo.pagina;
  if (!pagina.concluida) { pagina.passo(); return; }
  JsonDocument doc; prepararMensagem(doc, "evento");
  doc["id"] = operacaoCatalogo.solicitacao; doc["fase"] = "concluido";
  if (pagina.falhou) {
    doc["codigo"] = "FLASH_LISTAGEM"; doc["origem"] = "Flash"; doc["fase"] = "falhou";
    doc["descricao"] = "Erro ao percorrer diretorio; operacao interrompida, sem declarar pagina completa.";
    doc["excluidos"] = operacaoCatalogo.excluidos; doc["protegidos"] = operacaoCatalogo.protegidos;
    doc["falhas"] = operacaoCatalogo.falhas; doc["todos_excluidos"] = false;
    concluirCatalogo(doc); return;
  }
  if (operacaoCatalogo.tipo == OperacaoCatalogo::LISTAR) {
    if (operacaoCatalogo.revisao != revisaoCatalogo) {
      doc["codigo"] = "CATALOGO_ALTERADO"; doc["origem"] = "Flash"; doc["fase"] = "falhou";
      doc["descricao"] = "Catalogo mudou durante a pagina; reinicie a listagem.";
    } else {
      doc["codigo"] = "LISTA_ENSAIOS"; doc["catalogo_revisao"] = revisaoCatalogo;
      doc["apos"] = pagina.apos; doc["mais"] = pagina.mais;
      doc["proximo"] = pagina.quantidade ? pagina.ids[pagina.quantidade - 1] : pagina.apos;
      JsonArray itens = doc["ensaios"].to<JsonArray>();
      for (uint8_t i = 0; i < pagina.quantidade; i++) {
        InformacaoArquivo info = consultarArquivo(pagina.ids[i]);
        adicionarDetalhes(itens.add<JsonObject>(), pagina.ids[i], info, false);
      }
    }
    concluirCatalogo(doc); return;
  }
  if (operacaoCatalogo.indiceExclusao < pagina.quantidade) {
    const char *id = pagina.ids[operacaoCatalogo.indiceExclusao++];
    bool protegido = !strcmp(id, operacaoCatalogo.protecaoGravacao) || !strcmp(id, operacaoCatalogo.protecaoEnvio) || !strcmp(id, operacaoCatalogo.protecaoPacote);
    const char *codigo = excluirArquivo(id, protegido);
    if (!strcmp(codigo, "EXCLUIDO")) operacaoCatalogo.excluidos++;
    else if (!strcmp(codigo, "ENSAIO_PROTEGIDO")) operacaoCatalogo.protegidos++;
    else operacaoCatalogo.falhas++;
    doc["codigo"] = "EXCLUSAO_RESULTADO"; doc["fase"] = "andamento";
    doc["sequencia"] = ++operacaoCatalogo.resultados;
    resultadoExclusao(doc["resultado"].to<JsonObject>(), id, codigo);
    publicar(doc); return;
  }
  if (pagina.mais) {
    char apos[32]; strcpy(apos, pagina.ids[pagina.quantidade - 1]);
    if (pagina.iniciar(apos, LIMITE_BLOCO)) { operacaoCatalogo.indiceExclusao = 0; return; }
    doc["codigo"] = "FLASH_DIRETORIO"; doc["origem"] = "Flash";
    doc["descricao"] = "Exclusao interrompida ao abrir proxima pagina."; doc["fase"] = "falhou";
  } else doc["codigo"] = "EXCLUSAO_TOTAL_CONCLUIDA";
  doc["excluidos"] = operacaoCatalogo.excluidos; doc["protegidos"] = operacaoCatalogo.protegidos;
  doc["falhas"] = operacaoCatalogo.falhas;
  doc["todos_excluidos"] = !operacaoCatalogo.protegidos && !operacaoCatalogo.falhas && doc["fase"] == "concluido";
  doc["catalogo_revisao"] = revisaoCatalogo;
  concluirCatalogo(doc);
}

void adicionarEstadoCatalogo(JsonDocument &doc) {
  doc["catalogo"]["ocupado"] = operacaoCatalogo.tipo != OperacaoCatalogo::NENHUMA;
  doc["catalogo"]["solicitacao_id"] = operacaoCatalogo.solicitacao;
  doc["catalogo"]["operacao"] = exclusaoTotalAtiva() ? "EXCLUIR_TODOS" :
    operacaoCatalogo.tipo == OperacaoCatalogo::LISTAR ? "LISTAR_ENSAIOS" : "NENHUMA";
  doc["catalogo"]["excluidos"] = operacaoCatalogo.excluidos;
  doc["catalogo"]["protegidos"] = operacaoCatalogo.protegidos;
  doc["catalogo"]["falhas"] = operacaoCatalogo.falhas;
}
