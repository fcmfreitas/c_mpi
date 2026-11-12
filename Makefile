CC := mpicc
# A sequencial usa o compilador por tras do mpicc (confira com "mpicc --showme:command")
# e as mesmas flags de otimizacao da paralela, como o enunciado pede.
SEQ_CC ?= gcc
# -O2, como o sequencial.c do enunciado (errata da versao 2). Nao use -O3: o gcc
# "vetoriza" a troca do Bubble Sort com um load/store de 8 bytes que se sobrepoe ao
# load da iteracao seguinte, e o processador para a cada iteracao (store forwarding
# falha). No gcc 12 local, -O3 deixou o Bubble Sort 7x mais lento. O gcc 9.4 do
# cluster nao vetoriza em -O2; o gcc >= 12 vetoriza ate em -O2.
CFLAGS := -std=c17 -O2 -march=native -Wall -Wextra -Wpedantic
NP ?= 7
N ?= 40000

COMMON := sort_common.c sort_common.h
MPI_COMMON := sort_mpi_common.c sort_mpi_common.h

.PHONY: all clean test

all: sort_seq sort_mpi sort_mpi_bal

sort_seq: sort_seq.c $(COMMON)
	$(SEQ_CC) $(CFLAGS) sort_seq.c sort_common.c -o $@

sort_mpi: sort_mpi.c $(COMMON) $(MPI_COMMON)
	$(CC) $(CFLAGS) sort_mpi.c sort_mpi_common.c sort_common.c -o $@

sort_mpi_bal: sort_mpi_bal.c $(COMMON) $(MPI_COMMON)
	$(CC) $(CFLAGS) sort_mpi_bal.c sort_mpi_common.c sort_common.c -o $@

# Teste rapido local: as tres versoes com a mesma entrada devem dar check=OK e o
# mesmo checksum. delta = ceil(2N / (NP + 1)) para a arvore ocupar os NP processos.
test: all
	./sort_seq $(N)
	./sort_seq $(N) $$(( (2 * $(N) + $(NP)) / ($(NP) + 1) ))
	mpirun --oversubscribe -n $(NP) ./sort_mpi $(N) $$(( (2 * $(N) + $(NP)) / ($(NP) + 1) ))
	./sort_seq $(N) --balanced $(NP)
	mpirun --oversubscribe -n $(NP) ./sort_mpi_bal $(N)

clean:
	rm -f sort_seq sort_mpi sort_mpi_bal
