#!/bin/bash
#SBATCH --partition=compute
#SBATCH --account=uic193
#SBATCH --time=15:00
#SBATCH --nodes=2
#SBATCH --ntasks-per-node=64
#SBATCH --exclusive
#SBATCH --job-name=basic
#SBATCH --output=slurm_output_%j.txt
#SBATCH --error=slurm_error_%j.txt

fabric=ofi
path_to_exe=${1:-init/build/benchmark}

export OMP_PLACES=cores

echo "srun -n 2 pingpong_sym(${fabric}-multi-thread-8b-all-threads)"
srun --nodes=2 --ntasks-per-node=1 --ntasks=2 --mpi=pmi2 ${path_to_exe}/pingpong_sym 1 8 8
srun --nodes=2 --ntasks-per-node=1 --ntasks=2 --mpi=pmi2 ${path_to_exe}/pingpong_sym 2 8 8
srun --nodes=2 --ntasks-per-node=1 --ntasks=2 --mpi=pmi2 ${path_to_exe}/pingpong_sym 4 8 8
srun --nodes=2 --ntasks-per-node=1 --ntasks=2 --mpi=pmi2 ${path_to_exe}/pingpong_sym 8 8 8
srun --nodes=2 --ntasks-per-node=1 --ntasks=2 --mpi=pmi2 ${path_to_exe}/pingpong_sym 16 8 8
srun --nodes=2 --ntasks-per-node=1 --ntasks=2 --mpi=pmi2 ${path_to_exe}/pingpong_sym 32 8 8
srun --nodes=2 --ntasks-per-node=1 --ntasks=2 --mpi=pmi2 ${path_to_exe}/pingpong_sym 64 8 8

echo "srun -n 2 pingpong_sym(${fabric}-multi-process-8b-all-threads)"
srun --nodes=2 --ntasks-per-node=1 --ntasks=2 --cpu-bind=core --mpi=pmi2 ${path_to_exe}/pingpong_sym 1 8 8
srun --nodes=2 --ntasks-per-node=2 --ntasks=4 --cpu-bind=core --mpi=pmi2 ${path_to_exe}/pingpong_sym 1 8 8
srun --nodes=2 --ntasks-per-node=4 --ntasks=8 --cpu-bind=core --mpi=pmi2 ${path_to_exe}/pingpong_sym 1 8 8
srun --nodes=2 --ntasks-per-node=8 --ntasks=16 --cpu-bind=core --mpi=pmi2 ${path_to_exe}/pingpong_sym 1 8 8
srun --nodes=2 --ntasks-per-node=16 --ntasks=32 --cpu-bind=core --mpi=pmi2 ${path_to_exe}/pingpong_sym 1 8 8
srun --nodes=2 --ntasks-per-node=32 --ntasks=64 --cpu-bind=core --mpi=pmi2 ${path_to_exe}/pingpong_sym 1 8 8
srun --nodes=2 --ntasks-per-node=64 --ntasks=128 --cpu-bind=core --mpi=pmi2 ${path_to_exe}/pingpong_sym 1 8 8

echo "srun -n 2 pingpong_sym(${fabric}-multi-threaded-8b-64k-64-threads)"
srun --nodes=2 --ntasks-per-node=1 --ntasks=2 --mpi=pmi2 ${path_to_exe}/pingpong_sym 64 8 65536

echo "srun -n 2 pingpong_sym(${fabric}-multi-process-8b-64k-64-threads)"
srun --nodes=2 --ntasks-per-node=64 --ntasks=128 --cpu-bind=core --mpi=pmi2 ${path_to_exe}/pingpong_sym 1 8 65536