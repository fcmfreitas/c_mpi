// T3 - ordenacao por divisao e conquista com MPI: versao balanceada.
//
//   mpirun -n P ./sort_mpi_bal n [--isend] [--rank-stats]
//
// Ideia de A. Radenski ("Shared Memory, Message Passing, and Hybrid Merge Sorts for
// Standalone and Clustered SMPs", mpi_mergesort.c, GPL), reescrita para este
// trabalho: folhas com Bubble Sort, vetor decrescente e o tempo medido como na
// versao original (sort_mpi.c).
//
// Ao dividir, o processo entrega a metade DIREITA a um ajudante e fica com a
// ESQUERDA, em vez de entregar as duas e esperar. Assim todos os processos ordenam.
// No nivel l, o ajudante do processo r e r + 2^l:
//
//   nivel 0:  0 -> 1                     (0 fica com n/2,  1 recebe n/2)
//   nivel 1:  0 -> 2,  1 -> 3            (cada um fica com n/4)
//   nivel 2:  0 -> 4,  1 -> 5,  2 -> 6,  3 -> 7
//
// Ao chegar no nivel l, os ranks 0 .. 2^l - 1 ja tem um pedaco; somar 2^l a cada um
// deles da exatamente os 2^l ranks seguintes, sem repetir nem pular nenhum.
// Um processo divide enquanto o seu ajudante existir (r + 2^l < P). Com P potencia
// de 2 todos terminam com n/P elementos; senao, alguns ficam com o dobro.
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sort_common.h"
#include "sort_mpi_common.h"

// Com --isend, a metade vai com MPI_Isend + MPI_Request_free, como no codigo de
// Radenski. Sem a opcao (padrao), vai com MPI_Send. Motivo: o Open MPI so move uma
// mensagem grande quando o processo que envia esta dentro de alguma chamada MPI.
// Entre nos (TCP), um MPI_Isend seguido de um Bubble Sort longo deixa o ajudante
// esperando o Bubble Sort inteiro do pai. Na memoria compartilhada (mesmo no) o
// receptor copia sozinho e isso nao acontece. O MPI_Send so retorna depois que o
// ajudante, que ja esta parado em MPI_Probe, recebeu a metade: leva o tempo da
// transferencia (milissegundos) e garante que o ajudante comeca na hora.
static int use_isend = 0;

// Ordena v[0, n). O processo `rank` esta no nivel `level` da arvore.
static void sort_tree(int *v, int n, int *tmp, int level, int rank, int nprocs, RankStats *stats) {
    int helper = rank + (1 << level);
    if (helper >= nprocs) {  // nao ha mais ajudante: conquisto
        double t = MPI_Wtime();
        bubble_sort(v, n);
        stats->bubble += MPI_Wtime() - t;
        stats->bubbled = n;
        return;
    }

    int middle = n / 2;
    double t = MPI_Wtime();
    MPI_Request request = MPI_REQUEST_NULL;
    if (use_isend) {
        // Inicia o envio e retorna na hora; o envio segue em segundo plano.
        MPI_Isend(&v[middle], n - middle, MPI_INT, helper, TAG_DOWN, MPI_COMM_WORLD, &request);
    } else {
        MPI_Send(&v[middle], n - middle, MPI_INT, helper, TAG_DOWN, MPI_COMM_WORLD);
    }
    stats->send += MPI_Wtime() - t;

    // Fico com a metade esquerda e continuo descendo (ela pode ser dividida de novo
    // com o ajudante do proximo nivel). Isso so mexe em v[0, middle) e tmp[0, middle):
    // a metade direita, que pode estar saindo, nao e tocada.
    sort_tree(v, middle, tmp, level + 1, rank, nprocs, stats);

    if (use_isend) {
        // Libera o pedido sem esperar o envio terminar (ele termina sozinho). Nao ha
        // como testar o termino depois disso, e nao precisa: o MPI_Recv abaixo so
        // completa quando o ajudante devolve a metade ordenada, e para isso ele ja
        // recebeu a metade inteira. Entao o envio ja acabou quando o MPI_Recv grava
        // em v[middle, n), e o buffer pode ser reaproveitado.
        MPI_Request_free(&request);
    }

    t = MPI_Wtime();
    MPI_Recv(&v[middle], n - middle, MPI_INT, helper, TAG_UP, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    double t_back = MPI_Wtime();
    stats->wait_children += t_back - t;

    merge_halves(v, n, tmp);
    stats->merge += MPI_Wtime() - t_back;
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int rank, nprocs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

    int print_stats = 0;
    const char *n_text = NULL;
    int bad_args = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--rank-stats") == 0) {
            print_stats = 1;
        } else if (strcmp(argv[i], "--isend") == 0) {
            use_isend = 1;
        } else if (n_text == NULL) {
            n_text = argv[i];
        } else {
            bad_args = 1;
        }
    }
    if (n_text == NULL || bad_args) {
        if (rank == 0) fprintf(stderr, "uso: mpirun -n P %s n [--isend] [--rank-stats]\n", argv[0]);
        MPI_Finalize();
        return 1;
    }
    int n = parse_positive(n_text, "n");

    // Nivel em que este processo entra na arvore e quem o acorda: o processo que,
    // no nivel anterior, tinha este rank como ajudante.
    int level = balanced_top_level(rank);
    int parent = (rank == 0) ? -1 : rank - (1 << (level - 1));
    RankStats stats = {0};
    stats.depth = level;

    int *vetor = NULL;
    int *tmp = NULL;
    int tam = 0;
    if (rank == 0) {
        tam = n;
        vetor = malloc((size_t)tam * sizeof(int));
        tmp = malloc((size_t)tam * sizeof(int));
        init_decreasing(vetor, tam);  // fora da regiao medida
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double t_start = MPI_Wtime();

    if (rank != 0) {
        MPI_Status status;
        MPI_Probe(parent, TAG_DOWN, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_INT, &tam);
        vetor = malloc((size_t)tam * sizeof(int));
        tmp = malloc((size_t)tam * sizeof(int));
        MPI_Recv(vetor, tam, MPI_INT, parent, TAG_DOWN, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
    stats.received = tam;
    stats.wait_parent = MPI_Wtime() - t_start;

    sort_tree(vetor, tam, tmp, level, rank, nprocs, &stats);

    if (rank != 0) {
        double t_up = MPI_Wtime();
        MPI_Send(vetor, tam, MPI_INT, parent, TAG_UP, MPI_COMM_WORLD);
        stats.send_up = MPI_Wtime() - t_up;
    }
    double t_end = MPI_Wtime();
    stats.total = t_end - t_start;

    int code = finish_run(use_isend ? "bal-isend" : "bal", rank, nprocs, n, 0, t_end - t_start,
                          vetor, &stats, print_stats);
    free(vetor);
    free(tmp);
    MPI_Finalize();
    return code;
}
