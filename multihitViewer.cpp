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

#include "common/widgets/OSPGlutViewer.h"
#include "common/commandline/Utility.h"
#include "common/commandline/SceneParser/trianglemesh/TriangleMeshSceneParser.h"
#include "ospray/ospray.h"

class MultiHitRendererParser : public RendererParser
{
public:
  bool parse(int ac, const char **&av) override { return true; }
  ospray::cpp::Renderer renderer() override { return std::string("multihit"); }
};

class MultiHitSceneParser : public TriangleMeshSceneParser
{
public:
  MultiHitSceneParser(ospray::cpp::Renderer renderer,
                      std::string geometryType = "mhtriangles") :
    TriangleMeshSceneParser(renderer, geometryType) {}
};

int main(int ac, const char **av)
{
  ospInit(&ac,av);
  ospray::glut3D::initGLUT(&ac,av);

  ospLoadModule("multihit");
  auto ospObjs = parseCommandLine<MultiHitRendererParser,
                                  DefaultCameraParser,
                                  MultiHitSceneParser,
                                  DefaultLightsParser>(ac, av);

  std::deque<ospcommon::box3f>      bbox;
  std::deque<ospray::cpp::Model>    model;
  ospray::cpp::Renderer renderer;
  ospray::cpp::Camera   camera;

  std::tie(bbox, model, renderer, camera) = ospObjs;

  ospray::OSPGlutViewer window(bbox, model, renderer, camera);
  window.create("ospMultihitViewer: OSPRay Multihit Viewer");

  ospray::glut3D::runGLUT();
}
