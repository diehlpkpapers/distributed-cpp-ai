#!/usr/bin/env bash
#SBATCH -o chatgpt4.out
#SBATCH -e chatgpt4.err
#SBATCH -t 25:59:00
#SBATCH -p medusa
#SBATCH -N 1
#SBATCH -c 24
#SBATCH --mail-user=pdiehl@cct.lsu.edu


 for type in coroutines async openmpi parallel_algorithm; do
     for ((i=24; i>=1; i--)); do
         
        ./$type -t $i 19200000000 1080000000 10000 -0.75 0.0 3.0 >> data_$type.dat
        done
    done 

sed 'n;d' data_coroutines.dat > outputfile
echo "threads,time" > data_coroutines.dat
awk '{print $8 "," $5}' outputfile >> data_coroutines.dat

rm outputfile
sed 'n;d' data_async.dat > outputfile
n=$(wc -l < outputfile)
echo $n
echo "threads,time" > data_async.dat
awk -v max="$n" '{print (max-NR+1) "," $5}' outputfile >> data_async.dat

rm outputfile
sed 'n;d' data_openmp.dat > outputfile
echo "threads,time" > data_openmp.dat
awk '{print $8 "," $5}' outputfile >> data_openmp.dat

