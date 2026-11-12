#!/bin/bash
# Versao original (secao 6, item 2): P = 1, 3, 7, 15 e 31, delta = ceil(2N/(P+1)),
# $REPS repeticoes cada, com espera ativa explicita (yield=0). Depois das
# repeticoes, uma execucao extra com --rank-stats por P (tempos de cada processo,
# para o balanceamento por nivel da arvore); ela nao entra nas tabelas de tempo.
#SBATCH --job-name=t3_orig
#SBATCH --nodes=2
#SBATCH --nodelist=atlantica07,atlantica08
#SBATCH --exclusive
#SBATCH --time=02:00:00
#SBATCH --output=results/%x.%j.out
#SBATCH --no-requeue

cd "$SLURM_SUBMIT_DIR" || exit 1
source scripts/config.sh
source scripts/lib.sh
job_header
build

for P in $PROCS_ORIG; do
    delta=$(delta_for "$P")
    echo "----- original P=$P delta=$delta -----"
    for rep in $(seq "$REPS"); do
        run_mpi "$P" 0 ./sort_mpi "$N" "$delta"
    done
    run_mpi "$P" 0 ./sort_mpi "$N" "$delta" --rank-stats
done

echo "===== fim em $(date) ====="
