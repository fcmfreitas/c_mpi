# Ordenacao por divisao e conquista com MPI (T3)

Trabalho 3: ordenar um vetor em ordem decrescente (pior caso do Bubble Sort) com
uma arvore de processos MPI. Quem recebe um pedaco grande divide; quem recebe um
pedaco pequeno conquista com o Bubble Sort, e os pais intercalam na volta.
Enunciado: `t3-mpi-divisao-conquista-v2.pdf`.

## Arquivos

| Arquivo | Conteudo |
| --- | --- |
| `sort_common.c/.h` | Bubble Sort, intercalacao, vetor inicial, verificacao e checksum (sem MPI) |
| `sort_seq.c` | referencias sequenciais: T1 e T1^delta das duas arvores |
| `sort_mpi.c` | versao original (Figura 1): arvore em heap, so as folhas ordenam |
| `sort_mpi_bal.c` | versao balanceada (ideia de Radenski): o pai fica com uma metade |
| `sort_mpi_common.c/.h` | tags, estatisticas por processo (`--rank-stats`) e saida da raiz |
| `scripts/` | `sbatch` das medicoes, `config.sh`, `lib.sh` e `analyze.py` |
| `results/` | saidas brutas do Slurm (`t3_*.<job>.out`) e `resumo.md` |

## Do programa sequencial para o MPI

No sequencial, a divisao e conquista e uma recursao: `ordena(v)` chama
`ordena(metade1)` e `ordena(metade2)` e intercala. No MPI, cada chamada recursiva
vira um **processo**, e a pilha de chamadas vira uma **arvore de processos**:

| Sequencial | MPI |
| --- | --- |
| chamar `ordena(metade)` | `MPI_Send(metade, filho)` |
| receber o parametro `v, n` | `MPI_Probe` + `MPI_Get_count` + `MPI_Recv` do pai |
| `return` com a metade ordenada | `MPI_Send(pedaco, pai)` |
| usar o valor de retorno | `MPI_Recv(&v[meio], filho)` |
| as chamadas rodam uma depois da outra | os filhos rodam ao mesmo tempo |

Pontos que mudam a forma de pensar:

- **SPMD:** um unico executavel, iniciado P vezes (`srun -n P` / `mpirun -n P`).
  Todos rodam o mesmo `main`; o que cada um faz sai do `rank` (0..P-1). Ninguem
  "chama" outro processo: todos ja existem desde o inicio, e os que nao sao a
  raiz comecam parados em `MPI_Probe`, esperando o pai mandar trabalho.
- **Memoria distribuida:** cada processo tem a sua propria memoria, mesmo que
  estejam no mesmo no. Um ponteiro do pai nao vale nada no filho. Por isso nao da
  para passar "uma view" (`v + meio`) como no sequencial: os elementos precisam ser
  **copiados** por mensagem. O que da para evitar e copia *extra*: o envio parte
  direto de `&vetor[meio]` (sem buffer intermediario) e a resposta volta direto
  para `&vetor[meio]`.
- **O tamanho vem na mensagem:** o filho nao sabe quanto vai receber.
  `MPI_Probe` espera a mensagem chegar sem recebe-la, `MPI_Get_count` diz quantos
  `MPI_INT` ela tem, o filho aloca exatamente isso e so entao faz o `MPI_Recv`.
- **Tags** separam o sentido: `TAG_DOWN` (pai -> filho) e `TAG_UP` (filho -> pai).

### Quanto dado trafega entre os nos?

Sim, o MPI copia os dados, inclusive entre os nos. Mas o volume e pequeno:

- Com N ~ 150 mil inteiros (o vetor inteiro ocupa ~600 KB), a raiz manda ~300 KB
  para cada filho. Em cada nivel da arvore desce o vetor inteiro (somado entre os
  processos daquele nivel) e sobe o vetor inteiro: o total e 2 x 600 KB por nivel.
- Entre nos a rede e Gigabit Ethernet (~117 MB/s): 300 KB levam ~3 ms. Dentro do
  no, a memoria compartilhada (BTL `vader`) copia a GB/s.
- O calculo cai com o **quadrado** do pedaco (Bubble Sort), e a comunicacao so
  **linearmente**: com P=31 cada folha leva T1/256 (~0,2 s se T1 = 45 s), e o
  caminho da raiz ate uma folha e de volta move ~2 x 600 KB (~10 ms). Com poucos
  processos a comunicacao some; com centenas de processos o calculo fica tao
  pequeno que ela passa a pesar. Isso e parte da analise.

### Envio bloqueante x nao bloqueante (por que a balanceada usa MPI_Send)

`MPI_Send` so retorna quando o buffer pode ser reaproveitado; para mensagens
grandes, o Open MPI espera o receptor estar pronto (rendezvous) e transfere.
`MPI_Isend` so *inicia* o envio e retorna na hora. Mas, sem uma thread de
progresso, o Open MPI so move os dados quando o processo que envia esta dentro de
alguma chamada MPI. Teste local (rank 0 faz `MPI_Isend` de 500 KB e calcula 2 s):

| transporte | o receptor recebeu em |
| --- | --- |
| TCP (como entre os nos) | 2,0 s (so quando o emissor voltou ao MPI) |
| memoria compartilhada (`vader`) | 0,002 s |

