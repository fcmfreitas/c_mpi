#!/bin/bash
# Referencias sequenciais (secao 6, item 1), $REPS repeticoes cada, no primeiro no:
#  - T1: Bubble Sort do vetor inteiro;
#  - T1^delta da versao original: a mesma arvore, para o delta de cada P usado
#    (PROCS_ORIG e PROCS_OVER; P=1 tem delta = N, que e o proprio T1);
#  - T1^delta da versao balanceada: a mesma arvore de sort_mpi_bal com P processos
#    (com P potencia de 2 ela coincide com a arvore de delta = ceil(N/P); com P=31 nao).
#SBATCH --job-name=t3_seq
#SBATCH --nodes=1
#SBATCH --nodelist=atlantica07
#SBATCH --exclusive
#SBATCH --time=03:00:00
#SBATCH --output=results/%x.%j.out
#SBATCH --no-requeue

cd "$SLURM_SUBMIT_DIR" || exit 1
source scripts/config.sh
source scripts/lib.sh
job_header
build

echo "----- T1, $REPS repeticoes -----"
for rep in $(seq "$REPS"); do
    run_seq "$N"
done

echo "----- T1^delta da versao original -----"
for P in $PROCS_ORIG $PROCS_OVER; do
    [ "$P" -eq 1 ] && continue
    for rep in $(seq "$REPS"); do
        run_seq "$N" "$(delta_for "$P")"
    done
done

echo "----- T1^delta da versao balanceada -----"
for P in $PROCS_BAL; do
    for rep in $(seq "$REPS"); do
        run_seq "$N" --balanced "$P"
    done
done

echo "===== fim em $(date) ====="
