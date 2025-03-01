cds_t *cdsSetup(nrs_t *nrs, setupAide options)
{
  const std::string section = "cds-";
  cds_t *cds = new cds_t();
  platform_t *platform = platform_t::getInstance();
  device_t &device = platform->device;

  // initialize vector entries
  cds->mesh.resize(nrs->Nscalar);
  cds->fieldOffset.resize(nrs->Nscalar);
  cds->fieldOffsetScan.resize(nrs->Nscalar);
  cds->solver.resize(nrs->Nscalar);
  cds->compute.resize(nrs->Nscalar);
  cds->cvodeSolve.resize(nrs->Nscalar);
  cds->filterS.resize(nrs->Nscalar);

  cds->mesh[0] = nrs->_mesh;
  mesh_t *mesh = cds->mesh[0];
  cds->meshV = nrs->_mesh->fluid;
  cds->elementType = nrs->elementType;
  cds->dim = nrs->dim;
  cds->NVfields = nrs->NVfields;
  cds->NSfields = nrs->Nscalar;

  cds->g0 = nrs->g0;
  cds->idt = nrs->idt;

  cds->coeffEXT = nrs->coeffEXT;
  cds->coeffBDF = nrs->coeffBDF;
  cds->nBDF = nrs->nBDF;
  cds->nEXT = nrs->nEXT;
  cds->o_coeffEXT = nrs->o_coeffEXT;
  cds->o_coeffBDF = nrs->o_coeffBDF;

  cds->o_usrwrk = &(nrs->o_usrwrk);

  cds->vFieldOffset = nrs->fieldOffset;
  cds->vCubatureOffset = nrs->cubatureOffset;
  cds->fieldOffset[0] = nrs->fieldOffset;
  cds->fieldOffsetScan[0] = 0;
  dlong sum = cds->fieldOffset[0];
  for (int s = 1; s < cds->NSfields; ++s) {
    cds->fieldOffset[s] = cds->fieldOffset[0];
    cds->fieldOffsetScan[s] = sum;
    sum += cds->fieldOffset[s];
    cds->mesh[s] = cds->meshV;
  }
  cds->fieldOffsetSum = sum;

  cds->o_fieldOffsetScan = platform->device.malloc<dlong>(cds->NSfields, cds->fieldOffsetScan.data());

  cds->gsh = nrs->gsh;
  cds->gshT = (nrs->cht) ? oogs::setup(mesh->ogs, 1, nrs->fieldOffset, ogsDfloat, NULL, OOGS_AUTO) : cds->gsh;

  cds->U = nrs->U;
  cds->S = (dfloat *)calloc(std::max(cds->nBDF, cds->nEXT) * cds->fieldOffsetSum, sizeof(dfloat));

  cds->Nsubsteps = nrs->Nsubsteps;
  if (cds->Nsubsteps) {
    cds->nRK = nrs->nRK;
    cds->coeffsfRK = nrs->coeffsfRK;
    cds->weightsRK = nrs->weightsRK;
    cds->nodesRK = nrs->nodesRK;
    cds->o_coeffsfRK = nrs->o_coeffsfRK;
    cds->o_weightsRK = nrs->o_weightsRK;
  }

  cds->dt = nrs->dt;

  cds->o_prop = device.malloc<dfloat>(2 * cds->fieldOffsetSum);
  cds->o_diff = cds->o_prop.slice(0 * cds->fieldOffsetSum);
  cds->o_rho = cds->o_prop.slice(1 * cds->fieldOffsetSum);

  for (int is = 0; is < cds->NSfields; is++) {
    const std::string sid = scalarDigitStr(is);

    if (options.compareArgs("SCALAR" + sid + " SOLVER", "NONE")) {
      continue;
    }

    dfloat diff = 1;
    dfloat rho = 1;
    options.getArgs("SCALAR" + sid + " DIFFUSIVITY", diff);
    options.getArgs("SCALAR" + sid + " DENSITY", rho);

    auto o_diff = cds->o_diff + cds->fieldOffsetScan[is];
    auto o_rho = cds->o_rho + cds->fieldOffsetScan[is];
    platform->linAlg->fill(mesh->Nlocal, diff, o_diff);
    platform->linAlg->fill(mesh->Nlocal, rho, o_rho);
  }

  cds->o_ellipticCoeff = nrs->o_ellipticCoeff;

  cds->o_relUrst = nrs->o_relUrst;
  cds->o_Urst = nrs->o_Urst;

  cds->anyCvodeSolver = false;
  cds->anyEllipticSolver = false;

  cds->EToBOffset = cds->mesh[0]->Nelements * cds->mesh[0]->Nfaces;

  cds->EToB = (int *)calloc(cds->EToBOffset * cds->NSfields, sizeof(int));

  for (int is = 0; is < cds->NSfields; is++) {
    std::string sid = scalarDigitStr(is);

    cds->compute[is] = 1;
    if (options.compareArgs("SCALAR" + sid + " SOLVER", "NONE")) {
      cds->compute[is] = 0;
      cds->cvodeSolve[is] = 0;
      continue;
    }

    cds->cvodeSolve[is] = options.compareArgs("SCALAR" + sid + " SOLVER", "CVODE");
    cds->anyCvodeSolver |= cds->cvodeSolve[is];
    cds->anyEllipticSolver |= (!cds->cvodeSolve[is] && cds->compute[is]);

    mesh_t *mesh;
    (is) ? mesh = cds->meshV : mesh = cds->mesh[0]; // only first scalar can be a CHT mesh

    int cnt = 0;
    for (int e = 0; e < mesh->Nelements; e++) {
      for (int f = 0; f < mesh->Nfaces; f++) {
        cds->EToB[cnt + cds->EToBOffset * is] = bcMap::id(mesh->EToB[f + e * mesh->Nfaces], "scalar" + sid);
        cnt++;
      }
    }
  }
  cds->o_EToB = device.malloc<int>(cds->EToBOffset * cds->NSfields, cds->EToB);

  cds->o_compute = platform->device.malloc<dlong>(cds->NSfields, cds->compute.data());
  cds->o_cvodeSolve = platform->device.malloc<dlong>(cds->NSfields, cds->cvodeSolve.data());

  cds->o_U = nrs->o_U;
  cds->o_Ue = nrs->o_Ue;
  int nFieldsAlloc = cds->anyEllipticSolver ? std::max(cds->nBDF, cds->nEXT) : 1;
  cds->o_S = platform->device.malloc<dfloat>(nFieldsAlloc * cds->fieldOffsetSum, cds->S);

  nFieldsAlloc = cds->anyEllipticSolver ? cds->nEXT : 1;
  cds->o_FS = platform->device.malloc<dfloat>(nFieldsAlloc * cds->fieldOffsetSum);

  if (cds->anyEllipticSolver) {
    cds->o_Se = platform->device.malloc<dfloat>(cds->fieldOffsetSum);
    cds->o_BF = platform->device.malloc<dfloat>(cds->fieldOffsetSum);
  }

  bool scalarFilteringEnabled = false;
  bool avmEnabled = false;
  {
    for (int is = 0; is < cds->NSfields; is++) {
      std::string sid = scalarDigitStr(is);

      if (!options.compareArgs("SCALAR" + sid + " REGULARIZATION METHOD", "NONE")) {
        scalarFilteringEnabled = true;
      }

      if (options.compareArgs("SCALAR" + sid + " REGULARIZATION METHOD", "AVM_RESIDUAL")) {
        avmEnabled = true;
      }
      if (options.compareArgs("SCALAR" + sid + " REGULARIZATION METHOD", "AVM_HIGHEST_MODAL_DECAY")) {
        avmEnabled = true;
      }
    }
  }

  cds->applyFilter = 0;

  if (scalarFilteringEnabled) {

    std::vector<dlong> applyFilterRT(cds->NSfields, 0);
    const dlong Nmodes = cds->mesh[0]->N + 1;
    cds->o_filterRT = platform->device.malloc<dfloat>(cds->NSfields * Nmodes * Nmodes);
    cds->o_filterS = platform->device.malloc<dfloat>(cds->NSfields);
    cds->o_applyFilterRT = platform->device.malloc<dlong>(cds->NSfields);
    for (int is = 0; is < cds->NSfields; is++) {
      std::string sid = scalarDigitStr(is);

      if (options.compareArgs("SCALAR" + sid + " REGULARIZATION METHOD", "NONE")) {
        continue;
      }
      if (!cds->compute[is]) {
        continue;
      }

      if (options.compareArgs("SCALAR" + sid + " REGULARIZATION METHOD", "HPFRT")) {
        int filterNc = -1;
        options.getArgs("SCALAR" + sid + " HPFRT MODES", filterNc);
        dfloat filterS;
        options.getArgs("SCALAR" + sid + " HPFRT STRENGTH", filterS);
        filterS = -1.0 * fabs(filterS);
        cds->filterS[is] = filterS;

        cds->o_filterRT.copyFrom(lowPassFilter(cds->mesh[is], filterNc), Nmodes * Nmodes, is * Nmodes * Nmodes);

        applyFilterRT[is] = 1;
        cds->applyFilter = 1;
      }
    }

    cds->o_filterS.copyFrom(cds->filterS.data(), cds->NSfields);
    cds->o_applyFilterRT.copyFrom(applyFilterRT.data(), cds->NSfields);

    if (avmEnabled) {
      avm::setup(cds);
    }
  }

  std::string kernelName;
  const std::string suffix = "Hex3D";
  {
    kernelName = "strongAdvectionVolume" + suffix;
    cds->strongAdvectionVolumeKernel = platform->kernels.get(section + kernelName);

    if (platform->options.compareArgs("ADVECTION TYPE", "CUBATURE")) {
      kernelName = "strongAdvectionCubatureVolume" + suffix;
      cds->strongAdvectionCubatureVolumeKernel = platform->kernels.get(section + kernelName);
    }

    kernelName = "advectMeshVelocity" + suffix;
    cds->advectMeshVelocityKernel = platform->kernels.get(section + kernelName);

    kernelName = "maskCopy";
    cds->maskCopyKernel = platform->kernels.get(section + kernelName);

    kernelName = "maskCopy2";
    cds->maskCopy2Kernel = platform->kernels.get(section + kernelName);

    kernelName = "sumMakef";
    cds->sumMakefKernel = platform->kernels.get(section + kernelName);

    kernelName = "neumannBC" + suffix;
    cds->neumannBCKernel = platform->kernels.get(section + kernelName);
    kernelName = "dirichletBC";
    cds->dirichletBCKernel = platform->kernels.get(section + kernelName);

    kernelName = "setEllipticCoeff";
    cds->setEllipticCoeffKernel = platform->kernels.get(section + kernelName);

    kernelName = "filterRT" + suffix;
    cds->filterRTKernel = platform->kernels.get(section + kernelName);

    if (cds->Nsubsteps) {
      if (platform->options.compareArgs("ADVECTION TYPE", "CUBATURE")) {
        kernelName = "subCycleStrongCubatureVolume" + suffix;
        cds->subCycleStrongCubatureVolumeKernel = platform->kernels.get(section + kernelName);
      }
      kernelName = "subCycleStrongVolume" + suffix;
      cds->subCycleStrongVolumeKernel = platform->kernels.get(section + kernelName);
    }
  }

  cds->cvode = nullptr;

  return cds;
}
