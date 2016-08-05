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

#undef NDEBUG

#include "MultiHitRenderer.h"
#include "ospray/camera/PerspectiveCamera.h"
#include "ospray/volume/Volume.h"
// ospray stuff
#include "ospray/common/Ray.h"
#include "ospray/common/Data.h"
// ispc exports
#include "MultiHitRenderer_ispc.h"

#include <iostream>
using std::cout;
using std::endl;

#define STATS(a) 

namespace ospray {
namespace mhtk   {

    /*! \addtogroup mhtk_module_xray
      @{ */
    STATS(int64 numTotalFound=-1);
    STATS(int64 maxFound=-1);
    STATS(int64 numAnyFound=-1);

    MultiHitRenderer::MultiHitRenderer()
    { 
      ispcEquivalent = ispc::MultiHitRenderer_create(this);
    }

    std::string MultiHitRenderer::toString() const
    {
      return "ospray::mhtk::MultiHitRenderer";
    }

    /*! \brief create render job for ispc-based mhtk::xray renderer */
    void MultiHitRenderer::commit()
    {
      Renderer::commit();
      Camera *camera = (Camera *)getParamObject("camera");

      Data *ospIntersections = getParamData("intersections");
      Data *ospLanes = getParamData("activeLanes");
      Data *ospSwaps = getParamData("swaps");

      if (ospIntersections) intersections = (int*)ospIntersections->data;
      if (ospLanes) activeLanes = (int*)ospLanes->data;
      if (ospSwaps) swaps = (int*)ospSwaps->data;

      bufferWidth = getParam1i("bufferWidth", 0);

      if (model && camera)
      {
        ispc::MultiHitRenderer_set(getIE(),model->getIE(),camera->getIE(), &mhi,
                               intersections, activeLanes, swaps, bufferWidth);
      }
    }

    void MultiHitRenderer::endFrame(void *perFrameData,
                                    const int32 fbChannelFlags)
    {
      Renderer::endFrame(perFrameData, fbChannelFlags);

      cout << "Data: " << endl;
      cout << "     numHits: " << mhi.numHits << endl;
      cout << "    numSwaps: " << mhi.numSwaps << endl;
      cout << " numCoherent: " << mhi.numCoherent << endl;
      for (int i = 0; i < mhi.numHits; ++i) {
        auto &hit = mhi.hitArray[i];
        cout << " t" << i << ": ";
        cout << '{' << hit.t << ',' << hit.geomID << ',' << hit.primID;
        cout << ",{" << hit.Ng.x << ',' << hit.Ng.y << ',' << hit.Ng.z << "}}";
      }
      cout << endl << endl;

      if (activeLanes)
      {
        int w = bufferWidth;
        int h = bufferWidth; // XXX assumes square images
        // Sum reduce data and output results
        size_t totalIntersections = 0;
        size_t totalLanes         = 0;
        size_t rayHitCount        = 0;
        size_t totalSwaps         = 0;
        for (int i = 0; i < w*h; ++i)
        {
          const size_t numIs = static_cast<size_t>(intersections[i]);
          const size_t numLs = static_cast<size_t>(activeLanes[i]);
          totalIntersections += numIs;
          rayHitCount        += (numIs > 0) ? 1 : 0;
          totalLanes         += static_cast<size_t>(numLs);
          totalSwaps         += static_cast<size_t>(swaps[i]);
        }

        cout << endl << "-- Renderer Intersection data --" << endl;
        cout << "         Total Rays: " << w*h << endl;
        cout << "           Hit Rays: " << rayHitCount << endl;
        cout << "Total Intersections: " << totalIntersections << endl;
        cout << "   AVG lanes active: " << totalLanes/(double)totalSwaps << endl;
        cout << "          AVG swaps: " << totalSwaps/(double)rayHitCount << endl;
        cout << "           MIPS/fps: " << totalIntersections/(1024.f*1024) << endl;
        cout.flush();
      }
    }

    OSP_REGISTER_RENDERER(MultiHitRenderer,multihit)
    OSP_REGISTER_RENDERER(MultiHitRenderer,mhtk_xray_ispc)
    OSP_REGISTER_RENDERER(MultiHitRenderer,mhtk)
    OSP_REGISTER_RENDERER(MultiHitRenderer,xray)
    /*! @} */
    /*! \brief module initialization function \ingroup module_mhtk */
    extern "C" void ospray_init_module_multihit()
    {
      printf("Loaded 'multi-hit traversal' plugin ...\n");
    }

}// namespace mhtk
}// namespace ospray

