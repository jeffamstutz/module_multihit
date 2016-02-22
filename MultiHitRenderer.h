// ======================================================================== //
// Copyright 2015 SURVICE Engineering Company                               //
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

#pragma once

// ospray
#include "ospray/render/Renderer.h"
// embree
#include "embree2/rtcore.h"
#include "embree2/rtcore_scene.h"

namespace ospray {
  struct Camera;
  struct Model;
  struct Volume;
  
  namespace mhtk {

    // /*! \defgroup mhtk_module_xray "XRay" Test Renderer for MHTK Module 
    //   @ingroup mhtk_module
    //   @{ */

    /*! \brief ISPC variant of the sample "XRay" renderer to test
        the multi-hit traversal kernel (\ref mhtk_module) */
    struct MultiHitRenderer : public Renderer {
      MultiHitRenderer();

      std::string toString() const override;
      void commit() override;
      void endFrame(void *perFrameData, const int32 fbChannelFlags) override;

      Model *model;
      int *intersections;
      int *activeLanes;
      int *swaps;
      int bufferWidth;
    };

    /*! @} */
  }
}
