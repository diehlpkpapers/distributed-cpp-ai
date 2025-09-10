#!/usr/bin/env bash
#SBATCH -o claude.out
#SBATCH -e claude.err
#SBATCH -t 25:59:00
#SBATCH -p medusa
#SBATCH -N 1
#SBATCH -c 24
#SBATCH --mail-user=pdiehl@cct.lsu.edu

module load tbb

 for type in async openmp parallel_algorithm coroutines_fixed; do
     for ((i=24; i>=1; i--)); do
        ./$type -t $i  >> data_$type.dat
        done
    done 


