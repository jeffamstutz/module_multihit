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

// '1' for sorting after intersection
#define MHTK_POST_SORTING (0)

#define MHTK_LAYOUT_AOS (0)

// '1' to visualize # of sort swaps, '0' for average # of divergent sorts
#define MHTK_SHOW_SWAPS (1)

// '1' to visualize ray coherence, '0' to show swaps
#define MHTK_SHOW_RAY_COHERENCE (0)

// '1' to activate calculating performance data (required for visuals)
#define MHTK_MEASUREMENTS (1)

#if MHTK_SHOW_RAY_COHERENCE
#  undef MHTK_SHOW_SWAPS
#  define MHTK_SHOW_SWAPS (0)
#endif

#ifndef MHTK_SORTING
#  undef MHTK_SHOW_RAY_COHERENCE
#  define MHTK_SHOW_RAY_COHERENCE (1)
#endif

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

inline void sortHits_SoA(varying MultiHitInfo *uniform mhi)
{
  /* sort the hitpoints (only for measuring performance) */
  for (uniform int i = 0; i < mhi->numHits - 1; ++i)
  {
    float d = mhi->hitArray[i].t;
#if !INDEX_SORT
    int pos = i;
    bool doSwap = false;
#endif
    for (int j = i+1; j < mhi->numHits; ++j)
    {
      if(mhi->hitArray[j].t < d)
      {
#if INDEX_SORT
        mhi->hitOrder[i] = j;
#  if MHTK_MEASUREMENTS
        mhi->numSwaps++;

        /* count the number of incoherent lanes */
        uniform unique = 0;
        foreach_unique(val in programIndex)
          unique++;
        mhi->numCoherent += unique;
#  endif
#else
        doSwap = true;
        pos = j;
#endif
        d = mhi->hitArray[j].t;
      }
    }

#if !INDEX_SORT
    if (doSwap)
    {
      varying MHTKHit tmp = mhi->hitArray[i];
      mhi->hitArray[i]    = mhi->hitArray[pos];
      mhi->hitArray[pos]  = tmp;

#  if MHTK_MEASUREMENTS
      mhi->numSwaps++;

      /* count the number of incoherent lanes */
      uniform unique = 0;
      foreach_unique(val in programIndex)
        unique++;
      mhi->numCoherent += unique;
#  endif
    }
#endif
  }
}

inline void sortHits_AoS(MultiHitInfoAoS *uniform mhi)
{
  foreach_active (l)
  {
    uniform MultiHitInfo *uniform umhi = &((*mhi)[l]);
    for (uniform int i = 0; i < umhi->numHits - 1; ++i)
    {
      uniform float d = umhi->hitArray[i].t;
#if !INDEX_SORT
      uniform int pos = i;
      uniform bool doSwap = false;
#endif
      for (uniform int j = i+1; j < umhi->numHits; ++j)
      {
        if(umhi->hitArray[j].t < d)
        {
#if INDEX_SORT
          umhi->hitOrder[i] = j;
#else
          doSwap = true;
          pos = j;
#endif
          d = umhi->hitArray[j].t;
        }
      }

#if !INDEX_SORT
      if (doSwap)
      {
        uniform MHTKHit tmp = umhi->hitArray[i];
        umhi->hitArray[i]     = umhi->hitArray[pos];
        umhi->hitArray[pos]   = tmp;
      }
#endif
    }
  }
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
    sample.rgb = make_vec3f(1);//NOTE(jda) - use a configurable bg color!

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
