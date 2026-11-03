# Roteiro da apresentação (10 min, 9 slides)

Divisão sugerida: **F** = Francisco (slides 1–4: problema, modelo, como rodamos),
**P** = Pedro (slides 5–9: granularidade, balanceamento, forte, fraca, conclusão).
Troquem se preferirem, mas os dois precisam falar e os dois precisam saber tudo.
Tempos somam ~9 min, deixando 1 min de folga.

| # | Slide | Quem | Tempo |
|---|---|---|---|
| 1 | Capa | F | 0:15 |
| 2 | O fractal | F | 0:45 |
| 3 | Mestre/escravo | F | 2:00 |
| 4 | Como rodamos | F | 1:15 |
| 5 | Granularidade | P | 1:30 |
| 6 | Balanceamento | P | 0:45 |
| 7 | Escalabilidade forte | P | 1:45 |
| 8 | Forte × fraca | P | 0:45 |
| 9 | Conclusão | P | 0:20 |

---

## 1. Capa (F, 15 s)
"Paralelizamos o conjunto de Julia do T1 com MPI no modelo mestre/escravo e medimos no
cluster Atlântica. O foco é explicar os números, não só mostrá-los."

## 2. O fractal (F, 45 s)
- Cada pixel itera z = z² + c até |z| > 2 ou até `max_iter` = 6000. Pixels são independentes:
  encaixa no saco de tarefas.
- **A frase importante:** "os pixels não custam o mesmo". O interior custa 6000 iterações, a
  borda quase nada; a barra à direita é o custo por linha. Isso é o que vai causar o
  desbalanceamento com tarefas grandes (slide 5).
- c = −0,123 + 0,745i fica dentro do Mandelbrot, por isso há interior e o tempo cresce com
  `max_iter` (foi assim que calibramos T1).

## 3. Mestre/escravo (F, 2 min) — slide mais cobrado, cubra os 6 requisitos
1. **Tarefa:** bloco de 4 linhas consecutivas; `--chunk` vem da linha de comando
   (`julia_common.c:50`). 2160 linhas → 540 tarefas.
2. **Saco:** o mestre guarda só `next_row`. A última tarefa pode ser menor, então qualquer
   número de linhas e qualquer P ≥ 2 funcionam (`julia_mpi.c:47-57`).
3. **Iniciativa do escravo:** começa com `TAG_REQUEST` vazia (l. 89); cada `TAG_PIXELS`
   devolvido vale como pedido da próxima (l. 103).
4. **Ordem de chegada:** `MPI_Recv(..., MPI_ANY_SOURCE, MPI_ANY_TAG, ...)` (l. 71); o
   `status.MPI_SOURCE` diz a quem responder.
5. **Ordem do saco (diga isto, o slide não mostra):** a mensagem de resultado carrega em
   `block[0]` a primeira linha da tarefa; o mestre faz `memcpy` direto para essa posição da
   imagem (l. 77). O tamanho real vem de `MPI_Get_count` (último bloco menor).
6. **Término:** saco vazio → cada pedido recebe `TAG_DONE` (l. 49); o escravo sai do laço ao ver
   a tag (l. 94); o mestre encerra quando liberou os P−1 escravos (l. 79-83).
- Mestre não calcula. Um `MPI_Bcast` no início de cada quadro manda `c` (l. 193).

## 4. Como rodamos (F, 1:15)
- **Hardware (diga o lscpu):** cada nó tem 2 Xeon E5520 de 4 núcleos com HT → **8 núcleos
  físicos e 16 threads por nó**, não 16/32 como o enunciado supõe. Por isso P=16 é "1 por
  núcleo físico", P=32 é o ponto HT e P=64 é 2 processos por thread.
- Sempre atlantica07 e 08, `--exclusive`, `sbatch`; mestre e sequencial sempre no 07; ranks
  preenchem em bloco; `--cpu-bind=cores` até P=16, `threads` em 32, `--overcommit` em 64.
- `gcc 9.4` via `mpicc` (OpenMPI 4.1.1), `-O3 -march=native` nas duas versões, compilado no
  nó de cômputo.
- **Região medida:** `MPI_Wtime` no mestre após `MPI_Barrier`, do início da distribuição ao
  último resultado; sequencial com `clock_gettime` em volta do laço de linhas. Checksum e
  saída ficam fora.
- Entrada 3840×2160, `max_iter` 6000 → T1 = 40,4 s (30–60 s pedidos). 10 execuções
  independentes; mediana, mín e máx (variação < 1% até P=32, 3% em P=64).
- Toda execução imprime iterações e checksum FNV-1a; **12 entradas, todas iguais à sequencial**.

## 5. Granularidade (P, 1:30)
- Comparamos a escolhida (4 linhas, 540 tarefas) com bem menor (1 linha, 2160) e bem maior
  (até 512 linhas, 5 tarefas).
- **Computação vs comunicação:** 94 ms de cálculo por tarefa contra 19 µs de ida e volta no
  mesmo nó e 0,65 ms entre nós (rede Gigabit; memória compartilhada é ~30× mais rápida).
  Razões 5000 e 140. Diga os dois números: o slide só tem o inter-nó.
- Por isso 1, 2, 4 e 8 linhas empatam em 1,66–1,67 s; 4 foi escolhida por cortar as mensagens
  de 2160 para 540 sem custo medido.
- **Granularidades maiores perdem por desbalanceamento, não por comunicação:** com 32 linhas
  (68 tarefas) a cauda é 1,0 s de 2,27 s; com 128 (17 tarefas) 14 escravos nunca trabalham e o
  tempo é a tarefa mais cara (4,7 s); com 512, 14,6 s.
