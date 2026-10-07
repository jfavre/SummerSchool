#ifndef AscentAdaptor_h
#define AscentAdaptor_h

#include <ascent/ascent.hpp>
#include "conduit_blueprint.hpp"

namespace AscentAdaptor
{
  ascent::Ascent ascent;
  conduit::Node mesh;
  conduit::Node actions;

void Initialize(double *temperature_data, const int nx, const int ny, const int offset=0)
{ 
  std::cout << "AscentInitialize.........................................\n";

  conduit::Node ascent_options;
  std::string output_path = "datasets";
  std::string varname = "Temperature";
  if(!conduit::utils::is_directory(output_path))
  {
    ASCENT_INFO("Creating output folder: " + output_path);
    conduit::utils::create_directory(output_path);
  }
  ascent_options["default_dir"] = output_path;
#ifdef _MPICC_H
  ascent_options["mpi_comm"] = MPI_Comm_c2f(MPI_COMM_WORLD);
#endif
  ascent.open(ascent_options);
#if defined (ASCENT_CUDA_ENABLED)
  std::cout << "\n[using ascent cuda support]" << std::endl;
  ascent_options["runtime/vtkm/backend"] = "cuda";
#endif
  AscentAdaptor::ascent.open(ascent_options);

  mesh["coordsets/coords/dims/i"].set(nx);
  mesh["coordsets/coords/dims/j"].set(ny);
  //mesh["coordsets/coords/dims/k"].set(1);
  // do not specify the 3rd dimension with a dim of 1, a z_origin, and a z_spacing

  float spacing = 1.0/(nx+1.0);
 
  mesh["coordsets/coords/origin/x"].set(0.0);
  mesh["coordsets/coords/origin/y"].set(offset * spacing * (ny-1));
  //mesh["coordsets/coords/origin/z"].set(0.0);
  //std::cout << "Oy = "<< offset * spacing * (ny-1) << "\n";
  mesh["coordsets/coords/type"].set("uniform");

  mesh["coordsets/coords/spacing/dx"].set(spacing);
  mesh["coordsets/coords/spacing/dy"].set(spacing);
  //mesh["coordsets/coords/spacing/dz"].set(spacing);
  // add topology.
  mesh["topologies/mesh/type"].set("uniform");
  mesh["topologies/mesh/coordset"].set("coords");
  
  // Temperature is vertex-data
  mesh["fields/Temperature/association"].set("vertex");
  mesh["fields/Temperature/type"].set("scalar");
  mesh["fields/Temperature/topology"].set("mesh");
  mesh["fields/Temperature/volume_dependent"].set("false");
  mesh["fields/Temperature/values"].set_external(temperature_data, nx * ny);
  // Temperature_data is either on the host or on the device

  conduit::Node verify_info;
  if (!conduit::blueprint::mesh::verify(mesh, verify_info))
    {
    // verify failed, print error message
    CONDUIT_INFO("blueprint verify failed!" + verify_info.to_json());
    }
  //else CONDUIT_INFO("blueprint verify success!");

  conduit::Node pipelines;
  pipelines["isolines/f1/type"] = "contour";
  conduit::Node &params1 = pipelines["isolines/f1/params"];
  params1["field"] = varname;
  params1["iso_values"].set({0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9});

  conduit::Node &add_pipelines = actions.append();
  add_pipelines["action"] = "add_pipelines";
  add_pipelines["pipelines"] = pipelines;
  
  conduit::Node &add_action = actions.append();
  add_action["action"] = "add_scenes";
  conduit::Node &scenes       = add_action["scenes"];
  scenes["s1/plots/p1/type"]  = "pseudocolor";
  scenes["s1/plots/p1/field"] = varname;
  scenes["s1/plots/p1/color_table/name"] = "viridis";
  // adding the iso-lines
  scenes["s1/plots/p2/type"]  = "mesh";
  scenes["s1/plots/p2/pipeline"] = "isolines";
  scenes["s1/image_prefix"] = varname + "-Ascent.%05d";
  actions.save("my_ascent_actions.yaml");
}

void Execute(int timestep, double dt)
{
  mesh["state/cycle"].set(timestep);
  mesh["state/time"].set(timestep * dt);
  //mesh.print(); // don't use with CUDA. Causes a crash
  AscentAdaptor::ascent.publish(mesh);
  AscentAdaptor::ascent.execute(actions);
}

void Finalize()
{
#ifndef ASCENT_CUDA_ENABLED
  conduit::Node action;
  conduit::Node &add_action = action.append();
  
  add_action["action"] = "add_extracts";
  conduit::Node &extract = add_action["extracts"];
  extract["e1/type"] = "relay";
  extract["e1/params/path"] = "temperature_mesh";
  extract["e1/params/protocol"] = "hdf5";
  extract["e1/params/topologies"].append() = "mesh";
  ascent.publish(mesh);
  ascent.execute(action);
#endif
  ascent.close();
  std::cout << "AscentFinalize.........................................\n";
}
}
#endif
