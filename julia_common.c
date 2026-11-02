#include "julia_common.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

const char COMMON_OPTIONS_HELP[] =
    "  --c-re X --c-im Y      Constante c do Julia (padrao: -0.123 + 0.745i)\n"
    "  --width N --height N   Resolucao (padrao: 1280x720); --height define o numero de linhas\n"
    "  --max-iter N           Limite de iteracoes por pixel (padrao: 500)\n"
    "  --frames N             Quadros calculados em sequencia (padrao: 1)\n"
    "  --csv ARQUIVO          Salva os tempos brutos de cada quadro\n";

static bool parse_int(const char *text, int *out) {
    char *end;
    long value = strtol(text, &end, 10);
    if (*text == '\0' || *end != '\0' || value <= 0 || value > INT_MAX) return false;
    *out = (int)value;
    return true;
}

static bool parse_double(const char *text, double *out) {
    char *end;
    *out = strtod(text, &end);
    return *text != '\0' && *end == '\0';
}

bool parse_options(int argc, char **argv, Options *o) {
    /*
     * c padrao ("coelho de Douady") fica dentro do conjunto de Mandelbrot: a
     * imagem tem interior, que custa max_iter por pixel, entao o tempo cresce
     * com max_iter. Com c fora do conjunto o custo para de crescer.
     */
    *o = (Options){.width = 1280, .height = 720, .max_iter = 500, .frames = 1, .chunk = 1, .c_re = -0.123,
                   .c_im = 0.745};
    for (int i = 1; i < argc; ++i) {
        const char *name = argv[i];
        if (strcmp(name, "--window") == 0) { o->window = true; continue; }
        if (strcmp(name, "--rank-stats") == 0) { o->rank_stats = true; continue; }
        if (i + 1 >= argc) return false;
        const char *value = argv[++i];
        bool ok;
        if (strcmp(name, "--c-re") == 0) ok = parse_double(value, &o->c_re);
        else if (strcmp(name, "--c-im") == 0) ok = parse_double(value, &o->c_im);
        else if (strcmp(name, "--width") == 0) ok = parse_int(value, &o->width);
        else if (strcmp(name, "--height") == 0) ok = parse_int(value, &o->height);
        else if (strcmp(name, "--max-iter") == 0) ok = parse_int(value, &o->max_iter);
        else if (strcmp(name, "--frames") == 0) ok = parse_int(value, &o->frames);
        else if (strcmp(name, "--chunk") == 0) ok = parse_int(value, &o->chunk);
        else if (strcmp(name, "--pingpong") == 0) ok = parse_int(value, &o->pingpong);
        else if (strcmp(name, "--csv") == 0) { o->csv_path = value; ok = true; }
        else ok = false;
        if (!ok) return false;
    }
    /* Contagens de mensagens MPI sao int: a imagem inteira precisa caber em um. */
    if ((long long)o->width * o->height >= INT_MAX) return false;
    /* Um bloco maior que a imagem e so a imagem inteira em uma tarefa. */
    if (o->chunk > o->height) o->chunk = o->height;
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

uint64_t render_row(uint32_t *row, int y, const Options *o, double cx, double cy) {
    const double step_x = VIEW_WIDTH / o->width;
    const double zy = (y - o->height / 2.0) * (VIEW_HEIGHT / o->height);
    uint64_t total = 0;
    for (int x = 0; x < o->width; ++x) {
        int iterations = julia_iterations((x - o->width / 2.0) * step_x, zy, cx, cy, o->max_iter);
        row[x] = color_from_iterations(iterations, o->max_iter);
        total += (uint64_t)iterations;
    }
    return total;
}

uint64_t image_checksum(const uint32_t *pixels, const Options *o) {
    uint64_t checksum = 1469598103934665603ULL;
    for (size_t i = 0; i < (size_t)o->width * o->height; ++i) {
        checksum ^= pixels[i];
        checksum *= 1099511628211ULL;
    }
    return checksum;
}

FILE *open_csv(const char *path) {
    FILE *csv = fopen(path, "w");
    if (csv != NULL) fprintf(csv, "frame,c_re,c_im,mode,procs,width,height,max_iter,chunk,seconds,iterations,checksum\n");
    return csv;
}

void report_frame(FILE *csv, const Options *o, int frame, double cx, double cy, const char *mode, int procs,
                  int chunk, double seconds, uint64_t iterations, uint64_t checksum) {
    printf("frame=%d c=%.6f%+.6fi mode=%s procs=%d width=%d height=%d max_iter=%d chunk=%d time=%.6f s "
           "iterations=%llu checksum=%016llx\n",
           frame, cx, cy, mode, procs, o->width, o->height, o->max_iter, chunk, seconds,
           (unsigned long long)iterations, (unsigned long long)checksum);
    if (csv != NULL) {
        fprintf(csv, "%d,%.9f,%.9f,%s,%d,%d,%d,%d,%d,%.9f,%llu,%016llx\n", frame, cx, cy, mode, procs, o->width,
                o->height, o->max_iter, chunk, seconds, (unsigned long long)iterations,
                (unsigned long long)checksum);
    }
}
