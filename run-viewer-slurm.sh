#!/bin/bash
# A sample script of running the viewer workers through a Slurm
# job submission system

cd $WORK_DIR

PORT=6910
VARIABLE=O2

mpirun -np $SLURM_JOB_NUM_NODES -ppn 1 ./pidx_render_worker \
	-port $PORT \
   	-variable $VARIABLE \
	-timesteps ~/data/pidx_uintah

