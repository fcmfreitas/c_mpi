# Parametros compartilhados pelos scripts sbatch do T3 (cluster Atlantica/LAD).
# Todos podem ser sobrescritos pelo ambiente, ex.: N=120000 sbatch scripts/t3_orig.sbatch
# Os nos (sempre os mesmos 2, exclusivos) estao fixos nas diretivas #SBATCH -w de cada
# script e em FIRST_NODE (onde ficam a raiz e a sequencial).

FIRST_NODE=${FIRST_NODE:-atlantica07}

# Tamanho do vetor: escolhido com scripts/t3_calibra.sbatch para que T1 (Bubble Sort
# do vetor inteiro, em ordem decrescente) fique entre 30 e 60 s no no do cluster.
# PROVISORIO ate a calibracao: troque pelo valor medido.
N=${N:-150000}

# Tamanhos testados na calibracao (uma execucao de T1 cada).
CALIB_N=${CALIB_N:-"100000 125000 150000 175000 200000"}

# Repeticoes por configuracao; as tabelas reportam mediana, minimo e maximo.
REPS=${REPS:-10}

# Series do enunciado (secao 6).
PROCS_ORIG=${PROCS_ORIG:-"1 3 7 15 31"}            # versao original, ate o HT
PROCS_BAL=${PROCS_BAL:-"2 4 8 16 31 32"}           # versao balanceada (alterando o codigo)
PROCS_OVER=${PROCS_OVER:-"63 127 255 511 1023"}    # original com mais processos que threads
PROCS_ISEND=${PROCS_ISEND:-"16 32"}                # balanceada com MPI_Isend (Radenski), 2 nos

# Distribuicao dos ranks entre os 2 nos (srun --distribution). cyclic alterna os
# ranks entre os nos (0 no primeiro, 1 no segundo, 2 no primeiro, ...). Na arvore
# original as folhas sao os ultimos (P+1)/2 ranks; com block (ranks 0..P/2 no
# primeiro no) quase todas as folhas cairiam no segundo no, e o primeiro ficaria so
# com processos parados. Com cyclic cada no fica com metade das folhas.
DIST=${DIST:-cyclic}