No codigo de Radenski o pai faz `MPI_Isend` da metade e logo comeca a ordenar a
sua. Entre nos, o ajudante so receberia a metade quando o pai terminasse. Por isso
`sort_mpi_bal` usa `MPI_Send` por padrao (o ajudante ja esta parado em
`MPI_Probe`, entao custa so a transferencia), e `--isend` reproduz o esquema de
Radenski (`MPI_Isend` + `MPI_Request_free`) para medir a diferenca
(`scripts/t3_bal.sbatch` roda os dois).

## As duas versoes

**Original (`sort_mpi n delta`).** Arvore em heap: pai `(r-1)/2`, filhos `2r+1`
e `2r+2`. Quem recebe mais que `delta` elementos manda as duas metades aos filhos
(as duas antes de esperar qualquer resposta), espera e intercala. So as
`(P+1)/2` folhas ordenam; os processos internos ficam parados. Com P = 2^k - 1 e
`delta = ceil(2n/(P+1))` a arvore ocupa exatamente os P processos; antes de
comecar, todos os processos conferem isso e encerram com mensagem se nao bater
(em vez de alguem ficar esperando para sempre).

**Balanceada (`sort_mpi_bal n`).** Ao dividir no nivel l, o processo r manda a
metade direita para o ajudante `r + 2^l` e continua com a esquerda, dividindo de
novo no nivel seguinte enquanto houver ajudante (`r + 2^l < P`). Todos os
processos ordenam uma folha. Com P potencia de 2 as folhas tem n/P elementos; com
P = 31, o rank 15 nao tem ajudante no ultimo nivel e fica com o dobro (n/16).

## Decisoes de medicao

- **`-O2`, nao `-O3`.** Em `-O3` (e em `-O2` no gcc >= 12) o gcc vetoriza a troca
  do Bubble Sort com load/store de 8 bytes que se sobrepoem entre iteracoes, e o
  processador para a cada iteracao (*store forwarding*). No gcc 12 local, `-O3`
  ficou 7x mais lento que sem essa vetorizacao. O cluster usa gcc 9.4, que nao
  vetoriza em `-O2`, o nivel citado na errata do enunciado.
- **Distribuicao cyclic** dos ranks entre os 2 nos (`DIST` em `config.sh`): as
  folhas da arvore original sao os ultimos ranks, e com distribuicao em bloco
  quase todas cairiam no segundo no.
- **Ate 8 processos em um no**, 1 por nucleo fisico; 9-16 em 2 nos, 1 por nucleo
  fisico; 17-32 em 2 nos, 1 por thread (HT); acima de 32, `--overcommit` e
  `--cpu-bind=none`, para o escalonador do Linux poder mover os processos.
- **`OMPI_MCA_mpi_yield_when_idle` sempre explicito** (errata): 0 nas series
  1-31 e na balanceada; 0 e 1 na serie de 63 a 1023. Cada linha `result` imprime
  o valor em vigor.
- **Regiao medida:** `MPI_Wtime` na raiz, depois de um `MPI_Barrier`, do inicio da
  distribuicao ate a raiz ter o vetor inteiro ordenado. A inicializacao do vetor
  fica de fora. Na sequencial, `clock_gettime` em volta da ordenacao.
- **Equivalencia:** toda execucao confere que o resultado e 1, 2, ..., n (o vetor
  inicial e n, ..., 1, entao este e o resultado da sequencial) e imprime
  `check=OK` e um checksum FNV-1a, que deve ser o mesmo em todas as versoes.

## Compilacao e teste local

```sh
make                 # sort_seq (gcc), sort_mpi e sort_mpi_bal (mpicc)
make test N=40000 NP=7   # as 5 versoes com a mesma entrada: check=OK e mesmo checksum
```

Execucao manual:

```sh
./sort_seq 150000                      # T1
./sort_seq 150000 18750                # T1^delta da arvore original com P=15
./sort_seq 150000 --balanced 31        # T1^delta da arvore balanceada com P=31
mpirun -n 15 ./sort_mpi 150000 18750 --rank-stats
mpirun -n 31 ./sort_mpi_bal 150000 --rank-stats
```

## No cluster Atlantica (LAD)

Nos fixos `atlantica07,atlantica08` (`--exclusive`), compilacao dentro do job
(`-march=native` do no de computo). Submeter a partir da raiz do repositorio;
cada saida vai para `results/<job>.<id>.out`. O limite e de 2 jobs na fila por
usuario, e os jobs recompilam no mesmo diretorio, entao encadeie com
`--dependency`:

```sh
sbatch scripts/t3_calibra.sbatch   # 1o: testes de todos os P com n pequeno e T1 para cada N candidato
# escolher N (T1 entre 30 e 60 s) e gravar em scripts/config.sh
j=$(sbatch --parsable scripts/t3_seq.sbatch)                          # T1 e T1^delta, 10x cada
sbatch --dependency=afterany:$j scripts/t3_orig.sbatch                # P = 1, 3, 7, 15, 31
# quando um terminar:
j=$(sbatch --parsable scripts/t3_bal.sbatch)                          # P = 2, 4, 8, 16, 31, 32 (+ --isend)
sbatch --dependency=afterany:$j scripts/t3_over.sbatch                # P = 63..1023, yield 0 e 1
```

Analise: `python3 scripts/analyze.py` le o job mais recente de cada experimento e
gera `results/resumo.md`: mediana, minimo e maximo, S = T1/TP, S^delta =
T1^delta/TP, E = S^delta/P (S^delta/32 acima de 32 processos), T1/T1^delta e a
carga por nivel da arvore (das execucoes com `--rank-stats`).
