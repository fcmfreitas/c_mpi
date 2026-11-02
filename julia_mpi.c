#define _POSIX_C_SOURCE 200809L

#include <mpi.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "julia_common.h"
#include "julia_viewer.h"

/*
 * Tags do protocolo mestre/escravo (todas em MPI_COMM_WORLD):
 *   TAG_REQUEST  escravo -> mestre, vazia: primeiro pedido de trabalho do quadro.
 *   TAG_PIXELS   escravo -> mestre, {primeira linha, pixels...}: resultado de uma
 *                tarefa, que vale tambem como pedido da proxima.
 *   TAG_WORK     mestre -> escravo, {primeira linha, numero de linhas}: uma tarefa.
 *   TAG_DONE     mestre -> escravo, vazia: saco vazio, o escravo encerra o quadro.
 */
enum { TAG_REQUEST = 1, TAG_PIXELS, TAG_WORK, TAG_DONE };

/* Ida e volta descartadas antes de medir o ping-pong (estabelecimento de conexao, caches). */
enum { PINGPONG_WARMUP = 10 };

/* Medidas de um escravo em um quadro, para o balanceamento e o custo de uma tarefa. */
typedef struct {
    uint64_t iterations, tasks;
    double busy; /* segundos calculando, sem contar espera por mensagens */
} WorkerStats;

static void usage(const char *program) {
    fprintf(stderr,
            "Uso: mpirun -n P %s [opcoes]   (P >= 2: mestre + P-1 escravos)\n"
            "%s"
            "  --chunk N              Linhas por tarefa (granularidade, padrao: 1)\n"
            "  --rank-stats           Mostra host, tarefas, iteracoes e tempo ocupado de cada escravo\n"
            "  --pingpong N           Nao calcula: mede N idas e voltas de mensagens de uma tarefa com cada escravo\n",
            program, COMMON_OPTIONS_HELP);
}

/*
 * Responde ao escravo com a proxima tarefa do saco: {primeira linha, linhas}.
 * A ultima tarefa pode ter menos de chunk linhas, entao height nao precisa ser
 * multiplo de chunk. Com o saco vazio envia TAG_DONE e devolve false.
 */
static bool send_task(int worker, int *next_row, const Options *o) {
    if (*next_row >= o->height) {
        MPI_Send(NULL, 0, MPI_INT, worker, TAG_DONE, MPI_COMM_WORLD);
        return false;
    }
    int remaining = o->height - *next_row;
    int task[2] = {*next_row, remaining < o->chunk ? remaining : o->chunk};
    MPI_Send(task, 2, MPI_INT, worker, TAG_WORK, MPI_COMM_WORLD);
    *next_row += task[1];
    return true;
}

/*
 * Mestre: o saco de tarefas e o indice da proxima linha livre (next_row), em
 * ordem. Atende por ordem de chegada (MPI_ANY_SOURCE): cada mensagem de um
 * escravo, pedido inicial ou resultado, e respondida com a proxima tarefa ou,
 * com o saco vazio, com TAG_DONE. O mestre nao calcula. done_at[w] guarda
 * quando o escravo w foi liberado (segundos desde start), o que mostra o
 * desbalanceamento no fim do saco.
 */
