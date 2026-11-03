# Resumo das medicoes (gerado por scripts/analyze.py)
Entrada de referencia: 3840x2160, max_iter=6000; granularidade escolhida: 4 linha(s) por tarefa.


## T1 (sequencial) e equivalencia

| linhas | max\_iter | iteracoes | checksum | exec. seq | exec. MPI | equiv. |
|---|---|---|---|---|---|---|
| 720 | 500 | 69261705 | 982001c4f55f08af | 0 | 1 | ok |
| 2160 | 500 | 623228795 | 3dfb392e16d2a46f | 1 | 1 | ok |
| 2160 | 2000 | 2429632295 | 8e8e313b3613714e | 1 | 0 | ok |
| 2160 | 4000 | 4838170295 | a1e11ca92826e0af | 1 | 0 | ok |
| 2160 | 6000 | 7246708295 | afc642cca152dc8b | 1 | 1 | ok |
| 2160 | 8000 | 9655246295 | bfc7fe2530d5e0eb | 1 | 0 | ok |

T1 = mediana 40.405 s (min 40.352, max 40.424) em 10 execucoes.

| versao | execucoes | mediana (s) | min (s) | max (s) |
|---|---|---|---|---|
| sequencial | 10 | 40,405 | 40,352 | 40,424 |


## Escalabilidade forte

| P | mediana (s) | min (s) | max (s) | S | E | CPU/T1 | util. | cauda (s) |
|---|---|---|---|---|---|---|---|---|
| 2 | 40,34 | 40,33 | 40,34 | 1,00 | 50% | 1,00 | 100% | 0,00 |
| 4 | 13,45 | 13,45 | 13,58 | 3,00 | 75% | 1,00 | 100% | 0,00 |
| 8 | 6,08 | 6,08 | 6,08 | 6,64 | 83% | 1,05 | 100% | 0,00 |
| 16 | 2,87 | 2,86 | 2,87 | 14,10 | 88% | 1,05 | 99% | 0,00 |
| 32 | 1,66 | 1,66 | 1,66 | 24,36 | 76% | 1,26 | 99% | 0,01 |
| 64 | 1,71 | 1,68 | 1,76 | 23,56 | 37% | 2,40 | 90% | 0,08 |
| 64 (polling) | 1,76 | 1,72 | 1,86 | 22,95 | 36% | 2,49 | 91% | 0,08 |

CPU/T1: soma do tempo ocupado dos escravos dividida por T1 (>1: cada nucleo calcula mais devagar que na sequencial); util.: fracao do tempo em que os escravos estao calculando; cauda: tempo entre o primeiro escravo liberado e o fim.


## Granularidade (P=32)

| linhas/tarefa | tarefas | mediana (s) | min (s) | max (s) | tarefa (ms) | ida e volta intra ($\mu$s) | inter ($\mu$s) | tarefa/inter | util. | cauda (s) |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 2160 | 1,66 | 1,66 | 1,67 | 23,6 | 12 | 248 | 95 | 99% | 0,00 |
| 2 | 1080 | 1,66 | 1,66 | 1,66 | 47,2 | 14 | 382 | 123 | 99% | 0,00 |
| 4 | 540 | 1,66 | 1,66 | 1,66 | 94,3 | 19 | 653 | 144 | 99% | 0,01 |
| 8 | 270 | 1,67 | 1,67 | 1,67 | 188,7 | 30 | 808 | 234 | 98% | 0,03 |
| 16 | 135 | 1,81 | 1,79 | 1,82 | 376,0 | 53 | 871 | 432 | 90% | 0,27 |
| 32 | 68 | 2,27 | 2,25 | 2,31 | 739,0 | 96 | 1434 | 515 | 71% | 1,02 |
| 64 | 34 | 2,58 | 2,55 | 2,62 | 1472,0 | 177 | 2596 | 567 | 63% | 2,33 |
| 128 | 17 | 4,71 | 4,44 | 4,73 | 2938,5 | 342 | 4913 | 598 | 34% | 4,70 |
| 256 | 9 | 8,97 | 7,89 | 9,01 | 5395,7 | 985 | 9600 | 562 | 18% | 8,97 |
| 512 | 5 | 14,56 | 13,47 | 15,71 | 9362,7 | 3020 | 18905 | 495 | 10% | 14,56 |

tarefa: tempo medio de processar uma tarefa (tempo ocupado / tarefas, todos os escravos e repeticoes); ida e volta: mediana do --pingpong entre o mestre e os escravos do mesmo no (intra) e do outro no (inter).


## Balanceamento por escravo (P=32, 4 linha(s) por tarefa, mediana de 10 execucoes)

| rank | no | tarefas | iter. (M) | ocupado (s) | liberado em (s) |
|---|---|---|---|---|---|
| 1 | atlantica07 | 18 | 234 | 1,649 | 1,655 |
| 2 | atlantica07 | 20 | 265 | 1,651 | 1,655 |
| 3 | atlantica07 | 18 | 234 | 1,650 | 1,655 |
| 4 | atlantica07 | 18 | 233 | 1,650 | 1,655 |
| 5 | atlantica07 | 18 | 233 | 1,649 | 1,654 |
| 6 | atlantica07 | 18 | 234 | 1,650 | 1,655 |
| 7 | atlantica07 | 17 | 234 | 1,650 | 1,655 |
| 8 | atlantica07 | 17 | 234 | 1,651 | 1,655 |
| 9 | atlantica07 | 18 | 233 | 1,650 | 1,655 |
| 10 | atlantica07 | 18 | 233 | 1,650 | 1,654 |
| 11 | atlantica07 | 18 | 234 | 1,650 | 1,655 |
| 12 | atlantica07 | 18 | 234 | 1,651 | 1,654 |
| 13 | atlantica07 | 18 | 234 | 1,651 | 1,655 |
| 14 | atlantica07 | 17 | 234 | 1,652 | 1,655 |
| 15 | atlantica07 | 18 | 234 | 1,651 | 1,655 |
| 16 | atlantica08 | 17 | 232 | 1,638 | 1,656 |
| 17 | atlantica08 | 16 | 232 | 1,637 | 1,655 |
| 18 | atlantica08 | 17 | 232 | 1,637 | 1,657 |
| 19 | atlantica08 | 18 | 232 | 1,634 | 1,656 |
| 20 | atlantica08 | 18 | 231 | 1,632 | 1,655 |
| 21 | atlantica08 | 18 | 231 | 1,633 | 1,656 |
| 22 | atlantica08 | 18 | 232 | 1,635 | 1,656 |
| 23 | atlantica08 | 17 | 232 | 1,637 | 1,656 |
| 24 | atlantica08 | 17 | 232 | 1,634 | 1,656 |
| 25 | atlantica08 | 17 | 232 | 1,634 | 1,656 |
| 26 | atlantica08 | 18 | 231 | 1,633 | 1,655 |
| 27 | atlantica08 | 16 | 232 | 1,636 | 1,655 |
| 28 | atlantica08 | 16 | 232 | 1,636 | 1,658 |
| 29 | atlantica08 | 16 | 232 | 1,639 | 1,657 |
| 30 | atlantica08 | 16 | 233 | 1,639 | 1,657 |
| 31 | atlantica08 | 16 | 232 | 1,638 | 1,657 |

| no | escravos | tarefas | ocupado (s) | liberado em (s) |
|---|---|---|---|---|
| atlantica07 | 15 | 17--20 | 1,65--1,65 | 1,65--1,66 |
| atlantica08 | 16 | 16--18 | 1,63--1,64 | 1,65--1,66 |

T (mediana) = 1.659 s.

