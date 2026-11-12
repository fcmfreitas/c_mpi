#!/bin/bash
# Versao original com mais processos que as 32 threads de hardware (secao 6, item 4):
# P = 63, 127, 255, 511 e 1023, delta = ceil(2N/(P+1)), nos mesmos 2 nos, com
# OMPI_MCA_mpi_yield_when_idle=0 (espera ativa) e =1 (o processo parado cede o
# nucleo), os dois definidos explicitamente. $REPS repeticoes cada, mais uma com
# --rank-stats. Sem --cpu-bind (ver scripts/lib.sh): o escalonador do Linux
# distribui os processos entre os nucleos.
#SBATCH --job-name=t3_over
#SBATCH --nodes=2
#SBATCH --nodelist=atlantica07,atlantica08
#SBATCH --exclusive
#SBATCH --time=03:00:00
#SBATCH --output=results/%x.%j.out
#SBATCH --no-requeue

cd "$SLURM_SUBMIT_DIR" || exit 1
source scripts/config.sh
source scripts/lib.sh
job_header
build

for yield in 0 1; do
    for P in $PROCS_OVER; do
        delta=$(delta_for "$P")
        echo "----- original P=$P delta=$delta yield=$yield -----"
        for rep in $(seq "$REPS"); do
            run_mpi "$P" "$yield" ./sort_mpi "$N" "$delta"
        done
        run_mpi "$P" "$yield" ./sort_mpi "$N" "$delta" --rank-stats
    done
done

echo "===== fim em $(date) ====="
