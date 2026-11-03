#!/bin/bash
# Calibracao e teste do pipeline (poucos minutos): hardware dos nos, mapeamento dos
# ranks nos nos/CPUs, tempo da sequencial para varios max_iter (para escolher o que
# deixa T1 entre 30 e 60 s) e uma execucao MPI curta em cada P para conferir
# iteracoes e checksum contra a sequencial.
#SBATCH --job-name=t2_calibra
#SBATCH --nodes=2
#SBATCH --nodelist=atlantica07,atlantica08
#SBATCH --exclusive
#SBATCH --time=00:40:00
#SBATCH --output=results/%x.%j.out
#SBATCH --no-requeue

cd "$SLURM_SUBMIT_DIR" || exit 1
source scripts/config.sh
source scripts/lib.sh
job_header
build

echo "----- mapeamento rank -> no/cpus para cada P -----"
for P in $PROCS; do
    echo "P=$P: srun $(srun_opts "$P")"
    # shellcheck disable=SC2046
    srun $(srun_opts "$P") bash -c 'printf "rank=%2d host=%s cpus=%s\n" "$SLURM_PROCID" "$(hostname)" "$(grep Cpus_allowed_list /proc/self/status | cut -f2)"' | sort -t= -k2 -n
done

echo "----- sequencial x max_iter (escolher max_iter com T1 entre 30 e 60 s) -----"
for mi in 2000 4000 6000 8000; do
    mark exp=calibra P=1 chunk=0 height="$HEIGHT" rep=1
    run srun -N 1 -w "$FIRST_NODE" -n 1 --cpu-bind=cores ./julia_seq --width "$WIDTH" --height "$HEIGHT" --max-iter "$mi"
done

echo "----- MPI curto em cada P (max_iter=500): iteracoes e checksum devem bater com a sequencial -----"
mark exp=calibra_seq500 P=1 chunk=0 height="$HEIGHT" rep=1
run srun -N 1 -w "$FIRST_NODE" -n 1 --cpu-bind=cores ./julia_seq --width "$WIDTH" --height "$HEIGHT" --max-iter 500
for P in $PROCS; do
    mark exp=calibra_mpi P="$P" chunk="$CHUNK" height="$HEIGHT" rep=1
    MAX_ITER=500 run_mpi "$P" --height "$HEIGHT" --chunk "$CHUNK" --rank-stats
done

echo "----- P=64 sem yield (2 processos por thread: o polling do MPI compete com o calculo) -----"
mark exp=calibra_mpi_poll P=64 chunk="$CHUNK" height="$HEIGHT" rep=1
MAX_ITER=500 YIELD=0 run_mpi 64 --height "$HEIGHT" --chunk "$CHUNK" --rank-stats

echo "----- ping-pong curto, P=32 -----"
mark exp=calibra_pingpong P=32 chunk="$CHUNK" height="$HEIGHT" rep=1
run_mpi 32 --height "$HEIGHT" --chunk "$CHUNK" --pingpong 200

echo "===== fim em $(date) ====="
