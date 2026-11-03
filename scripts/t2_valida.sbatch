#!/bin/bash
# Equivalencia da escalabilidade fraca: roda a sequencial uma vez com cada entrada
# da fraca (height = 720 * P) para registrar iteracoes e checksum de referencia,
# que devem coincidir com os da versao MPI (results/t2_fraca.*.out). So importa o
# resultado, nao o tempo, por isso pode rodar em qualquer no (1 no exclusivo) e
# nao recompila (usa os binarios compilados pelos jobs de medicao nos mesmos nos).
#SBATCH --job-name=t2_valida
#SBATCH --nodes=1
#SBATCH --exclusive
#SBATCH --time=06:00:00
#SBATCH --output=results/%x.%j.out
#SBATCH --no-requeue

cd "$SLURM_SUBMIT_DIR" || exit 1
# A sequencial roda no no que o Slurm der a este job (nao no atlantica07, que esta ocupado pelas medicoes).
export FIRST_NODE=$SLURMD_NODENAME
source scripts/config.sh
source scripts/lib.sh
job_header

for P in $PROCS; do
    mark exp=valida P=1 chunk=0 height="$((HEIGHT * P))" rep=1
    run_seq --height "$((HEIGHT * P))"
done

echo "===== fim em $(date) ====="
