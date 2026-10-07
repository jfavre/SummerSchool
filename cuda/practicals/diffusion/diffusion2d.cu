#include <iostream>
#include <fstream>
#include <cstdio>

#include <cuda.h>

#include "util.hpp"
#include "cuda_stream.hpp"

// 2D diffusion example
// the grid has a fixed width of nx=128
// the use specifies the height, ny, as a power of two
// note that nx and ny have 2 added to them to account for halos

template <typename T>
void fill_gpu(T *v, T value, int n);

void write_to_file(int nx, int ny, double* data);

#ifdef USE_ASCENT
#include "AscentAdaptor.h"
#endif

#ifdef USE_CATALYST
#include "CatalystAdaptor.h"
#endif

#ifdef USE_VTKM
#include "VTKmAdaptor.h"
#endif

#ifdef USE_VISKORES
#include "ViskoresAdaptor.h"
#endif

__global__
void diffusion(const double *x0, double *x1, int nx, int ny, double dt) {
    int i = threadIdx.x + blockDim.x*blockIdx.x + 1;
    int j = threadIdx.y + blockDim.y*blockIdx.y + 1;

    if (i<nx-1 && j<ny-1) {
        int pos = nx*j + i;
          x1[pos] = x0[pos] + dt * (-4.*x0[pos]
                     + x0[pos-1] + x0[pos+1]
                     + x0[pos-nx] + x0[pos+nx]);

    }
}

int main(int argc, char** argv) {
    // set up parameters
    // first argument is the y dimension = 2^arg
    size_t pow    = read_arg(argc, argv, 1, 8);
    // second argument is the number of time steps
    size_t nsteps = read_arg(argc, argv, 2, 5001);

    // set domain size
    size_t ny = (1 << pow)+2;
    size_t nx = ny;
    double dt = 0.1;

    std::cout << "\n## " << nx << "x" << ny
              << " for " << nsteps << " time steps"
              << " (" << nx*ny << " grid points)"
              << std::endl;

    // allocate memory on device and host
    // note : allocate enough memory for the halo around the boundary
    auto buffer_size = nx*ny;
    double *x_host = malloc_host<double>(buffer_size);
    double *x0     = malloc_device<double>(buffer_size);
    double *x1     = malloc_device<double>(buffer_size);

    // set initial conditions of 0 everywhere
    fill_gpu(x0, 0., buffer_size);
    //fill_gpu(x1, 0., buffer_size); // no need to initialize x1. Will be set at the first timestep

    // set boundary conditions of 1 on south border
    fill_gpu(x0, 1., nx);
    fill_gpu(x1, 1., nx);
    fill_gpu(x0+nx*(ny-1), 1., nx);
    fill_gpu(x1+nx*(ny-1), 1., nx);

    cuda_stream stream;
    cuda_stream copy_stream{};
    auto start_event = stream.enqueue_event();

    // grid and block config
    auto find_num_blocks = [](int x, int bdim) {return (x+bdim-1)/bdim;};
    dim3 block_dim(16, 16);
    int nbx = find_num_blocks(nx-2, block_dim.x);
    int nby = find_num_blocks(ny-2, block_dim.y);
    dim3 grid_dim(nbx, nby);
    
#ifdef USE_ASCENT
#ifdef ASCENT_CUDA_ENABLED
    AscentAdaptor::Initialize(x1, nx, ny);
#else
    AscentAdaptor::Initialize(x_host, nx, ny);
#endif
#endif

#ifdef USE_VTKM
    vtkm::cont::Initialize(argc, argv);
    VTKmAdaptor::Initialize(nx, ny);
#endif

#ifdef USE_VISKORES
    viskores::cont::Initialize(argc, argv);
    ViskoresAdaptor::Initialize(nx, ny);
#endif

#ifdef USE_CATALYST
    if(argc == 4)
      {
      CatalystAdaptor::InitializeCatalyst(argv[3]);
      CatalystAdaptor::CreateConduitNode(x1, nx, ny);
      }
    else
      {
      std::cerr << "missing a Python Catalyst script as 3rd argument\n";
      exit(-1);
      }
#endif

    // time stepping loop
    for(auto step=0; step<nsteps; ++step) {
        diffusion<<<grid_dim, block_dim>>>(x0, x1, nx, ny, dt);

        if(!(step % 2000))
          {
#ifdef USE_ASCENT
#ifndef ASCENT_CUDA_ENABLED
          copy_to_host<double>(x1, x_host, buffer_size); // use x1 with most recent result
#endif
          AscentAdaptor::Execute(step, dt);
#endif

#ifdef USE_CATALYST
          CatalystAdaptor::Execute(step, dt);
#endif

#ifdef USE_VTKM
        VTKmAdaptor::Execute(step, x1, nx*ny); //execute every 1000 steps;
#endif
#ifdef USE_VISKORES
        ViskoresAdaptor::Execute(step, x1, nx*ny); //execute every 1000 steps;
#endif
          }
        std::swap(x0, x1);
    }
    auto stop_event = stream.enqueue_event();
    stop_event.wait();

    copy_to_host<double>(x0, x_host, buffer_size);
    cudaFree(x0);
    cudaFree(x1);
    
    double time = stop_event.time_since(start_event);
#ifdef USE_ASCENT
    AscentAdaptor::Finalize();
#endif
#ifdef USE_CATALYST
    CatalystAdaptor::Finalize();
#endif
#ifdef USE_VTKM
    VTKmAdaptor::Finalize();
#endif
#ifdef USE_VISKORES
    ViskoresAdaptor::Finalize();
#endif
    std::cout << "## " << time << "s, "
              << nsteps*(nx-2)*(ny-2) / time << " points/second"
              << std::endl << std::endl;

    std::cout << "writing to output.bin/bov" << std::endl;
    write_to_file(nx, ny, x_host);

    return 0;
}

template <typename T>
__global__
void fill(T *v, T value, int n) {
    int tid  = threadIdx.x + blockDim.x*blockIdx.x;

    if(tid<n) {
        v[tid] = value;
    }
}

template <typename T>
void fill_gpu(T *v, T value, int n) {
    auto block_dim = 192ul;
    auto grid_dim = n/block_dim + (n%block_dim ? 1 : 0);

    fill<T><<<grid_dim, block_dim>>>(v, value, n);
}

void write_to_file(int nx, int ny, double* data) {
    {
        FILE* output = fopen("output.bin", "wb");
        fwrite(data, sizeof(double), static_cast<size_t>(nx) * ny, output);
        fclose(output);
    }

    std::ofstream fid("output.bov");
    fid << "TIME: 0.0" << std::endl;
    fid << "DATA_FILE: output.bin" << std::endl;
    fid << "DATA_SIZE: " << nx << " " << ny << " 1" << std::endl;;
    fid << "DATA_FORMAT: DOUBLE" << std::endl;
    fid << "VARIABLE: phi" << std::endl;
    fid << "DATA_ENDIAN: LITTLE" << std::endl;
    fid << "CENTERING: nodal" << std::endl;
    fid << "BRICK_SIZE: 1.0 1.0 1.0" << std::endl;
}
