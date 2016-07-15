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

#include "geometry/Geometry.ih"
#include "embree2/rtcore_geometry.isph"
#include "multihit_kernel.ih"

void multiHitFilterPostSoA(void *uniform geom, varying RTCRay& _ray)
{
  varying Ray &ray = (varying Ray &)_ray;

  varying MultiHitInfo *uniform mhi =
      (varying MultiHitInfo *uniform)ray.userData;

  if (ray.geomID < 0 || mhi->numHits >= MAX_HITS_PER_TRACE) return;

  MHTKHit hit;
  hit.primID = ray.primID;
  hit.geomID = ray.geomID;
  hit.t      = ray.t;
  hit.Ng.x   = ray.Ng.x;
  hit.Ng.y   = ray.Ng.y;
  hit.Ng.z   = ray.Ng.z;

  const int pos = mhi->numHits;
  mhi->hitArray[pos] = hit;

  mhi->numHits++;
  ray.geomID = RTC_INVALID_GEOMETRY_ID; // -> discard this hit, we want them all
}

void multiHitFilterProgSoA(void *uniform geom, varying RTCRay& _ray)
{
  varying Ray &ray = (varying Ray &)_ray;

  varying MultiHitInfo *uniform mhi =
      (varying MultiHitInfo *uniform)ray.userData;

  if (ray.geomID < 0 || mhi->numHits >= MAX_HITS_PER_TRACE) return;

  MHTKHit hit;
  hit.primID = ray.primID;
  hit.geomID = ray.geomID;
  hit.t      = ray.t;
  hit.Ng.x   = ray.Ng.x;
  hit.Ng.y   = ray.Ng.y;
  hit.Ng.z   = ray.Ng.z;

  uniform int i = reduce_max(mhi->numHits)-1;
  int j = i;
  for (; i >= 0; --i, --j)
  {
    if (i < mhi->numHits)
    {
      if (mhi->hitArray[i].t > hit.t)
      {
        mhi->hitArray[i+1] = mhi->hitArray[i];
# if MHTK_MEASUREMENTS
        mhi->numSwaps++;
        /* count the number of incoherent lanes */
        uniform unique = 0;
        foreach_unique(val in programIndex)
          unique++;
        mhi->numCoherent += unique;
# endif
      }
      else
        break;
    }
  }

  mhi->hitArray[j+1] = hit;

#if MHTK_MEASUREMENTS
#  if !MHTK_POST_SORTING
  /* count the number of incoherent lanes */
  uniform unique = 0;
  foreach_unique(val in programIndex)
    unique++;
  mhi->numCoherent += unique;
#  endif
#endif

  mhi->numHits++;
  ray.geomID = RTC_INVALID_GEOMETRY_ID; // -> discard this hit, we want them all
}

#if INDEX_SORT
void multiHitFilterProgSoAIndex(void *uniform geom, varying RTCRay& _ray)
{
  varying Ray &ray = (varying Ray &)_ray;

  varying MultiHitInfo *uniform mhi =
      (varying MultiHitInfo *uniform)ray.userData;

  if (ray.geomID < 0 || mhi->numHits >= MAX_HITS_PER_TRACE) return;

  MHTKHit hit;
  hit.primID = ray.primID;
  hit.geomID = ray.geomID;
  hit.t      = ray.t;
  hit.Ng.x   = ray.Ng.x;
  hit.Ng.y   = ray.Ng.y;
  hit.Ng.z   = ray.Ng.z;

  const int pos = mhi->numHits;
  mhi->hitArray[pos] = hit;

  uniform int i = reduce_max(mhi->numHits)-1;
  int j = i;
  for (; i >= 0; --i, --j)
  {
    if (i < mhi->numHits)
    {
      if (mhi->hitArray[mhi->hitOrder[i]].t > ray.t)
      {
        mhi->hitOrder[i+1] = mhi->hitOrder[i];
# if MHTK_MEASUREMENTS
        mhi->numSwaps++;
        /* count the number of incoherent lanes */
        uniform unique = 0;
        foreach_unique(val in programIndex)
          unique++;
        mhi->numCoherent += unique;
# endif
      }
      else
        break;
    }
  }

  mhi->hitOrder[j+1] = pos;

#if MHTK_MEASUREMENTS
#  if !MHTK_POST_SORTING
  /* count the number of incoherent lanes */
  uniform unique = 0;
  foreach_unique(val in programIndex)
    unique++;
  mhi->numCoherent += unique;
#  endif
#endif

  mhi->numHits++;
  ray.geomID = RTC_INVALID_GEOMETRY_ID; // -> discard this hit, we want them all
}
#endif

