// Partes comuns as duas versoes MPI (original e balanceada): estatisticas por
// processo e a saida/verificacao feitas pela raiz depois da regiao medida.
#ifndef SORT_MPI_COMMON_H
#define SORT_MPI_COMMON_H

#define TAG_DOWN 1  // pedaco descendo do pai para o filho (ou ajudante)
#define TAG_UP 2    // pedaco ordenado subindo do filho para o pai

// Tempos de cada processo, em segundos, a partir do inicio da regiao medida.
// So doubles: o MPI_Gather manda a struct como um vetor de MPI_DOUBLE.
typedef struct {
    double depth;          // nivel em que o processo recebeu o vetor (raiz = 0)
    double received;       // elementos recebidos do pai (raiz: n)
    double bubbled;        // elementos ordenados com Bubble Sort (0 se so dividiu)
    double wait_parent;    // do inicio ate ter o vetor na memoria
    double send;           // enviando metades aos filhos/ajudantes
    double bubble;         // no Bubble Sort
    double wait_children;  // parado em MPI_Recv esperando os pedacos ordenados
    double merge;          // intercalando
    double send_up;        // devolvendo o pedaco ordenado ao pai
    double total;          // do inicio ate terminar a sua parte
} RankStats;

// Chamada por todos os processos depois da regiao medida. A raiz (que tem o vetor
// ordenado em v) verifica o resultado e imprime a linha "result ...". Com
// print_stats, junta os RankStats de todos e imprime uma linha "rank ..." por
// processo, com o host em que ele rodou. Devolve o codigo de saida do programa.
int finish_run(const char *version, int rank, int nprocs, int n, int delta, double seconds,
               const int *v, const RankStats *mine, int print_stats);

#endif
