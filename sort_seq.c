// T3 - versoes sequenciais de referencia (sem MPI).
//
//   ./sort_seq n                  T1: Bubble Sort do vetor inteiro
//   ./sort_seq n delta            T1^delta: a mesma arvore da versao original (sort_mpi)
//                                 em um so processo: divide enquanto o pedaco passa de
//                                 delta, ordena as folhas com Bubble Sort e intercala na volta
//   ./sort_seq n --balanced P     T1^delta da versao balanceada (sort_mpi_bal) com P
//                                 processos: percorre a mesma arvore, cujas folhas podem ter
//                                 tamanhos diferentes quando P nao e potencia de 2
//
// A linha "result ..." traz o formato da arvore (nos, folhas, menor e maior folha,
// profundidade) para mostrar que a referencia faz o mesmo trabalho da versao paralela.
#define _POSIX_C_SOURCE 199309L  // clock_gettime com -std=c17

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "sort_common.h"

typedef struct {
    int nodes;     // pedacos que passaram pela arvore (na original, = processos)
    int leaves;    // pedacos ordenados com Bubble Sort
    int min_leaf;
    int max_leaf;
    int depth;     // nivel da folha mais funda (raiz = 0)
} TreeShape;

static void note_leaf(TreeShape *shape, int size, int depth) {
    shape->leaves++;
    if (size < shape->min_leaf) shape->min_leaf = size;
    if (size > shape->max_leaf) shape->max_leaf = size;
    if (depth > shape->depth) shape->depth = depth;
}

// Mesma decisao de cada processo da versao original: conquista se tam <= delta,
// senao divide em v[0, n/2) e v[n/2, n) (os dois filhos) e intercala na volta.
static void tree_sort(int *v, int n, int delta, int *tmp, int depth, TreeShape *shape) {
    shape->nodes++;
    if (n <= delta) {
        bubble_sort(v, n);
        note_leaf(shape, n, depth);
        return;
    }
    int middle = n / 2;
    tree_sort(v, middle, delta, tmp, depth + 1, shape);
    tree_sort(v + middle, n - middle, delta, tmp, depth + 1, shape);
    merge_halves(v, n, tmp);
}

// Mesma recursao da versao balanceada: o processo `rank` no nivel `level` entrega a
// metade direita ao ajudante rank + 2^level (se existir) e fica com a esquerda.
static void balanced_sort(int *v, int n, int level, int rank, int procs, int *tmp,
                          TreeShape *shape) {
    int helper = rank + (1 << level);
    if (helper >= procs) {
        shape->nodes++;  // cada processo termina com exatamente uma folha
        bubble_sort(v, n);
        note_leaf(shape, n, level);
        return;
    }
    int middle = n / 2;
    balanced_sort(v, middle, level + 1, rank, procs, tmp, shape);              // fico com a esquerda
    balanced_sort(v + middle, n - middle, level + 1, helper, procs, tmp, shape);  // o ajudante faz a direita
    merge_halves(v, n, tmp);
}

static void usage(const char *prog) {
    fprintf(stderr,
            "uso: %s n                 (Bubble Sort do vetor inteiro)\n"
            "     %s n delta           (arvore da versao original)\n"
            "     %s n --balanced P    (arvore da versao balanceada com P processos)\n",
            prog, prog, prog);
    exit(1);
}

int main(int argc, char **argv) {
    if (argc < 2 || argc > 4) usage(argv[0]);
    int n = parse_positive(argv[1], "n");
    const char *mode = "bubble";
    int delta = n;   // delta >= n: a raiz conquista o vetor inteiro
    int procs = 1;
    if (argc == 3) {
        mode = "tree";
        delta = parse_positive(argv[2], "delta");
    } else if (argc == 4) {
        if (strcmp(argv[2], "--balanced") != 0) usage(argv[0]);
        mode = "balanced";
        procs = parse_positive(argv[3], "P");
    }

    int *v = malloc((size_t)n * sizeof(int));
    int *tmp = malloc((size_t)n * sizeof(int));
    if (v == NULL || tmp == NULL) {
        fprintf(stderr, "sem memoria para n=%d\n", n);
        return 1;
    }
    init_decreasing(v, n);  // fora da regiao medida
    TreeShape shape = {0, 0, n, 0, 0};

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    if (strcmp(mode, "balanced") == 0) {
        balanced_sort(v, n, 0, 0, procs, tmp, &shape);
    } else {
        tree_sort(v, n, delta, tmp, 0, &shape);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    double seconds = (double)(end.tv_sec - start.tv_sec) + (double)(end.tv_nsec - start.tv_nsec) * 1e-9;

    int bad = check_result(v, n);
    printf("result version=seq mode=%s n=%d delta=%d procs=%d nodes=%d leaves=%d min_leaf=%d "
           "max_leaf=%d depth=%d time=%.6f check=%s checksum=%016llx\n",
           mode, n, strcmp(mode, "balanced") == 0 ? 0 : delta, procs, shape.nodes, shape.leaves,
           shape.min_leaf, shape.max_leaf, shape.depth, seconds, bad < 0 ? "OK" : "FAIL",
           (unsigned long long)checksum(v, n));
    if (bad >= 0) {
        fprintf(stderr, "ERRO: v[%d] = %d, esperado %d\n", bad, v[bad], bad + 1);
    }
    free(v);
    free(tmp);
    return bad < 0 ? 0 : 2;
}
