// ======================================================================== //
// Copyright 2009-2016 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#include "MultiHitTriangles.h"
#include "MultiHitTriangles_ispc.h"
#include "common/Model.h"

namespace ospray {

std::string MultiHitTriangles::toString() const
{
  return "ospray::MultiHitTriangles";
}

void ospray::MultiHitTriangles::finalize(Model *model)
{
  TriangleMesh::finalize(model);

  ispc::MultihitTriangles_init(model->embreeSceneHandle, this->eMesh);
}

OSP_REGISTER_GEOMETRY(MultiHitTriangles,mhtriangles);

}//namespace ospray
