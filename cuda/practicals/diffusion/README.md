# diffusion instrumented to do in-situ visualization

### Compilation without in-situ

Minimal CMake configuration:
```shell
cmake -S . -B build
cmake --build build

### Compilation with Catalyst

uenv start prgenv-gnu/25.6:v1 --view=spack
export SPACK_SYSTEM_CONFIG_PATH="/user-environment/config"
spack load cray-mpich@8.1.32 cmake cuda@12.9.0/xo libcatalyst

cmake -S . -B buildCatalyst -DINSITU=Catalyst
cmake --build buildCatalyst

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:`spack location -i cuda@12.9.0/xo`/lib64
export CATALYST_IMPLEMENTATION_PATHS=/capstor/scratch/cscs/jfavre/ParaView/dev/lib64/catalyst
srun -t5 -pnormal --gpus-per-task=1 -n2 -N1 /users/jfavre/Projects/ParaView/select_local_device.sh buildCatalyst/bin/diffusion2d_mpi 8 50000 ../catalyst_pipeline.py
```

### Compilation with Ascent

uenv start prgenv-gnu/25.6:v1 --view=spack
export SPACK_SYSTEM_CONFIG_PATH="/user-environment/config"
spack load cray-mpich@8.1.32 cmake cuda@12.9.0/xo libcatalyst

cmake -S . -B buildAscent-cuda -DINSITU=Ascent
cmake --build buildAscent-cuda

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:`spack location -i cuda@12.9.0/xo`/lib64
srun -t5 -pnormal --gpus-per-task=1 -n2 -N1 /users/jfavre/Projects/ParaView/select_local_device.sh buildAscent-cuda/bin/diffusion2d_mpi 8 50000 ../catalyst_pipeline.py
```

### Local compilation on laptop "paradiso"
spack load libcatalyst
cmake -S . -B buildCatalyst -DCMAKE_CUDA_COMPILER=/usr/local/cuda-13.2/bin/nvcc -DINSITU=Catalyst
cmake --build buildCatalyst

