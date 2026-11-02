#define _POSIX_C_SOURCE 200809L

#include <math.h>
#include <mpi.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef NO_X11
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

enum { TAG_WORK = 1, TAG_DONE, TAG_PIXELS };
static const double SCALE = 4.0; /* largura visivel do plano complexo */

typedef struct {
    int width, height, max_iter, frames, chunk;
    double c_re, c_im;
    bool seq, window, rank_stats;
    const char *csv_path;
} Options;

static void usage(const char *program) {
    fprintf(stderr,
            "Uso: mpirun -n P %s [opcoes]\n"
            "  --mode seq|mpi         seq: so o rank 0 calcula; mpi (padrao, P >= 2): mestre + P-1 trabalhadores\n"
            "  --c-re X --c-im Y      Constante c do Julia (padrao: -0.123 + 0.745i)\n"
            "  --width N --height N   Resolucao (padrao: 1280x720)\n"
            "  --max-iter N           Limite de iteracoes por pixel (padrao: 500)\n"
            "  --frames N             Quadros calculados (padrao: 1)\n"
            "  --chunk N              Linhas por tarefa enviada a um trabalhador (padrao: 1)\n"
            "  --no-window            Nao abre janela; indicado para medir\n"
            "  --csv ARQUIVO          Salva tempos brutos por quadro\n"
            "  --rank-stats           Mostra iteracoes calculadas por rank\n",
            program);
}

static bool parse_int(const char *text, int *out) {
    char *end;
    long value = strtol(text, &end, 10);
    if (*text == '\0' || *end != '\0' || value <= 0 || value > INT32_MAX) return false;
    *out = (int)value;
    return true;
}

static bool parse_double(const char *text, double *out) {
    char *end;
    *out = strtod(text, &end);
    return *text != '\0' && *end == '\0';
}

static bool parse_options(int argc, char **argv, Options *o) {
    /*
     * c padrao ("coelho de Douady") fica dentro do conjunto de Mandelbrot: a
     * imagem tem interior, que custa max_iter por pixel, entao o tempo cresce
     * com max_iter. Com c fora do conjunto o custo para de crescer.
     */
    *o = (Options){.width = 1280, .height = 720, .max_iter = 500, .frames = 1, .chunk = 1,
                   .c_re = -0.123, .c_im = 0.745, .window = true};
    for (int i = 1; i < argc; ++i) {
        const char *name = argv[i];
        if (strcmp(name, "--no-window") == 0) { o->window = false; continue; }
        if (strcmp(name, "--rank-stats") == 0) { o->rank_stats = true; continue; }
        if (i + 1 >= argc) return false;
        const char *value = argv[++i];
        bool ok;
        if (strcmp(name, "--mode") == 0) {
            o->seq = strcmp(value, "seq") == 0;
            ok = o->seq || strcmp(value, "mpi") == 0;
        }
        else if (strcmp(name, "--c-re") == 0) ok = parse_double(value, &o->c_re);
        else if (strcmp(name, "--c-im") == 0) ok = parse_double(value, &o->c_im);
        else if (strcmp(name, "--width") == 0) ok = parse_int(value, &o->width);
        else if (strcmp(name, "--height") == 0) ok = parse_int(value, &o->height);
        else if (strcmp(name, "--max-iter") == 0) ok = parse_int(value, &o->max_iter);
        else if (strcmp(name, "--frames") == 0) ok = parse_int(value, &o->frames);
        else if (strcmp(name, "--chunk") == 0) ok = parse_int(value, &o->chunk);
        else if (strcmp(name, "--csv") == 0) { o->csv_path = value; ok = true; }
        else ok = false;
        if (!ok) return false;
    }
#ifdef NO_X11
    o->window = false;
#endif
    return true;
}

static uint32_t color_from_iterations(int iterations, int max_iter) {
    if (iterations == max_iter) return 0x000000;
    double t = (double)iterations / max_iter;
    uint32_t red = (uint32_t)(9.0 * (1.0 - t) * t * t * t * 255.0);
    uint32_t green = (uint32_t)(15.0 * (1.0 - t) * (1.0 - t) * t * t * 255.0);
    uint32_t blue = (uint32_t)(8.5 * (1.0 - t) * (1.0 - t) * (1.0 - t) * t * 255.0);
    return (red << 16) | (green << 8) | blue;
}

/* Itera z = z^2 + c a partir de z0 = pixel ate |z| >= 2 ou max_iter. */
static int julia_iterations(double zx, double zy, double cx, double cy, int max_iter) {
    int iteration = 0;
    while (zx * zx + zy * zy < 4.0 && iteration < max_iter) {
        double next_zx = zx * zx - zy * zy + cx;
        zy = 2.0 * zx * zy + cy;
        zx = next_zx;
        ++iteration;
    }
    return iteration;
}

