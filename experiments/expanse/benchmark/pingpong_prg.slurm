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

fabric=ucx
path_to_exe=${1:-init/build/benchmark}
allComputeTime=(0 10 100)

for compute_time in ${allComputeTime[@]}; do

export OMP_PLACES=cores
echo "srun -n 2 prg_${fabric}_compute(1 prg-${compute_time}us-prefill)"
srun --nodes=2 --ntasks-per-node=1 --ntasks=2 --mpi=pmi2 ${path_to_exe}/pingpong_prg 15 1 8 8 '15' ${compute_time} 8
srun --nodes=2 --ntasks-per-node=1 --ntasks=2 --mpi=pmi2 ${path_to_exe}/pingpong_prg 31 1 8 8 '31' ${compute_time} 8
srun --nodes=2 --ntasks-per-node=1 --ntasks=2 --mpi=pmi2 ${path_to_exe}/pingpong_prg 63 1 8 8 '63' ${compute_time} 8

export OMP_PROC_BIND=close;
export OMP_PLACES=cores
echo "srun -n 2 prg_${fabric}_compute(2 prg-${compute_time}us-prefill)"
srun --nodes=2 --ntasks-per-node=1 --ntasks=2 --mpi=pmi2 ${path_to_exe}/pingpong_prg 14 2 8 8 '14,15' ${compute_time} 8
export OMP_PLACES='{0},{16},{1},{17},{2},{18},{3},{19},{4},{20},{5},{21},{6},{22},{7},{23},{8},{24},{9},{25},{10},{26},{11},{27},{12},{28},{13},{29},{14},{30}'
srun --nodes=2 --ntasks-per-node=1 --ntasks=2 --mpi=pmi2 ${path_to_exe}/pingpong_prg 30 2 8 8 '15,31' ${compute_time} 8
export OMP_PLACES='{0},{16},{32},{48},{1},{17},{33},{49},{2},{18},{34},{50},{3},{19},{35},{51},{4},{20},{36},{52},{5},{21},{37},{53},{6},{22},{38},{54},{7},{23},{39},{55},{8},{24},{40},{56},{9},{25},{41},{57},{10},{26},{42},{58},{11},{27},{43},{59},{12},{28},{44},{60},{13},{29},{45},{61},{14},{30},{46},{62},{15},{31}'
srun --nodes=2 --ntasks-per-node=1 --ntasks=2 --mpi=pmi2 ${path_to_exe}/pingpong_prg 62 2 8 8 '47,63' ${compute_time} 8

echo "srun -n 2 prg_${fabric}_compute(4 prg-${compute_time}us-prefill)"
export OMP_PLACES='{0},{16},{1},{17},{2},{18},{3},{19},{4},{20},{5},{21},{6},{22},{7},{23},{8},{24},{9},{25},{10},{26},{11},{27},{12},{28},{13},{29}'
srun --nodes=2 --ntasks-per-node=1 --ntasks=2 --mpi=pmi2 ${path_to_exe}/pingpong_prg 28 4 8 8 '14,30,15,31' ${compute_time} 8
export OMP_PLACES='{0},{16},{32},{48},{1},{17},{33},{49},{2},{18},{34},{50},{3},{19},{35},{51},{4},{20},{36},{52},{5},{21},{37},{53},{6},{22},{38},{54},{7},{23},{39},{55},{8},{24},{40},{56},{9},{25},{41},{57},{10},{26},{42},{58},{11},{27},{43},{59},{12},{28},{44},{60},{13},{29},{45},{61},{14},{30},{46},{62}'
srun --nodes=2 --ntasks-per-node=1 --ntasks=2 --mpi=pmi2 ${path_to_exe}/pingpong_prg 60 4 8 8 '15,31,47,63' ${compute_time} 8

echo "srun -n 2 prg_${fabric}_compute(8 prg-${compute_time}us-prefill)"
export OMP_PLACES='{0},{16},{32},{48},{1},{17},{33},{49},{2},{18},{34},{50},{3},{19},{35},{51},{4},{20},{36},{52},{5},{21},{37},{53},{6},{22},{38},{54},{7},{23},{39},{55},{8},{24},{40},{56},{9},{25},{41},{57},{10},{26},{42},{58},{11},{27},{43},{59},{12},{28},{44},{60},{13},{29},{45},{61}'
srun --nodes=2 --ntasks-per-node=1 --ntasks=2 --mpi=pmi2 ${path_to_exe}/pingpong_prg 56 8 8 8 '14,30,46,62,15,31,47,63' ${compute_time} 8

done # end of experiments for a compute time
