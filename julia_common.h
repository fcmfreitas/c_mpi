#ifndef JULIA_COMMON_H
#define JULIA_COMMON_H

/*
 * Parte comum as versoes sequencial (julia_seq.c) e MPI (julia_mpi.c): opcoes,
 * calculo de uma linha, checksum e relatorio de cada quadro. As duas usam o
 * mesmo codigo compilado do calculo, entao a comparacao de tempos e justa.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/*
 * Regiao visivel do plano complexo: [-2, 2] x [-1.125, 1.125] (16:9). A
 * resolucao so muda a densidade de amostragem: dobrar --height dobra o numero
 * de linhas (tarefas) sem mudar o custo medio de cada linha, que e a regra de
 * crescimento da escalabilidade fraca.
 */
#define VIEW_WIDTH 4.0
#define VIEW_HEIGHT 2.25

typedef struct {
    int width, height, max_iter, frames, chunk, pingpong;
    double c_re, c_im;
    bool window, rank_stats;
    const char *csv_path;
} Options;

/* Texto de ajuda das opcoes aceitas pelas duas versoes. */
extern const char COMMON_OPTIONS_HELP[];

bool parse_options(int argc, char **argv, Options *o);

/* Calcula a linha y e devolve a soma das iteracoes dos seus pixels. */
uint64_t render_row(uint32_t *row, int y, const Options *o, double cx, double cy);

/* Hash FNV-1a da imagem: deve coincidir entre a versao sequencial e a MPI. */
uint64_t image_checksum(const uint32_t *pixels, const Options *o);

/* Abre o CSV de quadros e escreve o cabecalho; NULL se falhar. */
FILE *open_csv(const char *path);

/* Imprime a linha do quadro em stdout e, se csv != NULL, no CSV. */
void report_frame(FILE *csv, const Options *o, int frame, double cx, double cy, const char *mode, int procs,
                  int chunk, double seconds, uint64_t iterations, uint64_t checksum);

#endif