/* Calcula a linha y (imagem centrada na origem) e devolve a soma das iteracoes. */
static uint64_t render_row(uint32_t *row, int y, const Options *o, double cx, double cy) {
    const double step = SCALE / o->width;
    uint64_t total = 0;
    for (int x = 0; x < o->width; ++x) {
        int iterations = julia_iterations((x - o->width / 2.0) * step, (y - o->height / 2.0) * step, cx, cy, o->max_iter);
        row[x] = color_from_iterations(iterations, o->max_iter);
        total += (uint64_t)iterations;
    }
    return total;
}

static uint64_t render_all(uint32_t *pixels, const Options *o, double cx, double cy) {
    uint64_t total = 0;
    for (int y = 0; y < o->height; ++y) total += render_row(pixels + (size_t)y * o->width, y, o, cx, cy);
    return total;
}

static uint64_t image_checksum(const uint32_t *pixels, const Options *o) {
    uint64_t checksum = 1469598103934665603ULL;
    for (size_t i = 0; i < (size_t)o->width * o->height; ++i) {
        checksum ^= pixels[i];
        checksum *= 1099511628211ULL;
    }
    return checksum;
}

/* Envia {primeira linha, linhas} ao trabalhador; sem linhas restantes, envia TAG_DONE. */
static bool send_task(int worker, int *next_row, const Options *o) {
    if (*next_row >= o->height) {
        MPI_Send(NULL, 0, MPI_INT, worker, TAG_DONE, MPI_COMM_WORLD);
        return false;
    }
    int task[2] = {*next_row, o->height - *next_row < o->chunk ? o->height - *next_row : o->chunk};
    MPI_Send(task, 2, MPI_INT, worker, TAG_WORK, MPI_COMM_WORLD);
    *next_row += task[1];
    return true;
}

/*
 * Mestre: distribuicao dinamica. Quem devolve um bloco recebe o proximo, entao
 * as linhas caras do interior do conjunto nao ficam presas a um so rank.
 * Cada resposta e {primeira linha, pixels...}; o mestre recebe com folga de
 * chunk linhas e pergunta ao status quantos elementos chegaram, porque o
 * ultimo bloco pode ser menor.
 */
static void master(uint32_t *pixels, uint32_t *block, const Options *o, int procs) {
    int next_row = 0, active = 0;
    for (int worker = 1; worker < procs; ++worker) active += send_task(worker, &next_row, o);

    while (active > 0) {
        MPI_Status status;
        int received;
        MPI_Recv(block, 1 + o->chunk * o->width, MPI_UINT32_T, MPI_ANY_SOURCE, TAG_PIXELS, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_UINT32_T, &received);
        memcpy(pixels + (size_t)block[0] * o->width, block + 1, (size_t)(received - 1) * sizeof(*block));
        if (!send_task(status.MPI_SOURCE, &next_row, o)) --active;
    }
}

