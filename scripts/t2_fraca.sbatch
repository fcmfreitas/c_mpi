#!/bin/bash
# Escalabilidade fraca: a partir da entrada de referencia, o numero de tarefas
# (linhas) cresce na proporcao de P, com o mesmo custo medio por linha
# (height = 720 * P, mesma largura e max_iter), na mesma serie de processos,
# 10 repeticoes cada. T1 e o da sequencial com a entrada de referencia (t2_forte).
#SBATCH --job-name=t2_fraca
#SBATCH --nodes=2
#SBATCH --nodelist=atlantica07,atlantica08
#SBATCH --exclusive
#SBATCH --time=04:00:00
#SBATCH --output=results/%x.%j.out
#SBATCH --no-requeue

cd "$SLURM_SUBMIT_DIR" || exit 1
source scripts/config.sh
source scripts/lib.sh
job_header
build

echo "----- escalabilidade fraca: chunk=$CHUNK, height=$HEIGHT*P, $REPS repeticoes por P -----"
for P in $PROCS; do
    for rep in $(seq "$REPS"); do
        mark exp=fraca P="$P" chunk="$CHUNK" height="$((HEIGHT * P))" rep="$rep"
        run_mpi "$P" --height "$((HEIGHT * P))" --chunk "$CHUNK" --rank-stats
    done
done

echo "===== fim em $(date) ====="
