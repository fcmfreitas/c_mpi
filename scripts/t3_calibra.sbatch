#!/bin/bash
# Calibracao e testes rapidos, antes das medicoes:
#  1. hardware dos nos (lscpu) e compilacao;
#  2. todas as configuracoes de processos com um vetor pequeno (TEST_N): confere que
#     cada uma termina com check=OK e o mesmo checksum da sequencial, e mostra em
#     que no ficou cada rank (--rank-stats), inclusive P=1023;
#  3. T1 (Bubble Sort do vetor inteiro) para cada tamanho de CALIB_N, uma vez cada,
#     para escolher N com T1 entre 30 e 60 s.
#SBATCH --job-name=t3_calibra
#SBATCH --nodes=2
#SBATCH --nodelist=atlantica07,atlantica08
#SBATCH --exclusive
#SBATCH --time=01:00:00
#SBATCH --output=results/%x.%j.out
#SBATCH --no-requeue

cd "$SLURM_SUBMIT_DIR" || exit 1
source scripts/config.sh
source scripts/lib.sh
job_header
build

TEST_N=${TEST_N:-20000}
echo "----- testes com n=$TEST_N -----"
run_seq "$TEST_N"
for P in $PROCS_ORIG $PROCS_OVER; do
    N=$TEST_N run_mpi "$P" 1 ./sort_mpi "$TEST_N" "$(N=$TEST_N delta_for "$P")" --rank-stats
done
for P in $PROCS_BAL; do
    run_mpi "$P" 0 ./sort_mpi_bal "$TEST_N" --rank-stats
done
for P in $PROCS_ISEND; do
    run_mpi "$P" 0 ./sort_mpi_bal "$TEST_N" --isend --rank-stats
done

echo "----- T1 para cada tamanho candidato -----"
for n in $CALIB_N; do
    run_seq "$n"
done

echo "===== fim em $(date) ====="
