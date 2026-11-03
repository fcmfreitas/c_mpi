# Funcoes comuns dos scripts sbatch do T2. Uso: source scripts/config.sh; source scripts/lib.sh

# O shell do sbatch nao passa pelo /etc/profile do LAD: garante o OpenMPI 4.1.1 no PATH.
MPI_HOME=/LADAPPs/OpenMPI/openmpi-4.1.1
export PATH="$MPI_HOME/bin:$PATH"
export LD_LIBRARY_PATH="$MPI_HOME/lib:$MPI_HOME/lib/openmpi:${LD_LIBRARY_PATH:-}"

# Opcoes do srun para P processos dentro do job de 2 nos exclusivos. Cada no tem
# 8 nucleos fisicos e 16 threads de hardware (lscpu no cabecalho de cada saida):
#   P <= 8 : 1 no, 1 processo por nucleo fisico (ranks 0..P-1 no primeiro no)
#   P = 16 : 2 nos x 8, 1 processo por nucleo fisico (ranks 0-7 no primeiro no, 8-15 no segundo)
#   P = 32 : 2 nos x 16, 1 processo por thread de hardware (HT)
#   P = 64 : 2 nos x 32, 2 processos por thread de hardware (--overcommit)
# O mestre (rank 0) fica sempre no primeiro no (atlantica05).
srun_opts() {
    local P=$1
    if [ "$P" -le 8 ]; then
        echo "-N 1 -n $P --ntasks-per-node=$P --cpu-bind=cores"
    elif [ "$P" -le 16 ]; then
        echo "-N 2 -n $P --ntasks-per-node=$((P / 2)) --cpu-bind=cores"
    elif [ "$P" -le 32 ]; then
        echo "-N 2 -n $P --ntasks-per-node=$((P / 2)) --cpu-bind=threads"
    else
        echo "-N 2 -n $P --ntasks-per-node=$((P / 2)) --cpu-bind=threads --overcommit"
    fi
}

# Executa um comando mostrando a linha exata (fica na saida do Slurm, para reproducao).
run() {
    echo "+ $*"
    "$@"
}

# Marca o inicio de uma execucao: scripts/analyze.py associa a linha "frame=..." seguinte a ela.
mark() {
    echo "### $*"
}

# Uma execucao MPI com P processos; os demais argumentos vao para o julia_mpi.
run_mpi() {
    local P=$1
    shift
    # shellcheck disable=SC2046
    run srun $(srun_opts "$P") ./julia_mpi --width "$WIDTH" --max-iter "$MAX_ITER" "$@"
}

# Uma execucao sequencial (T1), em um nucleo de um no exclusivo.
run_seq() {
    run srun -N 1 -n 1 --cpu-bind=cores ./julia_seq --width "$WIDTH" --max-iter "$MAX_ITER" "$@"
}

# Cabecalho da saida: job, nos, hardware (lscpu nos nos de computo), compilador e parametros.
job_header() {
    echo "===== job $SLURM_JOB_ID ($SLURM_JOB_NAME) iniciado em $(date) ====="
    echo "nodelist=$SLURM_JOB_NODELIST submit_dir=$SLURM_SUBMIT_DIR git=$(git rev-parse --short HEAD 2>/dev/null)"
    echo "----- lscpu de cada no -----"
    srun -N "$SLURM_JOB_NUM_NODES" --ntasks-per-node=1 bash -c '
        echo "[$(hostname)]"
        lscpu | grep -E "Model name|^CPU\(s\)|Thread\(s\) per core|Core\(s\) per socket|Socket\(s\)|NUMA node\(s\)|CPU MHz"
        for s in /sys/class/net/*/speed; do n=${s#/sys/class/net/}; printf "  rede %s: %s Mb/s\n" "${n%/speed}" "$(cat "$s" 2>/dev/null)"; done'
    echo "----- compilador -----"
    mpicc --version | head -1
    echo "mpicc chama: $(mpicc --showme:command 2>/dev/null)"
    gcc --version | head -1
    echo "----- parametros -----"
    echo "WIDTH=$WIDTH HEIGHT=$HEIGHT MAX_ITER=$MAX_ITER CHUNK=$CHUNK REPS=$REPS PROCS=\"$PROCS\" CHUNKS=\"$CHUNKS\""
}

# Compila no proprio no de computo: -march=native precisa ser o do no que executa
# (o no de login tem outra CPU). O make mostra as linhas de compilacao com as flags.
build() {
    echo "----- compilacao em $(hostname) -----"
    make clean >/dev/null
    make X11=0 || exit 1
}
