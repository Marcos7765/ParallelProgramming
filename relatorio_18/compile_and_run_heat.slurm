#!/bin/bash
#SBATCH --partition=gpu-4-a100
#SBATCH --ntasks=1
#SBATCH --gpus-per-task=1
#SBATCH --time=30
#SBATCH --qos=preempt

module load compilers/gnu
module load compilers/nvidia/cuda
module load libraries/cuda
module load compilers/nvidia/nvhpc

cd ~/ProgParalela

nvc main.c -fast -mp=gpu -gpu=cc80 -lm -o main.out
nvc main_control.c -fast -mp=gpu -gpu=cc80 -lm -o controle.out
nvc main_controle_otimizado.c -fast -mp=gpu -gpu=cc80 -lm -o controle_otimizado.out

heat_params="8000 1000"
echo $SLURM_JOB_NODELIST
echo $(hostname)

echo "-------------------------------------- COM GPU"

./main.out $heat_params

echo "-------------------------------------- CONTROLE"
./controle.out $heat_params

echo "-------------------------------------- CONTROLE SOLUCAO OTIMIZADA"
./controle_otimizado.out $heat_params
