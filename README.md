# Conjunto de Julia em C com MPI (mestre/escravo)

Trabalho 2: o fractal do T1 (`c_openmp`), agora o conjunto de Julia,
paralelizado com MPI no modelo mestre/escravo. Cada pixel `z0` itera
`z = z^2 + c` com `c` constante.

## Arquivos

| Arquivo | Conteudo |
|---|---|
| `julia_common.c/.h` | opcoes, calculo de uma linha (`render_row`), checksum e relatorio; usado pelas duas versoes |
| `julia_seq.c` | versao sequencial de referencia (T1), **sem MPI** |
| `julia_mpi.c` | versao mestre/escravo e ping-pong |
| `julia_viewer.c/.h` | janela X11 opcional (`--window`), fora da regiao medida |

## Constante c

Padrao `-0.123 + 0.745i` ("coelho de Douady"), que fica **dentro** do conjunto
de Mandelbrot: a imagem tem interior, cujos pixels custam `max_iter` cada, e o
tempo cresce com `max_iter`, o que serve para calibrar a entrada. Com `c` fora
do conjunto (ex.: `-0.8 + 0.156i`) nao ha interior e o custo para de crescer
com `max_iter`.

## Modelo mestre/escravo

- **Tarefa:** um bloco de `--chunk` linhas consecutivas. O saco e o indice da
  proxima linha livre no mestre; ha `ceil(height / chunk)` tarefas e a ultima
  pode ser menor.
- **Tags:** `TAG_REQUEST` (escravo pede o primeiro trabalho), `TAG_PIXELS`
  (resultado `{primeira linha, pixels...}`, que vale como novo pedido),
  `TAG_WORK` (tarefa `{primeira linha, linhas}`) e `TAG_DONE` (saco vazio).
- O escravo toma a iniciativa: comeca com `TAG_REQUEST`.
- O mestre recebe com `MPI_ANY_SOURCE`/`MPI_ANY_TAG`, copia o bloco para a
  linha `block[0]` (ordem original) e responde ao mesmo escravo com a proxima
  tarefa ou com `TAG_DONE`. O mestre nao calcula, por isso e preciso P >= 2.
- Um `MPI_Bcast` no inicio de cada quadro informa `c` ou manda encerrar.

## Medicao de tempo

- MPI: `MPI_Wtime` no mestre, depois de um `MPI_Barrier`, do inicio da
  distribuicao ate a chegada do ultimo resultado.
- Sequencial: `clock_gettime(CLOCK_MONOTONIC)` em volta do calculo das linhas.
- Nas duas, ficam de fora a alocacao, o checksum, as estatisticas e a saida.

## Compilacao

```sh
make                  # julia_mpi (com X11) e julia_seq
make X11=0            # sem X11 (cluster)
make run NP=4         # MPI com janela: c gira em torno da origem
make run-seq          # sequencial com janela
```

A sequencial usa `gcc` com as mesmas flags da paralela (`-O3 -march=native`).
No cluster, confira se `mpicc --version` e `gcc --version` mostram o mesmo
compilador; se nao, use `make SEQ_CC=<compilador do mpicc>`.

## Execucao e experimentos

Cada execucao imprime uma linha `frame=... time=... iterations=... checksum=...`.
`--frames N` repete o calculo N vezes no mesmo processo, e `--csv ARQ` grava
os tempos brutos.

```sh
# T1 sequencial (calibrar --max-iter para 30-60 s no cluster)
./julia_seq --max-iter 4000

# Escalabilidade forte
mpirun -n 8 ./julia_mpi --max-iter 4000 --chunk 4

# Escalabilidade fraca: height = 720 * P, mesma largura e max_iter
mpirun -n 8 ./julia_mpi --max-iter 4000 --chunk 4 --height 5760
./julia_seq --max-iter 4000 --height 5760     # confere iterations/checksum

# Granularidade e balanceamento (por escravo: host, tarefas, iteracoes,
# tempo ocupado, tempo medio por tarefa e instante em que foi liberado)
mpirun -n 32 ./julia_mpi --max-iter 4000 --chunk 4 --rank-stats

# Ida e volta das mensagens de uma tarefa, sem calculo, com cada escravo
mpirun -n 32 ./julia_mpi --chunk 4 --pingpong 1000
```

### Regra de crescimento da escalabilidade fraca

A regiao visivel do plano e fixa (`[-2, 2] x [-1.125, 1.125]`). Aumentar
`--height` so torna a amostragem vertical mais densa, entao o numero de
tarefas cresce na proporcao de `height` e o custo medio de uma linha fica o
mesmo. Medido com `--max-iter 1000`:

| height | iteracoes | razao |
|---|---|---|
| 720 | 136180205 | 1,00 |
| 1440 | 272233905 | 2,00 |
| 2880 | 544621529 | 4,00 |
| 5760 | 1089182411 | 8,00 |

### Granularidade

O custo de uma tarefa e `per_task` (de `--rank-stats`). O custo da comunicacao
e `round_trip` (de `--pingpong`, com o mesmo `--chunk`). Os escravos no mesmo
no do mestre medem a comunicacao intra-no; os do outro no, a inter-no.

## Validacao

`iterations` e `checksum` devem coincidir entre `julia_seq` e `julia_mpi` para
qualquer P, `--chunk` e `--height`. Exemplos:

- `--max-iter 500`: `iterations=69261705`, `checksum=982001c4f55f08af`.
- `--max-iter 4000`: `iterations=537691205`, `checksum=f8b746cef3e07759`.

O CSV traz `frame,c_re,c_im,mode,procs,width,height,max_iter,chunk,seconds,iterations,checksum`.