- O mestre não é gargalo: ~20 µs por mensagem, 540 mensagens em 1,66 s → < 1% ocupado.

## 6. Balanceamento (P, 45 s)
- 31 escravos ocupados 1,63–1,65 s (99%), liberados com ±2 ms, 16–20 tarefas cada.
- **Explique as barras do 08 um pouco mais baixas:** cada tarefa deles espera 0,65 ms a mais
  (rede), então pegam 1–2 tarefas a menos. É a comunicação inter-nó aparecendo, e é tudo que
  ela custa.
- Fim do saco não pesa: as últimas linhas (borda inferior) são baratas.

## 7. Escalabilidade forte (P, 1:45) — use a decomposição E = (P−1)/P × 1/(CPU/T1) × util
- **P=2: S=1,0, E=50%.** O mestre ocupa um núcleo e não calcula; limite (P−1)/P. P=4: S=3,0
  exato.
- **P=8: S=6,6, E=83% contra 87,5% de limite.** Cada escravo ficou 5% mais lento com o nó
  cheio (74,6 → 77,9 ms por tarefa); compatível com perda de Turbo Boost. Comunicação intra-nó
  não aparece (util 100%).
- **P=16: S=14,1, E=88%.** Rede custa 1% de utilização aos 8 escravos do outro nó; mesmos
  5% por núcleo.
- **P=32: S=24,4, E=76%. Satura por hardware:** 16 núcleos físicos; CPU/T1 = 1,26 → dois
  escravos no mesmo núcleo rodam 26% mais devagar cada, logo o núcleo rende 1,59× (16 × 1,59
  ≈ 25 escravos equivalentes). HT ajuda porque z²+c é uma cadeia dependente, limitada por
  latência. Curiosidade: o escravo que divide o núcleo com o mestre é o mais rápido (84,6 ms):
  a espera ativa do mestre quase não atrapalha.
- **P=64: S=23,6.** Cada processo tem meio thread (CPU/T1 = 2,40); util 90% porque o SO fatia
  desigualmente. Sem `yield_when_idle` fica 3% pior (polling disputa CPU).

## 8. Forte × fraca (P, 45 s)
- Altura 2160·P; iterações crescem exatamente P× (medida de trabalho).
- E_fraca = 50/75/83/89/76/39%: a mesma curva da forte dentro de 2%.
- **Por quê não melhora:** a fraca só ganha quando há sobrecusto fixo para diluir; aqui
  comunicação e cauda são desprezíveis, e os fatores que limitam (mestre, Turbo, HT,
  oversubscrição) crescem com P igual nas duas. Só em 64 ela dilui o fatiamento do SO
  (util 94% vs 90%).

## 9. Conclusão (P, 20 s)
- 24× com 32 processos, 99% dos escravos calculando, 100% dos checksums iguais.
- Limites: núcleos físicos, HT e o núcleo do mestre. Não: comunicação nem balanceamento.
- Repositório com scripts sbatch, saídas brutas e `analyze.py` que regera tudo.

---

## Perguntas prováveis (os dois precisam saber responder)

**Chamadas MPI**
- `MPI_Recv` com `MPI_ANY_SOURCE`/`MPI_ANY_TAG`: por que? Atender por ordem de chegada; o
  `status` dá fonte e tag. `MPI_Get_count` dá o tamanho real (último bloco menor).
- Por que `MPI_Send` bloqueante basta? Mensagens pequenas (8 B de tarefa) e o escravo não tem
  nada a fazer enquanto espera; o mestre responde em ~20 µs.
- `MPI_Barrier` antes do `MPI_Wtime`: alinha todos antes de medir; `MPI_Wtime` só no mestre.
- `MPI_Bcast` dos parâmetros por quadro; `MPI_Gather` das estatísticas (fora da região medida)
  e dos nomes de host (`MPI_Get_processor_name`), para saber em que nó cada rank rodou.
- O escravo recebe `MPI_ANY_TAG` do rank 0 e decide pelo `status.MPI_TAG` (WORK ou DONE).

**Números**
- Por que E=50% em P=2? Mestre conta em P e não calcula.
- Por que chunk 4 e não 1? Empate em tempo; 4× menos mensagens. Bem menor = 1, bem maior = 512.
- Qual o custo da comunicação? 19 µs intra, 650 µs inter para 61 KB; 0,25 ms de latência +
  ~0,13 ms por linha de 15 KB. Para mensagens grandes a banda chega a ~400 MB/s porque os
  nós têm 4 interfaces Gigabit e o OpenMPI usa todas.
- De onde vêm 1,26 e 1,59? Soma do tempo ocupado dos escravos ÷ T1 = 1,26 (cada escravo mais
  lento); 2 escravos por núcleo → 2/1,26 = 1,59 de ganho por núcleo.
- Como sabem que o mestre não é gargalo? 540 msgs em 1,66 s, ~20 µs cada → < 1%; com 1 linha
  por tarefa (2160 msgs) ainda < 3%, e o tempo não mudou.
- Por que a fraca não é melhor? Sem sobrecusto fixo para diluir.
- O que é o checksum? FNV-1a de todos os pixels; igual em sequencial e MPI para as 12
  entradas (incluindo as da fraca, validadas com `t2_valida`).
- Como foi calibrado T1? 720p/8000 deu 6 s; 4K tem 9× pixels → 6000 iterações dá 40 s.
- O que é `yield_when_idle`? Processo ocioso cede o processador em vez de espera ativa; só
  importa com mais processos que threads (P=64).

**IA:** scripts sbatch, `analyze.py`, execução no cluster e primeira versão do relatório foram
feitos com Claude; o grupo revisou tudo e responde por cada linha.
