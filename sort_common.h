// Rotinas comuns as versoes sequencial e MPI do T3 (ordenacao por divisao e conquista).
// Nao dependem de MPI: a sequencial e compilada com gcc puro.
#ifndef SORT_COMMON_H
#define SORT_COMMON_H

#include <stdint.h>

// v = n, n-1, ..., 1: ordem decrescente, o pior caso do Bubble Sort.
void init_decreasing(int *v, int n);

// Bubble Sort com parada antecipada (para quando uma passada nao troca nada).
void bubble_sort(int *v, int n);

// Intercala as metades ja ordenadas v[0, n/2) e v[n/2, n) usando tmp (n posicoes)
// como rascunho e copia o resultado de volta para v. A metade direita tem
// n - n/2 elementos, a mesma divisao feita na hora de dividir o vetor.
void merge_halves(int *v, int n, int *tmp);

// Como o vetor inicial e n, n-1, ..., 1, o resultado correto (o mesmo da versao
// sequencial) e exatamente 1, 2, ..., n. Devolve -1 se v e esse vetor, ou o
// primeiro indice errado.
int check_result(const int *v, int n);

// FNV-1a de 64 bits sobre os elementos: impresso por todas as versoes, para que a
// igualdade com a sequencial fique registrada nas saidas.
uint64_t checksum(const int *v, int n);

// Converte argv para inteiro > 0 ou encerra com mensagem de erro.
int parse_positive(const char *text, const char *name);

// Nivel mais alto do processo `rank` na arvore balanceada (Radenski): o menor l com
// 2^l > rank. O processo 0 comeca no nivel 0; quem tem nivel l recebeu o vetor no
// nivel l-1 e, a partir dele, divide com ajudantes rank + 2^l, rank + 2^(l+1), ...
int balanced_top_level(int rank);

#endif
