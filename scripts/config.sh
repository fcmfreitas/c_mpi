# Parametros compartilhados pelos scripts sbatch do T2 (cluster Atlantica/LAD).
# Todos podem ser sobrescritos pelo ambiente, ex.: MAX_ITER=2000 sbatch scripts/t2_forte.sbatch
# Os nos (sempre os mesmos 2, exclusivos) estao fixos nas diretivas #SBATCH -w de cada script
# e em FIRST_NODE (onde ficam o mestre e a sequencial).

FIRST_NODE=${FIRST_NODE:-atlantica07}

# Entrada de referencia: imagem 4K (3840x2160, 2160 linhas = tarefas de 1 linha) com
# max_iter calibrado para T1 (sequencial) entre 30 e 60 s no no do cluster
# (results/t2_calibra.*.out: 1280x720 com max_iter=8000 leva 6,0 s; 4K tem 9x mais pixels).
WIDTH=${WIDTH:-3840}
HEIGHT=${HEIGHT:-2160}
MAX_ITER=${MAX_ITER:-6000}

# Granularidade escolhida (linhas por tarefa), justificada por results/t2_gran.*.out.
CHUNK=${CHUNK:-4}

# Granularidades comparadas no experimento de granularidade (P=32): de 1 linha
# (2160 tarefas) ate 512 linhas (5 tarefas, menos que os 31 escravos).
CHUNKS=${CHUNKS:-"1 2 4 8 16 32 64 128 256 512"}

# Repeticoes por configuracao; as tabelas reportam mediana, minimo e maximo.
REPS=${REPS:-10}

# Serie de processos (o mestre ocupa um nucleo e conta em P).
PROCS=${PROCS:-"2 4 8 16 32 64"}
