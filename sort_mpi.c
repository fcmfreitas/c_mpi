// T3 - ordenacao por divisao e conquista com MPI: versao original (Figura 1 do enunciado).
//
//   mpirun -n P ./sort_mpi n delta [--rank-stats]
//
// SPMD: todos os processos rodam este programa, e o papel de cada um sai do rank.
// A arvore e binaria e numerada como um heap:
//
//                 0                 pai(r)    = (r - 1) / 2
//             1       2             filhos(r) = 2r + 1 e 2r + 2
//           3   4   5   6           nivel(r)  = floor(log2(r + 1))
//
// Quem recebe um pedaco maior que delta divide: manda uma metade para cada filho,
// espera os dois pedacos ordenados e intercala. Quem recebe um pedaco <= delta
// conquista com o Bubble Sort. So as folhas ordenam; os processos internos ficam
// parados esperando os filhos. Com P = 2^k - 1 e delta = ceil(2n / (P + 1)), a
// arvore ocupa exatamente os P processos.
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sort_common.h"
#include "sort_mpi_common.h"

// Percorre a arvore que n e delta formam, sem ordenar nada, contando os processos
// usados e o maior rank necessario. Todos os processos fazem a mesma conta e chegam
// a mesma conclusao, entao ninguem fica esperando uma mensagem que nao vem.
static void tree_shape(int size, int delta, int rank, int nprocs, int *used, int *max_rank) {
    (*used)++;
    if (rank > *max_rank) *max_rank = rank;
    if (size <= delta || rank >= nprocs) return;  // folha, ou ja faltou processo
    tree_shape(size / 2, delta, 2 * rank + 1, nprocs, used, max_rank);
    tree_shape(size - size / 2, delta, 2 * rank + 2, nprocs, used, max_rank);
}

static int heap_level(int rank) {
    int level = 0;
    while (rank > 0) {
        rank = (rank - 1) / 2;
        level++;
    }
    return level;
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int rank, nprocs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

    // Argumentos: n e delta (posicionais) e --rank-stats (opcional).
    int print_stats = 0;
    const char *positional[2];
    int npositional = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--rank-stats") == 0) {
            print_stats = 1;
        } else if (npositional < 2) {
            positional[npositional++] = argv[i];
        } else {
            npositional = 3;  // argumento sobrando
        }
    }
    if (npositional != 2) {
        if (rank == 0) fprintf(stderr, "uso: mpirun -n P %s n delta [--rank-stats]\n", argv[0]);
        MPI_Finalize();
        return 1;
    }
    int n = parse_positive(positional[0], "n");
    int delta = parse_positive(positional[1], "delta");

    int used = 0, max_rank = 0;
    tree_shape(n, delta, 0, nprocs, &used, &max_rank);
    if (used != nprocs || max_rank >= nprocs) {
        if (rank == 0) {
            fprintf(stderr,
                    "a arvore de n=%d e delta=%d usa %d processos (ranks ate %d), mas ha %d.\n"
                    "Com P = 2^k - 1 processos, use delta = ceil(2n / (P + 1)) = %lld.\n",
                    n, delta, used, max_rank, nprocs,
                    (2LL * n + nprocs) / (nprocs + 1));
        }
        MPI_Finalize();
        return 1;
    }

    int parent = (rank - 1) / 2;
    int left_child = 2 * rank + 1;
    int right_child = 2 * rank + 2;
    RankStats stats = {0};
    stats.depth = heap_level(rank);

    int *vetor = NULL;
    int tam = 0;
    if (rank == 0) {  // sou a raiz: gero o vetor, fora da regiao medida
        tam = n;
        vetor = malloc((size_t)tam * sizeof(int));
        init_decreasing(vetor, tam);
    }

    // Todos chegam aqui antes de a raiz disparar o relogio: a medicao nao inclui
    // o tempo de inicializacao dos outros processos.
    MPI_Barrier(MPI_COMM_WORLD);
    double t_start = MPI_Wtime();

    // 1. Recebo o vetor do pai. O tamanho nao e combinado antes: MPI_Probe espera a
    //    mensagem chegar sem recebe-la, e MPI_Get_count diz quantos MPI_INT ela tem.
    if (rank != 0) {
        MPI_Status status;
        MPI_Probe(parent, TAG_DOWN, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_INT, &tam);
        vetor = malloc((size_t)tam * sizeof(int));
        MPI_Recv(vetor, tam, MPI_INT, parent, TAG_DOWN, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
    double t = MPI_Wtime();
    stats.received = tam;
    stats.wait_parent = t - t_start;

    // 2. Dividir ou conquistar?
    if (tam <= delta) {
        bubble_sort(vetor, tam);  // conquisto
        stats.bubbled = tam;
        stats.bubble = MPI_Wtime() - t;
    } else {
        // Divido. As duas metades saem antes de qualquer espera, para os dois filhos
        // trabalharem ao mesmo tempo. Os envios partem direto de dentro do vetor
        // (&vetor[meio]): nao ha copia para um buffer separado.
        int meio = tam / 2;
        MPI_Send(&vetor[0], meio, MPI_INT, left_child, TAG_DOWN, MPI_COMM_WORLD);
        MPI_Send(&vetor[meio], tam - meio, MPI_INT, right_child, TAG_DOWN, MPI_COMM_WORLD);
        double t_sent = MPI_Wtime();
        stats.send = t_sent - t;

        // Os pedacos ordenados voltam para o mesmo lugar de onde sairam.
        MPI_Recv(&vetor[0], meio, MPI_INT, left_child, TAG_UP, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(&vetor[meio], tam - meio, MPI_INT, right_child, TAG_UP, MPI_COMM_WORLD,
                 MPI_STATUS_IGNORE);
        double t_back = MPI_Wtime();
        stats.wait_children = t_back - t_sent;

        int *tmp = malloc((size_t)tam * sizeof(int));
        merge_halves(vetor, tam, tmp);  // intercalo
        free(tmp);
        stats.merge = MPI_Wtime() - t_back;
    }

    // 3. Devolvo o pedaco ordenado ao pai.
    if (rank != 0) {
        double t_up = MPI_Wtime();
        MPI_Send(vetor, tam, MPI_INT, parent, TAG_UP, MPI_COMM_WORLD);
        stats.send_up = MPI_Wtime() - t_up;
    }
    double t_end = MPI_Wtime();
    stats.total = t_end - t_start;

    // Na raiz, t_end - t_start vai do inicio da distribuicao ate o vetor inteiro ordenado.
    int code = finish_run("orig", rank, nprocs, n, delta, t_end - t_start, vetor, &stats,
                          print_stats);
    free(vetor);
    MPI_Finalize();
    return code;
}
