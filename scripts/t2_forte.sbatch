#!/bin/bash
# T1 (sequencial, 10x) e escalabilidade forte: entrada de referencia fixa,
# P = 2, 4, 8, 16, 32 e 64 processos, 10 repeticoes cada, com --rank-stats
# (tarefas, iteracoes e tempo ocupado por escravo, e instante de liberacao).
# P=64 (2 processos por thread de hardware) roda com yield quando ocioso; a
# variante "poll" (sem yield) mostra o custo da espera ativa do MPI nesse caso.
#SBATCH --job-name=t2_forte
#SBATCH --nodes=2
#SBATCH --nodelist=atlantica05,atlantica06
#SBATCH --exclusive
#SBATCH --time=02:00:00
#SBATCH --output=results/%x.%j.out
#SBATCH --no-requeue

cd "$SLURM_SUBMIT_DIR" || exit 1
source scripts/config.sh
source scripts/lib.sh
job_header
build

echo "----- T1: sequencial, $REPS repeticoes -----"
for rep in $(seq "$REPS"); do
    mark exp=seq P=1 chunk=0 height="$HEIGHT" rep="$rep"
    run_seq --height "$HEIGHT"
done

echo "----- escalabilidade forte: chunk=$CHUNK, $REPS repeticoes por P -----"
for P in $PROCS; do
    for rep in $(seq "$REPS"); do
        mark exp=forte P="$P" chunk="$CHUNK" height="$HEIGHT" rep="$rep"
        run_mpi "$P" --height "$HEIGHT" --chunk "$CHUNK" --rank-stats
    done
done

echo "----- P=64 sem yield (espera ativa), $REPS repeticoes -----"
for rep in $(seq "$REPS"); do
    mark exp=forte_poll P=64 chunk="$CHUNK" height="$HEIGHT" rep="$rep"
    YIELD=0 run_mpi 64 --height "$HEIGHT" --chunk "$CHUNK" --rank-stats
done

echo "===== fim em $(date) ====="
