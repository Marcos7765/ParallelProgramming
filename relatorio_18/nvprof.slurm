#!/bin/bash
#SBATCH --partition=gpu-4-a100
#SBATCH --nodes=1
#SBATCH --ntasks-per-node=1
#SBATCH --cpus-per-task=1
#SBATCH --gpus-per-task=1
#SBATCH --time=2
#SBATCH --qos=preempt

module load compilers/gnu
module load compilers/nvidia/cuda
module load libraries/cuda
module load compilers/nvidia/nvhpc
# module load containers/nvhpc

cd ~/ProgParalela

heat_params="1000 1000"
echo $SLURM_JOB_NODELIST
echo $(hostname)

echo "-------------------------------------- COM GPU"

nsys nvprof ./main.out $heat_params

echo "-------------------------------------- CONTROLE"
nsys nvprof ./controle.out $heat_params

echo "-------------------------------------- CONTROLE SOLUCAO OTIMIZADA"
nsys nvprof ./controle_otimizado.out $heat_params
