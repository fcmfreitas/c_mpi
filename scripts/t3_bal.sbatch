#!/bin/bash
# Versao balanceada (secao 6, item 3): P = 2, 4, 8, 16, 31 e 32, $REPS repeticoes
# cada, mais uma execucao com --rank-stats por P (distribuicao da carga com P=31).
# Depois, a mesma versao com --isend (MPI_Isend + MPI_Request_free, como no codigo
# de Radenski) em 2 nos, para medir o efeito de enviar sem esperar (ver sort_mpi_bal.c).
#SBATCH --job-name=t3_bal
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

for P in $PROCS_BAL; do
    echo "----- balanceada P=$P -----"
    for rep in $(seq "$REPS"); do
        run_mpi "$P" 0 ./sort_mpi_bal "$N"
    done
    run_mpi "$P" 0 ./sort_mpi_bal "$N" --rank-stats
done

for P in $PROCS_ISEND; do
    echo "----- balanceada com --isend P=$P -----"
    for rep in $(seq "$REPS"); do
        run_mpi "$P" 0 ./sort_mpi_bal "$N" --isend
    done
    run_mpi "$P" 0 ./sort_mpi_bal "$N" --isend --rank-stats
done

echo "===== fim em $(date) ====="
