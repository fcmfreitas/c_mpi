#include "sort_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void init_decreasing(int *v, int n) {
    for (int i = 0; i < n; i++) {
        v[i] = n - i;
    }
}

void bubble_sort(int *v, int n) {
    int swapped = 1;
    for (int pass = 0; pass < n - 1 && swapped; pass++) {
        swapped = 0;
        // Depois de `pass` passadas, os `pass` maiores ja estao no fim.
        for (int j = 0; j < n - pass - 1; j++) {
            if (v[j] > v[j + 1]) {
                int temp = v[j];
                v[j] = v[j + 1];
                v[j + 1] = temp;
                swapped = 1;
            }
        }
    }
}

void merge_halves(int *v, int n, int *tmp) {
    int middle = n / 2;
    int i = 0;       // proximo da metade esquerda v[0, middle)
    int j = middle;  // proximo da metade direita  v[middle, n)
    int k = 0;
    while (i < middle && j < n) {
        tmp[k++] = (v[i] <= v[j]) ? v[i++] : v[j++];
    }
    while (i < middle) {
        tmp[k++] = v[i++];
    }
    while (j < n) {
        tmp[k++] = v[j++];
    }
    memcpy(v, tmp, (size_t)n * sizeof(int));
}

int check_result(const int *v, int n) {
    for (int i = 0; i < n; i++) {
        if (v[i] != i + 1) {
            return i;
        }
    }
    return -1;
}

uint64_t checksum(const int *v, int n) {
    uint64_t hash = 1469598103934665603ULL;
    for (int i = 0; i < n; i++) {
        hash ^= (uint32_t)v[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

int parse_positive(const char *text, const char *name) {
    char *end;
    long value = strtol(text, &end, 10);
    if (*end != '\0' || value <= 0 || value > 2000000000L) {
        fprintf(stderr, "%s invalido: '%s' (esperado inteiro > 0)\n", name, text);
        exit(1);
    }
    return (int)value;
}

int balanced_top_level(int rank) {
    int level = 0;
    while ((1 << level) <= rank) {
        level++;
    }
    return level;
}
