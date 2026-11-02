#ifndef JULIA_VIEWER_H
#define JULIA_VIEWER_H

/*
 * Janela X11 opcional (--window), usada pelas duas versoes so para visualizar:
 * fica fora da regiao medida. Compilado com -DNO_X11, viewer_open falha.
 */

#include <stdbool.h>
#include <stdint.h>

#include "julia_common.h"

typedef struct Viewer Viewer;

/* Abre a janela sobre o buffer pixels (que continua do programa); NULL se falhar. */
Viewer *viewer_open(uint32_t *pixels, const Options *o, const char *title);

/* Desenha o quadro e devolve false se alguma tecla foi pressionada. */
bool viewer_show(Viewer *v);

void viewer_close(Viewer *v);

/* Proximo c da animacao: gira em torno da origem mantendo |c|. */
void viewer_next_c(double *c_re, double *c_im);

#endif
