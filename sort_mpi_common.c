#include "sort_mpi_common.h"

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sort_common.h"

#define HOST_LEN 32
#define STATS_DOUBLES ((int)(sizeof(RankStats) / sizeof(double)))

int finish_run(const char *version, int rank, int nprocs, int n, int delta, double seconds,
               const int *v, const RankStats *mine, int print_stats) {
    int bad = -1;
    if (rank == 0) {
        bad = check_result(v, n);
        // yield: como o Open MPI espera em MPI_Recv (0 = espera ativa, 1 = cede o nucleo).
        const char *yield = getenv("OMPI_MCA_mpi_yield_when_idle");
        printf("result version=%s procs=%d n=%d delta=%d yield=%s stats=%d time=%.6f check=%s "
               "checksum=%016llx\n",
               version, nprocs, n, delta, yield ? yield : "unset", print_stats, seconds,
               bad < 0 ? "OK" : "FAIL", (unsigned long long)checksum(v, n));
        if (bad >= 0) {
            fprintf(stderr, "ERRO: v[%d] = %d, esperado %d\n", bad, v[bad], bad + 1);
        }
    }

    if (print_stats) {
        char host[HOST_LEN] = {0};
        char full_name[MPI_MAX_PROCESSOR_NAME];
        int name_len;
        MPI_Get_processor_name(full_name, &name_len);
        snprintf(host, HOST_LEN, "%.*s", HOST_LEN - 1, full_name);

        RankStats *all = NULL;
        char *hosts = NULL;
        if (rank == 0) {
            all = malloc((size_t)nprocs * sizeof(RankStats));
            hosts = malloc((size_t)nprocs * HOST_LEN);
        }
        MPI_Gather(mine, STATS_DOUBLES, MPI_DOUBLE, all, STATS_DOUBLES, MPI_DOUBLE, 0, MPI_COMM_WORLD);
        MPI_Gather(host, HOST_LEN, MPI_CHAR, hosts, HOST_LEN, MPI_CHAR, 0, MPI_COMM_WORLD);
        if (rank == 0) {
            for (int r = 0; r < nprocs; r++) {
                const RankStats *s = &all[r];
                printf("rank r=%d host=%s depth=%d received=%d bubbled=%d wait_parent=%.6f send=%.6f "
                       "bubble=%.6f wait_children=%.6f merge=%.6f send_up=%.6f total=%.6f\n",
                       r, hosts + (size_t)r * HOST_LEN, (int)s->depth, (int)s->received,
                       (int)s->bubbled, s->wait_parent, s->send, s->bubble, s->wait_children,
                       s->merge, s->send_up, s->total);
            }
            free(all);
            free(hosts);
        }
    }
    fflush(stdout);
    return bad < 0 ? 0 : 2;
}
