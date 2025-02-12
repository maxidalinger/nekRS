#include "nrs.hpp"
#include "platform.hpp"
#include "linAlg.hpp"

namespace
{
int firstTime = 1;
occa::memory h_scratch;
}

void setup(nrs_t *nrs)
{
  mesh_t *mesh = nrs->meshV;
  h_scratch = platform->device.mallocHost<dfloat>(mesh->Nelements);

  if (nrs->elementType == QUADRILATERALS || nrs->elementType == HEXAHEDRA) {
    auto dH = (dfloat *) calloc((mesh->N + 1), sizeof(dfloat));

    for (int n = 0; n < (mesh->N + 1); n++) {
      if (n == 0)
        dH[n] = mesh->gllz[n + 1] - mesh->gllz[n];
      else if (n == mesh->N)
        dH[n] = mesh->gllz[n] - mesh->gllz[n - 1];
      else
        dH[n] = 0.5 * (mesh->gllz[n + 1] - mesh->gllz[n - 1]);
    }
    for (int n = 0; n < (mesh->N + 1); n++)
      dH[n] = 1.0 / dH[n];

    nrs->o_idH = platform->device.malloc<dfloat>((mesh->N + 1), dH);
    free(dH);
  }
  firstTime = 0;
}

dfloat computeCFL(nrs_t *nrs)
{
  mesh_t *mesh = nrs->meshV;

  if (firstTime)
    setup(nrs);

  auto o_cfl = platform->o_memPool.reserve<dfloat>(mesh->Nelements);

  nrs->cflKernel(mesh->Nelements,
                 nrs->dt[0],
                 mesh->o_vgeo,
                 nrs->o_idH,
                 nrs->fieldOffset,
                 nrs->o_U,
                 mesh->o_U,
                 o_cfl);

  auto scratch = (dfloat *) h_scratch.ptr();
  o_cfl.copyTo(scratch);

  dfloat cfl = 0;
  for (dlong n = 0; n < mesh->Nelements; ++n) {
    cfl = std::max(cfl, scratch[n]);
  }

  dfloat gcfl = 0;
  MPI_Allreduce(&cfl, &gcfl, 1, MPI_DFLOAT, MPI_MAX, platform->comm.mpiComm);

  return gcfl;
}
