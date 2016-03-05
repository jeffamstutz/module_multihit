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


#include "ospray/fb/FrameBuffer.ih"
#include "ospray/camera/PerspectiveCamera.ih"
#include "ospray/common/Ray.ih"
#include "ospray/render/util.ih"
#include "ospray/render/Renderer.ih"
#include "ospray/common/Model.ih"
#include "multihit_kernel.ih"

struct MultiHitRenderer
{
  uniform Renderer super;
  RTCScene embreeSceneHandle;
  uniform int *uniform intersections;
  uniform int *uniform lanesActive;
  uniform int *uniform swaps;
  uniform int bufferWidth;
};

inline vec3f make_bin_color(const int i, const int max)
{
  float f = i;
  if (i == 0)
    return make_vec3f(0.f);                                   // black
  else if (i <= max/4)
    return make_vec3f(0.f, 0.f, f/(max/4));                   // bluish
  else if (i <= max/2)
    return make_vec3f(0.f, f/(max/4) - 1.f, 2.f - f/(max/4)); // cyan/green
  else if (i <= 3*max/4)
    return make_vec3f(f/(max/4)-2.f, 1.f, 0.f);               // green/yellow
  else if (i <= max)
    return make_vec3f(1.f, 4.0-f/(max/4), 0.f);               // orange
  else
    return make_vec3f(1.f, 0.0, 0.f);                         // red
}

void MultiHitRenderer_renderSample(uniform Renderer *uniform _renderer,
                                   void *uniform /*perFrameData*/,
                                   varying ScreenSample &sample)
{
  uniform MultiHitRenderer *uniform renderer =
      (uniform MultiHitRenderer *uniform)_renderer;

#if MHTK_LAYOUT_AOS
  MultiHitInfoAoS mhi;
  foreach_active (l)
    mhi[l].numHits = 0;
#else
  MultiHitInfo mhi;
  mhi.numHits     = 0;
  mhi.numSwaps    = 0;
  mhi.numCoherent = 0;
#endif

  sample.ray.userData = &mhi;

  traceRay(renderer->super.model, sample.ray);

#if MHTK_POST_SORTING
#  if MHTK_LAYOUT_AOS
  sortHits_AoS(&mhi);
#  else
  sortHits_SoA(&mhi);
#  endif
#endif

#if !MHTK_LAYOUT_AOS
  if (mhi.numHits > 0)
#  if MHTK_SHOW_RAY_COHERENCE
    sample.rgb = make_bin_color(programCount - (mhi.numCoherent /
                                                (float)mhi.numHits),
                                programCount);
#  else
#    if MHTK_SHOW_SWAPS
    sample.rgb = make_bin_color(mhi.numSwaps, 128);
#    else
    sample.rgb = make_bin_color(programCount - (mhi.numCoherent /
                                                (float)mhi.numSwaps),
                                programCount);
#    endif
#  endif
  else
    sample.rgb = renderer->super.bgColor;

  const int id = (renderer->bufferWidth*sample.sampleID.y) + sample.sampleID.x;
  if (renderer->intersections) renderer->intersections[id] = mhi.numHits;
  if (renderer->lanesActive)   renderer->lanesActive[id]   = mhi.numCoherent;
  if (renderer->swaps)         renderer->swaps[id]         = mhi.numSwaps;
#endif
}

export void *uniform MultiHitRenderer_create(void *uniform cppE)
{
  uniform MultiHitRenderer *uniform renderer =
      uniform new uniform MultiHitRenderer;
  Renderer_Constructor(&renderer->super,cppE);
  renderer->super.renderSample = MultiHitRenderer_renderSample;
  return renderer;
}

export void MultiHitRenderer_set(void *uniform _renderer,
                             void *uniform _model,
                             void *uniform _camera,
                             void *uniform _intersections,
                             void *uniform _lanesActive,
                             void *uniform _swaps,
                             uniform int32 _bufferWidth)
{
  uniform MultiHitRenderer *uniform renderer =
      (uniform MultiHitRenderer *uniform)_renderer;
  uniform Model  *uniform model  = (uniform Model  *uniform)_model;
  uniform Camera *uniform camera = (uniform Camera *uniform)_camera;
  renderer->super.model = model;
  renderer->super.camera = camera;
  renderer->embreeSceneHandle = model->embreeSceneHandle;
  renderer->intersections = (uniform int *uniform)_intersections;
  renderer->lanesActive = (uniform int *uniform)_lanesActive;
  renderer->swaps = (uniform int *uniform) _swaps;
  renderer->bufferWidth = _bufferWidth;
}