/* Trabalhador: calcula blocos ate receber TAG_DONE; devolve suas iteracoes no quadro. */
static uint64_t worker(uint32_t *block, const Options *o, double cx, double cy) {
    uint64_t total = 0;
    for (;;) {
        int task[2];
        MPI_Status status;
        MPI_Recv(task, 2, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        if (status.MPI_TAG == TAG_DONE) return total;

        block[0] = (uint32_t)task[0];
        for (int row = 0; row < task[1]; ++row) {
            total += render_row(block + 1 + (size_t)row * o->width, task[0] + row, o, cx, cy);
        }
        MPI_Send(block, 1 + task[1] * o->width, MPI_UINT32_T, 0, TAG_PIXELS, MPI_COMM_WORLD);
    }
}

#ifndef NO_X11
typedef struct { Display *display; Window window; XImage *image; } Viewer;

static bool viewer_open(Viewer *v, uint32_t *pixels, const Options *o) {
    v->display = XOpenDisplay(NULL);
    if (v->display == NULL) return false;
    int screen = DefaultScreen(v->display);
    v->window = XCreateSimpleWindow(v->display, RootWindow(v->display, screen), 0, 0, o->width, o->height, 1,
                                    BlackPixel(v->display, screen), WhitePixel(v->display, screen));
    XStoreName(v->display, v->window, "Julia C + MPI (tecla para sair)");
    XSelectInput(v->display, v->window, ExposureMask | KeyPressMask);
    XMapWindow(v->display, v->window);
    v->image = XCreateImage(v->display, DefaultVisual(v->display, screen), DefaultDepth(v->display, screen), ZPixmap,
                            0, (char *)pixels, o->width, o->height, 32, 0);
    return true;
}

/* Desenha o quadro e devolve false se alguma tecla foi pressionada. */
static bool viewer_show(Viewer *v) {
    XPutImage(v->display, v->window, DefaultGC(v->display, DefaultScreen(v->display)), v->image, 0, 0, 0, 0,
              v->image->width, v->image->height);
    XFlush(v->display);
    nanosleep(&(struct timespec){.tv_nsec = 33000000}, NULL);
    bool running = true;
    while (XPending(v->display) > 0) {
        XEvent event;
        XNextEvent(v->display, &event);
        if (event.type == KeyPress) running = false;
    }
    return running;
}

static void viewer_close(Viewer *v) {
    v->image->data = NULL; /* os pixels sao do programa, nao do XImage */
    XDestroyImage(v->image);
    XDestroyWindow(v->display, v->window);
    XCloseDisplay(v->display);
}
#else
typedef int Viewer;
static bool viewer_open(Viewer *v, uint32_t *pixels, const Options *o) { (void)v; (void)pixels; (void)o; return false; }
static bool viewer_show(Viewer *v) { (void)v; return false; }
static void viewer_close(Viewer *v) { (void)v; }
#endif

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int rank, procs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &procs);

    /* Todos os ranks leem os mesmos argumentos e chegam as mesmas opcoes. */
    Options o;
    if (!parse_options(argc, argv, &o) || (!o.seq && procs < 2)) {
        if (rank == 0) usage(argv[0]);
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    /* block guarda {primeira linha, ate chunk linhas}; a imagem inteira so existe no rank 0. */
    uint32_t *block = malloc((1 + (size_t)o.chunk * o.width) * sizeof(*block));
    uint32_t *pixels = rank == 0 ? calloc((size_t)o.width * o.height, sizeof(*pixels)) : NULL;
    uint64_t *per_rank = rank == 0 ? calloc((size_t)procs, sizeof(*per_rank)) : NULL;
    FILE *csv = rank == 0 && o.csv_path != NULL ? fopen(o.csv_path, "w") : NULL;
    if (block == NULL || (rank == 0 && (pixels == NULL || per_rank == NULL || (o.csv_path != NULL && csv == NULL)))) {
        fprintf(stderr, "rank %d: nao foi possivel alocar memoria ou abrir o CSV.\n", rank);
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
    Viewer viewer = {0};
    if (rank == 0 && o.window && !viewer_open(&viewer, pixels, &o)) {
        fprintf(stderr, "Nao foi possivel abrir o display X11. Use --no-window para medir.\n");
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
    if (csv != NULL) fprintf(csv, "frame,c_re,c_im,mode,procs,width,height,max_iter,chunk,seconds,iterations,checksum\n");

    /* Na janela, c gira em torno da origem mantendo |c|; sem janela fica fixo. */
    double c_re = o.c_re, c_im = o.c_im;
    double angle = atan2(c_im, c_re), radius = hypot(c_re, c_im);
    bool running = true;
    for (int frame = 0;; ++frame) {
        /* O rank 0 decide se ha mais um quadro e com qual c; os demais recebem. */
        double params[3] = {running && frame < o.frames, c_re, c_im};
        MPI_Bcast(params, 3, MPI_DOUBLE, 0, MPI_COMM_WORLD);
        if (params[0] == 0.0) break;
        double cx = params[1], cy = params[2];

        /* t1 e t2 lidos no mesmo processo (rank 0), depois de todos chegarem a barreira. */
        MPI_Barrier(MPI_COMM_WORLD);
        double start = MPI_Wtime();
        uint64_t local = 0, total = 0;
        if (o.seq) { if (rank == 0) local = render_all(pixels, &o, cx, cy); }
        else if (rank == 0) master(pixels, block, &o, procs);
        else local = worker(block, &o, cx, cy);
        MPI_Reduce(&local, &total, 1, MPI_UINT64_T, MPI_SUM, 0, MPI_COMM_WORLD);
        uint64_t checksum = rank == 0 ? image_checksum(pixels, &o) : 0;
        double seconds = MPI_Wtime() - start;

        if (o.rank_stats) MPI_Gather(&local, 1, MPI_UINT64_T, per_rank, 1, MPI_UINT64_T, 0, MPI_COMM_WORLD);
        if (rank != 0) continue;

        const char *mode = o.seq ? "seq" : "mpi";
        int used = o.seq ? 1 : procs;
        printf("frame=%d c=%.6f%+.6fi mode=%s procs=%d chunk=%d time=%.6f s iterations=%llu checksum=%016llx\n", frame,
               cx, cy, mode, used, o.chunk, seconds, (unsigned long long)total, (unsigned long long)checksum);
        if (csv != NULL) fprintf(csv, "%d,%.9f,%.9f,%s,%d,%d,%d,%d,%d,%.9f,%llu,%016llx\n", frame, cx, cy, mode, used,
                                 o.width, o.height, o.max_iter, o.chunk, seconds, (unsigned long long)total,
                                 (unsigned long long)checksum);
        for (int r = 0; o.rank_stats && r < procs; ++r) printf("  rank=%d iterations=%llu\n", r, (unsigned long long)per_rank[r]);

        if (o.window) {
            running = viewer_show(&viewer);
            angle += 0.005;
            c_re = radius * cos(angle);
            c_im = radius * sin(angle);
        }
    }

    if (rank == 0 && o.window) viewer_close(&viewer);
    if (csv != NULL) fclose(csv);
    free(per_rank);
    free(pixels);
    free(block);
    MPI_Finalize();
    return EXIT_SUCCESS;
}
