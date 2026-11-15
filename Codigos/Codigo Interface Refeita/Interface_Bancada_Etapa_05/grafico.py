"""Gráfico de histórico: usa as amostras guardadas, sem regenerar a curva."""

import tkinter as tk
from dados_memoria import numero


class GraficoHistorico(tk.Canvas):
    def __init__(self, parent, fonte):
        super().__init__(parent, width=1, height=1, bd=0, highlightthickness=0)
        self.fonte = fonte
        self.teste = None
        self.cores = None
        self.bind("<Configure>", lambda evento: self.desenhar())

    def exibir(self, teste):
        self.teste = teste
        self.desenhar()

    def aplicar_tema(self, cores):
        self.cores = cores
        self.configure(background=cores["superficie"])
        self.desenhar()

    def desenhar(self):
        self.delete("all")
        if not self.teste or not self.cores:
            return
        c = self.cores
        largura, altura = self.winfo_width(), self.winfo_height()
        if largura < 120 or altura < 85:
            self.create_text(largura / 2, altura / 2, text="Amplie para ver o gráfico",
                             fill=c["secundario"], width=max(70, largura - 10), font=(self.fonte, 9))
            return
        esquerda, direita, topo, base = 60, largura - 22, 28, altura - 38
        tempos, valores = zip(*self.teste.amostras)
        tmin, tmax = min(0, tempos[0]), max(1, tempos[-1])
        ymin, ymax = min(0, min(valores)), max(0, max(valores))
        faixa = max(ymax - ymin, 1)
        ymax += faixa * 0.12
        if ymin < 0:
            ymin -= faixa * 0.08

        def ponto(t, valor):
            return (esquerda + (t - tmin) / (tmax - tmin) * (direita - esquerda),
                    base - (valor - ymin) / (ymax - ymin) * (base - topo))

        divisao_y = 4 if altura >= 180 else 2
        for passo in range(divisao_y + 1):
            valor = ymin + (ymax - ymin) * passo / divisao_y
            _, y = ponto(tmin, valor)
            self.create_line(esquerda, y, direita, y, fill=c["borda"], tags="eixos")
            self.create_text(esquerda - 9, y, text=numero(valor), anchor="e",
                             fill=c["secundario"], font=(self.fonte, 8), tags="eixos")
        for passo in range(5):
            tempo = tmin + (tmax - tmin) * passo / 4
            x, _ = ponto(tempo, ymin)
            self.create_line(x, topo, x, base, fill=c["borda"], dash=(2, 5), tags="eixos")
            self.create_text(x, base + 13, text=numero(tempo), fill=c["secundario"],
                             font=(self.fonte, 8), tags="eixos")
        self.create_text(esquerda, 10, text=f"Empuxo ({self.teste.unidade})", anchor="w",
                         fill=c["texto"], font=(self.fonte, 9), tags="eixos")
        self.create_text(direita, altura - 5, text="Tempo (s)", anchor="se",
                         fill=c["secundario"], font=(self.fonte, 9), tags="eixos")
        pontos = [coordenada for t, valor in self.teste.amostras for coordenada in ponto(t, valor)]
        if len(pontos) >= 4:
            self.create_line(*pontos, fill=c["curva"], width=2, smooth=False, tags="curva")
        else:
            x, y = pontos
            self.create_oval(x - 3, y - 3, x + 3, y + 3, fill=c["curva"], outline="", tags="curva")