/*! Intersection filter function for varying rays. */
void multiHitFilterPostAoS(void *uniform geom, varying RTCRay& _ray)
{
  varying Ray &ray = (varying Ray &)_ray;

  if (ray.geomID < 0)
    return;

  uniform MultiHitInfo *uniform mhiaos =
      (uniform MultiHitInfo *uniform)ray.userData;
  foreach_active (l)
  {
    uniform MultiHitInfo *uniform umhi = &mhiaos[l];
    const uniform float t = extract(ray.t, l);

    if (umhi->numHits >= MAX_HITS_PER_TRACE)
      continue;

    uniform int i = umhi->numHits-1;

    uniform MHTKHit hit;
    hit.primID = extract(ray.primID, l);
    hit.geomID = extract(ray.geomID, l);
    hit.t      = t;
    hit.Ng.x   = extract(ray.Ng.x, l);
    hit.Ng.y   = extract(ray.Ng.y, l);
    hit.Ng.z   = extract(ray.Ng.z, l);

    umhi->hitArray[i+1] = hit;
    umhi->numHits++;
  }

  ray.geomID = RTC_INVALID_GEOMETRY_ID; // -> discard this hit, we want them all
}

/*! Intersection filter function for varying rays. */
void multiHitFilterProgAoS(void *uniform geom, varying RTCRay& _ray)
{
  varying Ray &ray = (varying Ray &)_ray;

  if (ray.geomID < 0)
    return;

  uniform MultiHitInfo *uniform mhiaos =
      (uniform MultiHitInfo *uniform)ray.userData;
  foreach_active (l)
  {
    uniform MultiHitInfo *uniform umhi = &mhiaos[l];
    const uniform float t = extract(ray.t, l);

    if (umhi->numHits >= MAX_HITS_PER_TRACE)
      continue;

    uniform int i = umhi->numHits-1;
    for (; i >= 0; --i)
    {
      if (umhi->hitArray[i].t > t)
        umhi->hitArray[i+1] = umhi->hitArray[i];
      else
        break;
    }

    uniform MHTKHit hit;
    hit.primID = extract(ray.primID, l);
    hit.geomID = extract(ray.geomID, l);
    hit.t      = t;
    hit.Ng.x   = extract(ray.Ng.x, l);
    hit.Ng.y   = extract(ray.Ng.y, l);
    hit.Ng.z   = extract(ray.Ng.z, l);

    umhi->hitArray[i+1] = hit;
    umhi->numHits++;
  }

  ray.geomID = RTC_INVALID_GEOMETRY_ID; // -> discard this hit, we want them all
}

#if INDEX_SORT
void multiHitFilterProgAoSIndex(void *uniform geom, varying RTCRay& _ray)
{
  varying Ray &ray = (varying Ray &)_ray;

  if (ray.geomID < 0)
    return;

  uniform MultiHitInfo *uniform mhiaos =
      (uniform MultiHitInfo *uniform)ray.userData;
  foreach_active (l)
  {
    uniform MultiHitInfo *uniform umhi = &mhiaos[l];
    const uniform float t = extract(ray.t, l);

    if (umhi->numHits >= MAX_HITS_PER_TRACE)
      continue;

    uniform int i = umhi->numHits-1;
    for (; i >= 0; --i)
    {
      if (umhi->hitArray[umhi->hitOrder[i]].t > t)
        umhi->hitOrder[i+1] = umhi->hitOrder[i];
      else
        break;
    }

    uniform MHTKHit hit;
    hit.primID = extract(ray.primID, l);
    hit.geomID = extract(ray.geomID, l);
    hit.t      = t;
    hit.Ng.x   = extract(ray.Ng.x, l);
    hit.Ng.y   = extract(ray.Ng.y, l);
    hit.Ng.z   = extract(ray.Ng.z, l);

    const uniform int pos = umhi->numHits;
    umhi->hitArray[pos] = hit;

    umhi->hitOrder[i+1] = pos;

    umhi->numHits++;
  }

  ray.geomID = RTC_INVALID_GEOMETRY_ID; // -> discard this hit, we want them all
}
#endif

export void MultihitTriangles_init(void *uniform _scene, uniform int32 geomID)
{
  RTCScene scene = (RTCScene)_scene;
  RTCFilterFuncVarying filter_function;

#if MHTK_LAYOUT_AOS
#  if MHTK_POST_SORTING
  filter_function = multiHitFilterPostAoS;
#  else
#    if INDEX_SORT
  filter_function = multiHitFilterProgAoSIndex;
#    else
  filter_function = multiHitFilterProgAoS;
#    endif
#  endif
#else
#  if MHTK_POST_SORTING
  filter_function = multiHitFilterPostSoA;
#  else
#    if INDEX_SORT
  filter_function = multiHitFilterProgSoAIndex;
#    else
  filter_function = multiHitFilterProgSoA;
#    endif
#  endif
#endif

  rtcSetIntersectionFilterFunction(scene, geomID, filter_function);
}
