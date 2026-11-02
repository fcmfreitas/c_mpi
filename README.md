# Conjunto de Julia em C com MPI

Trabalho 2: o fractal do T1 (`c_openmp`), agora o conjunto de Julia,
paralelizado com MPI. Cada pixel `z0` itera `z = z^2 + c` com `c` constante.

## Constante c

Padrao `-0.123 + 0.745i` ("coelho de Douady"), que fica **dentro** do conjunto
de Mandelbrot: a imagem tem interior, cujos pixels custam `max_iter` cada, e o
tempo cresce com `max_iter` (x3,95 de iteracoes para x4 de `max_iter`). Isso
permite calibrar a entrada e fazer escalabilidade fraca por `max_iter`, como no
T1. Com `c` fora do conjunto (ex.: `-0.8 + 0.156i`) nao ha interior e o custo
para de crescer com `max_iter`.

## Esquema MPI

Mestre/trabalhador com distribuicao dinamica de linhas (o equivalente ao
`schedule(dynamic)` escolhido no T1):

- o rank 0 nao calcula: envia `{primeira linha, linhas}` (`TAG_WORK`) e entrega
  o proximo bloco a quem acabou de devolver o anterior;
- o trabalhador responde com uma mensagem `{primeira linha, pixels...}`
  (`TAG_PIXELS`); o mestre recebe com folga de `--chunk` linhas e descobre o
  tamanho real com `MPI_Get_count`, porque o ultimo bloco pode ser menor;
- `TAG_DONE` encerra o quadro do trabalhador; a soma de iteracoes vem por
  `MPI_Reduce`;
- um `MPI_Bcast` no inicio de cada quadro informa `c` ou manda encerrar.

Com P processos ha P-1 trabalhadores, por isso o modo `mpi` exige P >= 2.
`--mode seq` calcula tudo no rank 0 e serve de referencia T1.

## Medicao de tempo

`t1` e `t2` sao lidos no rank 0 com `MPI_Wtime`, depois de um `MPI_Barrier`.
A regiao medida inclui distribuicao, calculo, recepcao dos pixels, a reducao
das iteracoes e o checksum; exclui janela X11, pausa e CSV.

## Compilacao e execucao

```sh
make                              # com janela X11
make X11=0                        # sem X11 (cluster)
make run NP=4                     # janela: c gira em torno da origem

mpirun -n 1 ./julia --mode seq --no-window --max-iter 4000
mpirun -n 8 ./julia --no-window --max-iter 4000 --rank-stats
srun -N 2 -n 8 ./julia --no-window --max-iter 4000 --csv data/mpi_8.csv
```

Acima de 8 processos nesta maquina (8 nucleos fisicos), use
`mpirun --use-hwthread-cpus -n 16 ...`.

O CSV traz `frame,c_re,c_im,mode,procs,width,height,max_iter,chunk,seconds,iterations,checksum`.

## Validacao

`checksum` e `iterations` devem coincidir entre `seq` e `mpi` para qualquer P e
`--chunk`. Com `--max-iter 4000` e o `c` padrao: `iterations=537691205`,
`checksum=f8b746cef3e07759`.

O `max_iter` da entrada de referencia (30-60 s no sequencial) ainda precisa
ser calibrado para o Julia.
