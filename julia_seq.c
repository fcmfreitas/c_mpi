#define _POSIX_C_SOURCE 200809L

/*
 * Versao sequencial de referencia (T1 do speed-up), sem MPI: calcula as linhas
 * em ordem, direto na imagem, com o mesmo render_row dos escravos. Nao ha
 * mensagens, copias de bloco nem processo mestre.
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "julia_common.h"
#include "julia_viewer.h"

static void usage(const char *program) {
    fprintf(stderr, "Uso: %s [opcoes]\n%s", program, COMMON_OPTIONS_HELP);
}

static double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

int main(int argc, char **argv) {
    Options o;
    /* --chunk e aceito para os scripts poderem passar os mesmos argumentos, mas nao tem efeito aqui. */
    if (!parse_options(argc, argv, &o) || o.rank_stats || o.pingpong > 0) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    uint32_t *pixels = calloc((size_t)o.width * o.height, sizeof(*pixels));
    FILE *csv = o.csv_path != NULL ? open_csv(o.csv_path) : NULL;
    if (pixels == NULL || (o.csv_path != NULL && csv == NULL)) {
        fprintf(stderr, "Nao foi possivel alocar a imagem ou abrir o CSV.\n");
        return EXIT_FAILURE;
    }

    Viewer *viewer = o.window ? viewer_open(pixels, &o, "Julia sequencial (tecla para sair)") : NULL;
    if (o.window && viewer == NULL) {
        fprintf(stderr, "Nao foi possivel abrir o display X11 (ou compilado com X11=0).\n");
        return EXIT_FAILURE;
    }

    /* Na janela, c gira em torno da origem mantendo |c|; sem janela fica fixo. */
    double cx = o.c_re, cy = o.c_im;
    bool running = true;
    for (int frame = 0; running && frame < o.frames; ++frame) {
        /* Mesma regiao medida do mestre MPI: so o calculo; checksum, saida e janela ficam de fora. */
        double start = now_seconds();
        uint64_t total = 0;
        for (int y = 0; y < o.height; ++y) total += render_row(pixels + (size_t)y * o.width, y, &o, cx, cy);
        double seconds = now_seconds() - start;

        report_frame(csv, &o, frame, cx, cy, "seq", 1, 0, seconds, total, image_checksum(pixels, &o));
        if (o.window) {
            running = viewer_show(viewer);
            viewer_next_c(&cx, &cy);
        }
    }

    viewer_close(viewer);
    if (csv != NULL) fclose(csv);
    free(pixels);
    return EXIT_SUCCESS;
}
