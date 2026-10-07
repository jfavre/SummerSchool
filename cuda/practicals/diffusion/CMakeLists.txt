cmake_minimum_required(VERSION 3.20)
project (diffusion CUDA C CXX)

include (GNUInstallDirs)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(../include)

add_executable(diffusion2d "")
target_sources(diffusion2d
  PRIVATE
    diffusion2d.cu
)
set_target_properties(diffusion2d PROPERTIES CUDA_ARCHITECTURES "native")

find_package(MPI COMPONENTS C CXX)
if (NOT MPI_FOUND)
  message(STATUS
    "Skipping example: ${CMAKE_PROJECT_NAME} requires MPI.")
  return ()
endif ()


add_executable(diffusion2d_mpi "")
target_sources(diffusion2d_mpi
  PRIVATE
    diffusion2d_mpi.cu
)
set_target_properties(diffusion2d_mpi PROPERTIES CUDA_ARCHITECTURES "native")

set(INSITU None CACHE STRING "Enable in-situ support")
set_property(CACHE INSITU PROPERTY STRINGS None Catalyst Ascent VTK-m Viskores)

if(INSITU STREQUAL "None")
  target_link_libraries(diffusion2d PRIVATE cublas cudart)
  target_link_libraries(diffusion2d_mpi MPI::MPI_CXX cublas cudart)
elseif(INSITU STREQUAL "Catalyst")
  find_package(catalyst REQUIRED PATHS "${EBROOTCATALYST}/lib/cmake/catalyst-2.0")
  target_compile_definitions(diffusion2d PUBLIC USE_CATALYST=1)
  target_link_libraries(diffusion2d PRIVATE cublas cudart catalyst::catalyst)
  target_compile_definitions(diffusion2d_mpi PUBLIC USE_CATALYST=1)
  target_link_libraries(diffusion2d_mpi MPI::MPI_CXX cublas cudart catalyst::catalyst)
elseif(INSITU STREQUAL "Ascent")
  find_package(Ascent REQUIRED PATHS "/local/apps/Ascent-cuda/install/ascent-checkout/lib/cmake/ascent")
  target_compile_definitions(diffusion2d PRIVATE USE_ASCENT=1)
  target_link_libraries(diffusion2d PRIVATE cublas ascent::ascent)

  target_compile_definitions(diffusion2d_mpi PRIVATE USE_ASCENT=1)
  target_link_libraries(diffusion2d_mpi PRIVATE cublas MPI::MPI_CXX ascent::ascent_mpi)
endif()