static void master(uint32_t *pixels, uint32_t *block, const Options *o, int procs, double start, double *done_at) {
    int next_row = 0, active = procs - 1;
    while (active > 0) {
        MPI_Status status;
        MPI_Recv(block, 1 + o->chunk * o->width, MPI_UINT32_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        if (status.MPI_TAG == TAG_PIXELS) {
            /* O buffer tem folga de chunk linhas; o tamanho real (ultimo bloco menor) vem do status. */
            int received;
            MPI_Get_count(&status, MPI_UINT32_T, &received);
            /* block[0] e a primeira linha da tarefa: o resultado volta para a posicao de origem. */
            memcpy(pixels + (size_t)block[0] * o->width, block + 1, (size_t)(received - 1) * sizeof(*block));
        }
        if (!send_task(status.MPI_SOURCE, &next_row, o)) {
            done_at[status.MPI_SOURCE] = MPI_Wtime() - start;
            --active;
        }
    }
}

/* Escravo: pede trabalho e calcula tarefas ate receber TAG_DONE. */
static WorkerStats worker(uint32_t *block, const Options *o, double cx, double cy) {
    WorkerStats stats = {0};
    MPI_Send(NULL, 0, MPI_UINT32_T, 0, TAG_REQUEST, MPI_COMM_WORLD);
    for (;;) {
        int task[2];
        MPI_Status status;
        MPI_Recv(task, 2, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        if (status.MPI_TAG == TAG_DONE) return stats;

        double start = MPI_Wtime();
        block[0] = (uint32_t)task[0];
        for (int row = 0; row < task[1]; ++row) {
            stats.iterations += render_row(block + 1 + (size_t)row * o->width, task[0] + row, o, cx, cy);
        }
        stats.busy += MPI_Wtime() - start;
        ++stats.tasks;
        MPI_Send(block, 1 + task[1] * o->width, MPI_UINT32_T, 0, TAG_PIXELS, MPI_COMM_WORLD);
    }
}

/*
 * --pingpong N: mede so a comunicacao de uma tarefa. Com cada escravo, o mestre
 * faz N idas e voltas com as mesmas mensagens de uma tarefa (TAG_WORK de 2 ints,
 * TAG_PIXELS de 1 + chunk*width valores), sem calculo, e imprime a media.
 * Escravos no mesmo no do mestre mostram a comunicacao intra-no; os do outro
 * no, a inter-no.
 */
static void pingpong_master(uint32_t *block, const Options *o, int procs, char (*hosts)[MPI_MAX_PROCESSOR_NAME]) {
    int task[2] = {0, o->chunk};
    int count = 1 + o->chunk * o->width;
    for (int w = 1; w < procs; ++w) {
        double start = 0.0;
        for (int i = -PINGPONG_WARMUP; i < o->pingpong; ++i) {
            if (i == 0) start = MPI_Wtime();
            MPI_Send(task, 2, MPI_INT, w, TAG_WORK, MPI_COMM_WORLD);
            MPI_Recv(block, count, MPI_UINT32_T, w, TAG_PIXELS, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        double round_trip = (MPI_Wtime() - start) / o->pingpong;
        printf("pingpong rank=%d host=%s master_host=%s chunk=%d bytes=%zu round_trip=%.9f s\n", w, hosts[w], hosts[0],
               o->chunk, (size_t)count * sizeof(*block), round_trip);
        MPI_Send(NULL, 0, MPI_INT, w, TAG_DONE, MPI_COMM_WORLD);
    }
}

static void pingpong_worker(uint32_t *block, const Options *o) {
    for (;;) {
        int task[2];
        MPI_Status status;
        MPI_Recv(task, 2, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        if (status.MPI_TAG == TAG_DONE) return;
        MPI_Send(block, 1 + task[1] * o->width, MPI_UINT32_T, 0, TAG_PIXELS, MPI_COMM_WORLD);
    }
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int rank, procs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &procs);

    /* Todos os ranks leem os mesmos argumentos e chegam as mesmas opcoes. */
    Options o;
    if (!parse_options(argc, argv, &o) || procs < 2) {
        if (rank == 0) usage(argv[0]);
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    /* block guarda {primeira linha, ate chunk linhas}; a imagem inteira so existe no mestre. */
    uint32_t *block = calloc(1 + (size_t)o.chunk * o.width, sizeof(*block));
    uint32_t *pixels = rank == 0 ? calloc((size_t)o.width * o.height, sizeof(*pixels)) : NULL;
    WorkerStats *stats = rank == 0 ? calloc((size_t)procs, sizeof(*stats)) : NULL;
    double *done_at = rank == 0 ? calloc((size_t)procs, sizeof(*done_at)) : NULL;
    char (*hosts)[MPI_MAX_PROCESSOR_NAME] = rank == 0 ? calloc((size_t)procs, sizeof(*hosts)) : NULL;
    FILE *csv = rank == 0 && o.csv_path != NULL ? open_csv(o.csv_path) : NULL;
    if (block == NULL || (rank == 0 && (pixels == NULL || stats == NULL || done_at == NULL || hosts == NULL ||
                                        (o.csv_path != NULL && csv == NULL)))) {
        fprintf(stderr, "rank %d: nao foi possivel alocar memoria ou abrir o CSV.\n", rank);
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }

    /* Nome do no de cada rank, para registrar a distribuicao dos processos. */
    char host[MPI_MAX_PROCESSOR_NAME] = {0};
    int host_length;
    MPI_Get_processor_name(host, &host_length);
    MPI_Gather(host, MPI_MAX_PROCESSOR_NAME, MPI_CHAR, hosts, MPI_MAX_PROCESSOR_NAME, MPI_CHAR, 0, MPI_COMM_WORLD);

    if (o.pingpong > 0) {
        if (rank == 0) pingpong_master(block, &o, procs, hosts);
        else pingpong_worker(block, &o);
        o.frames = 0; /* pula o laco de quadros */
        o.window = false;
    }

    Viewer *viewer = rank == 0 && o.window ? viewer_open(pixels, &o, "Julia MPI (tecla para sair)") : NULL;
    if (rank == 0 && o.window && viewer == NULL) {
        fprintf(stderr, "Nao foi possivel abrir o display X11 (ou compilado com X11=0).\n");
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }

    /* Na janela, c gira em torno da origem mantendo |c|; sem janela fica fixo. */
    double c_re = o.c_re, c_im = o.c_im;
    bool running = true;
    for (int frame = 0;; ++frame) {
        /* O mestre decide se ha mais um quadro e com qual c; os escravos recebem. */
        double params[3] = {running && frame < o.frames, c_re, c_im};
        MPI_Bcast(params, 3, MPI_DOUBLE, 0, MPI_COMM_WORLD);
        if (params[0] == 0.0) break;
        double cx = params[1], cy = params[2];

        /*
         * Regiao medida, so no mestre: do inicio da distribuicao ate a chegada do
         * ultimo resultado. A barreira alinha todos antes do inicio; a coleta de
         * estatisticas e o checksum ficam de fora.
         */
        MPI_Barrier(MPI_COMM_WORLD);
        WorkerStats local = {0};
        double seconds = 0.0;
        if (rank == 0) {
            double start = MPI_Wtime();
            master(pixels, block, &o, procs, start, done_at);
            seconds = MPI_Wtime() - start;
        }
        else {
            local = worker(block, &o, cx, cy);
        }

        /* Os nos do cluster sao homogeneos, entao a struct vai como bytes. */
        MPI_Gather(&local, sizeof(local), MPI_BYTE, stats, sizeof(local), MPI_BYTE, 0, MPI_COMM_WORLD);
        if (rank != 0) continue;

        uint64_t total = 0;
        for (int r = 1; r < procs; ++r) total += stats[r].iterations;
        report_frame(csv, &o, frame, cx, cy, "mpi", procs, o.chunk, seconds, total, image_checksum(pixels, &o));
        if (o.rank_stats) {
            printf("  rank=0 host=%s role=master\n", hosts[0]);
            for (int r = 1; r < procs; ++r) {
                double per_task = stats[r].tasks > 0 ? stats[r].busy / (double)stats[r].tasks : 0.0;
                printf("  rank=%d host=%s tasks=%llu iterations=%llu busy=%.6f s per_task=%.9f s done_at=%.6f s\n", r,
                       hosts[r], (unsigned long long)stats[r].tasks, (unsigned long long)stats[r].iterations,
                       stats[r].busy, per_task, done_at[r]);
            }
        }

        if (o.window) {
            running = viewer_show(viewer);
            viewer_next_c(&c_re, &c_im);
        }
    }

    viewer_close(viewer);
    if (csv != NULL) fclose(csv);
    free(hosts);
    free(done_at);
    free(stats);
    free(pixels);
    free(block);
    MPI_Finalize();
    return EXIT_SUCCESS;
}
