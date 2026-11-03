#!/bin/bash
# Granularidade e balanceamento com P = 32:
#  (a) --pingpong: ida e volta das mensagens de uma tarefa (sem calculo) com cada
#      escravo, para cada granularidade; os escravos no no do mestre medem a
#      comunicacao intra-no e os do outro no, a inter-no. O tempo de processar
#      uma tarefa vem do per_task de --rank-stats.
#  (b) tempo total para cada granularidade (a escolhida, menores e maiores), 10x.
#  (c) --rank-stats: tarefas, iteracoes, tempo ocupado e instante de liberacao
#      de cada escravo, em todas as execucoes.
#SBATCH --job-name=t2_gran
#SBATCH --nodes=2
#SBATCH --nodelist=atlantica07,atlantica08
#SBATCH --exclusive
#SBATCH --time=01:30:00
#SBATCH --output=results/%x.%j.out
#SBATCH --no-requeue

cd "$SLURM_SUBMIT_DIR" || exit 1
source scripts/config.sh
source scripts/lib.sh
job_header
build

echo "----- (a) ida e volta das mensagens de uma tarefa, P=32, por granularidade e por escravo -----"
for chunk in $CHUNKS; do
    # Menos idas e voltas para mensagens grandes (512 linhas = 7,9 MB), para o passo nao demorar.
    n=$((2000 / chunk)); [ "$n" -lt 50 ] && n=50
    mark exp=pingpong P=32 chunk="$chunk" height="$HEIGHT" rep=1
    run_mpi 32 --height "$HEIGHT" --chunk "$chunk" --pingpong "$n"
done

echo "----- (b)(c) tempo total e estatisticas por escravo, P=32, $REPS repeticoes por granularidade -----"
for chunk in $CHUNKS; do
    for rep in $(seq "$REPS"); do
        mark exp=gran P=32 chunk="$chunk" height="$HEIGHT" rep="$rep"
        run_mpi 32 --height "$HEIGHT" --chunk "$chunk" --rank-stats
    done
done

echo "===== fim em $(date) ====="
