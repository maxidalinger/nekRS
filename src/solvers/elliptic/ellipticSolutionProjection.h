/*

   The MIT License (MIT)

   Copyright (c) 2017 Tim Warburton, Noel Chalmers, Jesse Chan, Ali Karakus

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.

 */

#ifndef ELLIPTIC_RESIDUAL_PROJECTION_H
#define ELLIPTIC_RESIDUAL_PROJECTION_H
#include <vector>
#include <sstream>
#include <functional>
#include "elliptic.h"

class SolutionProjection final
{
public:
  enum class ProjectionType {
    CLASSIC,
    ACONJ,
  };
  SolutionProjection(elliptic_t& _elliptic,
                     const ProjectionType _type,
                     const dlong _maxNumVecsProjection = 8,
                     const dlong _numTimeSteps = 5);
  void pre(occa::memory& o_r);
  void post(occa::memory& o_x);
  dlong getNumVecsProjection() const { return numVecsProjection; }
  dlong getPrevNumVecsProjection() const { return prevNumVecsProjection; }
  dlong getMaxNumVecsProjection() const { return maxNumVecsProjection; }
private:
  void computePreProjection(occa::memory& o_r);
  void computePostProjection(occa::memory& o_x);
  void updateProjectionSpace();
  void matvec(occa::memory& o_Ax, const dlong Ax_offset, const occa::memory& o_x, const dlong x_offset);
  const dlong maxNumVecsProjection;
  const dlong numTimeSteps;
  const ProjectionType type;
  dlong timestep;
  bool verbose;

  std::string solverName;

  occa::memory o_xbar;
  occa::memory o_xx;
  occa::memory o_bb;
  occa::memory o_alpha;
  // references to memory on elliptic
  occa::memory& o_invDegree;
  occa::memory& o_rtmp;
  occa::memory& o_Ap;

  occa::kernel scalarMultiplyKernel;
  occa::kernel multiScaledAddwOffsetKernel;
  occa::kernel accumulateKernel;

  dfloat* alpha;

  dlong numVecsProjection;
  dlong prevNumVecsProjection;
  const dlong Nlocal; // vector size
  const dlong fieldOffset; // offset
  const dlong Nfields;

  std::function<void(const occa::memory&, occa::memory&)> matvecOperator;
  std::function<void(occa::memory&)> maskOperator;
};
#endif
