#!/usr/bin/env bash
#SBATCH -o chatgpt4.out
#SBATCH -e chatgpt4.err
#SBATCH -t 25:59:00
#SBATCH -p medusa
#SBATCH -N 1
#SBATCH -c 24
#SBATCH --mail-user=pdiehl@cct.lsu.edu


 for type in coroutines async openmp parallel_algorithm; do
     for ((i=24; i>=1; i--)); do
        ./$type -t $i 192000 10800 1000 -0.75 0.0 3.0 >> data_$type.dat
        done
    done 


