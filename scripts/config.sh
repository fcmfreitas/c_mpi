# Parametros compartilhados pelos scripts sbatch do T2 (cluster Atlantica/LAD).
# Todos podem ser sobrescritos pelo ambiente, ex.: MAX_ITER=2000 sbatch scripts/t2_forte.sbatch
# Os nos (sempre os mesmos 2, exclusivos) estao fixos nas diretivas #SBATCH -w de cada script.

# Entrada de referencia: 1280x720 e max_iter calibrado para T1 (sequencial) entre
# 30 e 60 s no no do cluster (results/t2_calibra.*.out).
WIDTH=${WIDTH:-1280}
HEIGHT=${HEIGHT:-720}
MAX_ITER=${MAX_ITER:-4000}

# Granularidade escolhida (linhas por tarefa), justificada por results/t2_gran.*.out.
CHUNK=${CHUNK:-1}

# Granularidades comparadas no experimento de granularidade (P=32).
CHUNKS=${CHUNKS:-"1 2 4 8 16 32 64 128"}

# Repeticoes por configuracao; as tabelas reportam mediana, minimo e maximo.
REPS=${REPS:-10}

# Serie de processos (o mestre ocupa um nucleo e conta em P).
PROCS=${PROCS:-"2 4 8 16 32 64"}
