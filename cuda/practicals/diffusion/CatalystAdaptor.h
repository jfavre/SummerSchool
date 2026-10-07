#ifndef CatalystAdaptor_h
#define CatalystAdaptor_h

#include <catalyst.hpp>
#include <catalyst_conduit_blueprint.hpp>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
  
namespace CatalystAdaptor
{
  conduit_cpp::Node catalyst_mesh;
  conduit_cpp::Node exec_params;

void InitializeCatalyst(const char* PythonFilename)
{
  std::cout << "CatalystInitialize" << std::endl;

  conduit_cpp::Node node;
  // the Catalyst Python filename should be the only argument at this time

  const auto path = std::string(PythonFilename);
  node["catalyst/scripts/script0/filename"].set_string(path);

  // indicate that we want to load ParaView-Catalyst
  node["catalyst_load/implementation"].set_string("paraview");
  // the env variable CATALYST_IMPLEMENTATION_PATHS should indicate where to find the ParaView specific implementation
  catalyst_status err = catalyst_initialize(conduit_cpp::c_node(&node));
  if (err != catalyst_status_ok)
  {
    std::cerr << "ERROR: Failed to initialize Catalyst: " << err << std::endl;
  }
}

void CreateConduitNode(const double *temperature_data,
             const int nx,
             const int ny,
             const int offset=0)
{
  auto channel = exec_params["catalyst/channels/grid"];
  channel["type"].set("mesh");
  channel["data"] = catalyst_mesh;
  auto mesh = channel["data"];

  mesh["coordsets/coords/dims/i"].set(nx);
  mesh["coordsets/coords/dims/j"].set(ny);
  // do not specify the 3rd dimension with a dim of 1, a z_origin, and a z_spacing
  
  float spacing = 1.0/(nx+1.0);

  mesh["coordsets/coords/origin/x"].set(0.0);
  mesh["coordsets/coords/origin/y"].set(offset * spacing * (ny-1));
  //std::cout << "Oy = "<< offset * spacing * (ny-1) << "\n";
  mesh["coordsets/coords/type"].set("uniform");

  mesh["coordsets/coords/spacing/dx"].set(spacing);
  mesh["coordsets/coords/spacing/dy"].set(spacing);
  mesh["coordsets/coords/type"].set("uniform");

  mesh["topologies/mesh/type"].set("uniform");
  mesh["topologies/mesh/coordset"].set("coords");

  // Finally, add fields.
  auto fields = mesh["fields"];
  // temperature is vertex-data.
  fields["temperature/association"].set("vertex");
  fields["temperature/type"].set("scalar");
  fields["temperature/topology"].set("mesh");
  fields["temperature/volume_dependent"].set("false");
  fields["temperature/values"].set_external(temperature_data, nx*ny);
  // // temperature_data is on the host
  //mesh.print();
  conduit_cpp::Node verify_info;
  if(!conduit_cpp::Blueprint::verify("mesh", mesh, verify_info))
    std::cerr << "Diffusion mesh verify failed!" << std::endl;
  else
    std::cerr << "Diffusion mesh verify passed!" << std::endl;
}

void Execute(int timestep, double dt)
{
  auto state = exec_params["catalyst/state"];
  state["timestep"].set(timestep);
  state["time"].set(timestep * dt);

  catalyst_status err = catalyst_execute(conduit_cpp::c_node(&exec_params));
  if (err != catalyst_status_ok)
  {
    std::cerr << "ERROR: Failed to execute Catalyst: " << err << std::endl;
  }
}

void Finalize()
{
  conduit_cpp::Node node;
  catalyst_status err = catalyst_finalize(conduit_cpp::c_node(&node));
  if (err != catalyst_status_ok)
  {
    std::cerr << "ERROR: Failed to finalize Catalyst: " << err << std::endl;
  }
}
}
#endif

