# Funcoes comuns dos scripts sbatch do T3. Uso: source scripts/config.sh; source scripts/lib.sh

# O shell do sbatch nao passa pelo /etc/profile do LAD: garante o OpenMPI 4.1.1 no PATH.
MPI_HOME=/LADAPPs/OpenMPI/openmpi-4.1.1
export PATH="$MPI_HOME/bin:$PATH"
export LD_LIBRARY_PATH="$MPI_HOME/lib:$MPI_HOME/lib/openmpi:${LD_LIBRARY_PATH:-}"

# delta = ceil(2n / (P + 1)): com P = 2^k - 1 a arvore original ocupa exatamente os P
# processos. Arredondado para cima para o pedaco maior (quando a divisao nao e exata)
# nao tentar dividir de novo para filhos que nao existem.
delta_for() {
    local P=$1
    echo $(( (2 * N + P) / (P + 1) ))
}

# Opcoes do srun para P processos dentro do job de 2 nos exclusivos. Cada no tem
# 8 nucleos fisicos e 16 threads de hardware (lscpu no cabecalho de cada saida):
#   P <= 8  : 1 no ($FIRST_NODE), 1 processo por nucleo fisico (--cpu-bind=cores)
#   P <= 16 : 2 nos, 1 processo por nucleo fisico
#   P <= 32 : 2 nos, 1 processo por thread de hardware (HT)
#   P >  32 : 2 nos, mais processos que threads (--overcommit) e sem amarrar
#             processo a nucleo (--cpu-bind=none): quem balanceia e o escalonador
#             do Linux, que precisa poder mover os processos entre os nucleos.
# Nos casos de 2 nos, os ranks alternam entre os nos ($DIST, ver config.sh), e a
# raiz (rank 0) fica no primeiro no, como a sequencial.
srun_opts() {
    local P=$1
    if [ "$P" -le 8 ]; then
        echo "-N 1 -w $FIRST_NODE -n $P --cpu-bind=cores"
    elif [ "$P" -le 16 ]; then
        echo "-N 2 -n $P --distribution=$DIST --cpu-bind=cores"
    elif [ "$P" -le 32 ]; then
        echo "-N 2 -n $P --distribution=$DIST --cpu-bind=threads"
    else
        echo "-N 2 -n $P --distribution=$DIST --cpu-bind=none --overcommit"
    fi
}

# Executa um comando mostrando a linha exata (fica na saida do Slurm, para reproducao).
run() {
    echo "+ $*"
    "$@"
}

# Uma execucao MPI: run_mpi P YIELD programa argumentos...
# YIELD vai explicito em toda execucao (errata do enunciado): 0 = espera ativa em
# MPI_Recv, 1 = o processo parado cede o nucleo. O programa imprime o valor na
# linha "result".
run_mpi() {
    local P=$1 yield=$2
    shift 2
    # shellcheck disable=SC2046
    run env OMPI_MCA_mpi_yield_when_idle="$yield" srun $(srun_opts "$P") "$@"
}

# Uma execucao sequencial, em um nucleo do primeiro no.
run_seq() {
    run srun -N 1 -w "$FIRST_NODE" -n 1 --cpu-bind=cores ./sort_seq "$@"
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
    echo "FIRST_NODE=$FIRST_NODE N=$N REPS=$REPS DIST=$DIST"
    echo "PROCS_ORIG=\"$PROCS_ORIG\" PROCS_BAL=\"$PROCS_BAL\" PROCS_OVER=\"$PROCS_OVER\" PROCS_ISEND=\"$PROCS_ISEND\""
}

# Compila no proprio no de computo: -march=native precisa ser o do no que executa
# (o no de login tem outra CPU). O make mostra as linhas de compilacao com as flags.
build() {
    echo "----- compilacao em $(hostname) -----"
    make clean >/dev/null
    make || exit 1
}
