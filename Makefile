CC := mpicc
# A sequencial usa o compilador por tras do mpicc (confira com "mpicc --version") e as mesmas flags.
SEQ_CC ?= gcc
CFLAGS := -std=c17 -O3 -march=native -Wall -Wextra -Wpedantic
LDLIBS := -lX11 -lm
NP ?= 4

# No cluster normalmente nao ha X11: compile com "make X11=0".
ifeq ($(X11),0)
CFLAGS += -DNO_X11
LDLIBS := -lm
endif

COMMON := julia_common.c julia_common.h julia_viewer.c julia_viewer.h

.PHONY: all clean run run-seq

all: julia_mpi julia_seq

julia_mpi: julia_mpi.c $(COMMON)
	$(CC) $(CFLAGS) julia_mpi.c julia_common.c julia_viewer.c -o $@ $(LDLIBS)

julia_seq: julia_seq.c $(COMMON)
	$(SEQ_CC) $(CFLAGS) julia_seq.c julia_common.c julia_viewer.c -o $@ $(LDLIBS)

run: julia_mpi
	mpirun -n $(NP) ./julia_mpi --window --frames 2000 $(ARGS)

run-seq: julia_seq
	./julia_seq --window --frames 2000 $(ARGS)

clean:
	rm -f julia_mpi julia_seq
