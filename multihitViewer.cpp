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

#define WARN_ON_INCLUDING_OSPCOMMON 1

// viewer widget
#include "sample_apps/common/widgets/glut3D.h"
// mini scene graph for loading the model
#include "sample_apps/common/miniSG/miniSG.h"
// ospray, for rendering
#include "ospray/ospray.h"

// stl
#include <algorithm>
#include <sstream>

namespace ospray {
  using namespace ospcommon;

  using std::cout;
  using std::endl;
  bool doShadows = 1;
  OSPTexture2D g_tex; // holds last loaded texture, for debugging

  const char *outFileName = NULL;
  size_t numAccumsFrameInFileOutput = 1;
  size_t numSPPinFileOutput = 1;

  float g_near_clip = 1e-6f;
  bool  g_fullScreen       = false;
  glut3D::Glut3DWidget::ViewPort g_viewPort;

  vec2i g_windowSize;

  int g_benchWarmup = 0, g_benchFrames = 0;
  bool g_alpha = false;
  bool g_createDefaultMaterial = true;
  int accumID = -1;
  int maxAccum = 64;
  int spp = 1; /*! number of samples per pixel */
  int maxDepth = 2; // only set with home/end
  unsigned int maxObjectsToConsider = (uint32_t)-1;
  // if turned on, we'll put each triangle mesh into its own instance, no matter what
  bool forceInstancing = false;
  /*! if turned on we're showing the depth buffer rather than the (accum'ed) color buffer */
  bool showDepthBuffer = 0;
  glut3D::Glut3DWidget::FrameBufferMode g_frameBufferMode = glut3D::Glut3DWidget::FRAMEBUFFER_UCHAR;

  /*! when using the OBJ renderer, we create a automatic dirlight with this direction; use ''--sun-dir x y z' to change */
  vec3f defaultDirLight_direction(.3, -1, -.2);

  Ref<miniSG::Model> msgModel = NULL;
  OSPModel           ospModel = NULL;
  OSPRenderer        ospRenderer = NULL;
  bool alwaysRedraw = false;

  //! the renderer we're about to use
  std::string rendererType = "xray";

  std::vector<miniSG::Model *> msgAnimation;

  void error(const std::string &msg)
  {
    cout << "#ospModelViewer fatal error : " << msg << endl;
    cout << endl;
    cout << "Proper usage: " << endl;
    cout << "  ./ospModelViewer [-bench <warmpup>x<numFrames>] [-model] <inFileName>" << endl;
    cout << endl;
    exit(1);
  }

  struct DisplayWall {
    std::string hostname;
    std::string streamName;
    vec2i size;
    OSPFrameBuffer fb;
    OSPPixelOp     po;

    DisplayWall() : hostname(""), streamName(""), size(-1), fb(NULL), po(NULL)
    {}
  };
  DisplayWall *displayWall = NULL;

  using ospray::glut3D::Glut3DWidget;

  // helper function to write the rendered image as PPM file
  void writePPM(const char *fileName,
      const int sizeX, const int sizeY,
      const uint32_t *pixel)
  {
    FILE *file = fopen(fileName, "wb");
    fprintf(file, "P6\n%i %i\n255\n", sizeX, sizeY);
    unsigned char *out = (unsigned char *)alloca(3*sizeX);
    for (int y = 0; y < sizeY; y++) {
      const unsigned char *in = (const unsigned char *)&pixel[(sizeY-1-y)*sizeX];
      for (int x = 0; x < sizeX; x++) {
        out[3*x + 0] = in[4*x + 0];
        out[3*x + 1] = in[4*x + 1];
        out[3*x + 2] = in[4*x + 2];
      }
      fwrite(out, 3*sizeX, sizeof(char), file);
    }
    fprintf(file, "\n");
    fclose(file);

    std::string alphaName(fileName);
    alphaName.resize(alphaName.length()-4); // remove ".ppm"
    alphaName.append("_alpha.pgm");

    file = fopen(alphaName.c_str(), "wb");
    fprintf(file, "P5\n%i %i\n255\n", sizeX, sizeY);
    for (int y = 0; y < sizeY; y++) {
      const unsigned char *in = (const unsigned char *)&pixel[(sizeY-1-y)*sizeX];
      for (int x = 0; x < sizeX; x++)
        out[x] = in[4*x + 3];
      fwrite(out, sizeX, sizeof(char), file);
    }
    fprintf(file, "\n");
    fclose(file);
  }

  /*! mini scene graph viewer widget. \internal Note that all handling
    of camera is almost exactly similar to the code in volView;
    might make sense to move that into a common class! */
  struct MSGViewer : public Glut3DWidget {
    MSGViewer(OSPModel model, OSPRenderer renderer)
      : Glut3DWidget(Glut3DWidget::FRAMEBUFFER_NONE),
        fb(NULL), renderer(renderer), model(model)
    {
      Assert(model && "null model handle");
      camera = ospNewCamera("perspective");
      Assert(camera != NULL && "could not create camera");
      ospSet3f(camera,"pos",-1,1,-1);
      ospSet3f(camera,"dir",+1,-1,+1);
//      ospSet1f(camera,"fovy",120);
      ospCommit(camera);

      ospSetObject(renderer,"world",model);
      ospSetObject(renderer,"model",model);
      ospSetObject(renderer,"camera",camera);
      ospSet1i(renderer,"spp",spp);
      ospCommit(camera);
      ospCommit(renderer);

    }

    void reshape(const ospray::vec2i &newSize) override
    {
      Glut3DWidget::reshape(newSize);
      g_windowSize = newSize;
      if (fb) ospFreeFrameBuffer(fb);
      fb = ospNewFrameBuffer((const osp::vec2i&)newSize,
                             OSP_FB_SRGBA,
                             OSP_FB_COLOR|OSP_FB_DEPTH|OSP_FB_ACCUM);
      ospSet1f(fb, "gamma", 2.2f);
      ospCommit(fb);
      ospFrameBufferClear(fb,OSP_FB_ACCUM);

      /*! for now, let's just attach the pixel op to the _main_ frame
          buffer - eventually we need to have a _second_ frame buffer
          of the proper (much higher) size, but for now let's just use
          the existing one... */
      if (displayWall && displayWall->fb != fb) {
        PRINT(displayWall->size);
        displayWall->fb = ospNewFrameBuffer((const osp::vec2i&)displayWall->size,
                                            OSP_FB_NONE,
                                            OSP_FB_COLOR|OSP_FB_DEPTH|OSP_FB_ACCUM);
        ospFrameBufferClear(displayWall->fb,OSP_FB_ACCUM);
        if (displayWall->po == NULL) {
          displayWall->po = ospNewPixelOp("display_wall");
          if (!displayWall->po)
            throw std::runtime_error("could not load 'display_wall' component.");
          ospSetString(displayWall->po, "hostname", displayWall->hostname.c_str());
          ospSetString(displayWall->po, "streamName", displayWall->streamName.c_str());
          ospCommit(displayWall->po);
        }

        ospSetPixelOp(displayWall->fb,displayWall->po);
      }

      ospSetf(camera,"aspect",viewPort.aspect);
      ospCommit(camera);
      viewPort.modified = true;
      forceRedraw();
    }

    void keypress(char key, const vec2i &where) override
    {
      switch (key) {
      case 'R':
        alwaysRedraw = !alwaysRedraw;
        forceRedraw();
        break;
      case '!': {
        const uint32_t * p = (uint32_t*)ospMapFrameBuffer(fb, OSP_FB_COLOR);
        writePPM("ospmodelviewer.ppm", g_windowSize.x, g_windowSize.y, p);
        printf("#ospModelViewer: saved current frame to 'ospmodelviewer.ppm'\n");
      } break;
      case 'X':
        if (viewPort.up == vec3f(1,0,0) || viewPort.up == vec3f(-1.f,0,0))
          viewPort.up = - viewPort.up;
        else
          viewPort.up = vec3f(1,0,0);
        viewPort.modified = true;
        forceRedraw();
        break;
      case 'Y':
        if (viewPort.up == vec3f(0,1,0) || viewPort.up == vec3f(0,-1.f,0))
          viewPort.up = - viewPort.up;
        else
          viewPort.up = vec3f(0,1,0);
        viewPort.modified = true;
        forceRedraw();
        break;
      case 'Z':
        if (viewPort.up == vec3f(0,0,1) || viewPort.up == vec3f(0,0,-1.f))
          viewPort.up = - viewPort.up;
        else
          viewPort.up = vec3f(0,0,1);
        viewPort.modified = true;
        forceRedraw();
        break;
      case 'f':
        g_fullScreen = !g_fullScreen;
        if(g_fullScreen) glutFullScreen();
        else glutPositionWindow(0,10);
        break;
      case 'r':
        viewPort = g_viewPort;
        break;
      case 'p':
        printf("-vp %f %f %f -vu %f %f %f -vi %f %f %f\n", viewPort.from.x, viewPort.from.y, viewPort.from.z, viewPort.up.x, viewPort.up.y, viewPort.up.z, viewPort.at.x, viewPort.at.y, viewPort.at.z);
        fflush(stdout);
        break;
      default:
        Glut3DWidget::keypress(key,where);
      }
    }

    void specialkey(int32_t key, const vec2i &where) override
    {
      switch(key) {
      case GLUT_KEY_PAGE_UP:
        g_near_clip += 20.f * motionSpeed;
      case GLUT_KEY_PAGE_DOWN:
        g_near_clip -= 10.f * motionSpeed;
        g_near_clip = std::max(g_near_clip, 1e-6f);
        ospSet1f(camera, "near_clip", g_near_clip);
        ospCommit(camera);
        accumID=0;
        ospFrameBufferClear(fb,OSP_FB_ACCUM);
        forceRedraw();
        break;
      case GLUT_KEY_HOME:
        maxDepth += 2;
      case GLUT_KEY_END:
        maxDepth--;
        ospSet1i(ospRenderer, "maxDepth", maxDepth);
        PRINT(maxDepth);
        ospCommit(ospRenderer);
        accumID=0;
        ospFrameBufferClear(fb,OSP_FB_ACCUM);
        forceRedraw();
        break;
      default:
        Glut3DWidget::specialkey(key,where);
      }
    }

    void mouseButton(int32_t whichButton, bool released, const vec2i &pos) override
    {
      Glut3DWidget::mouseButton(whichButton, released, pos);
      if(currButtonState ==  (1<<GLUT_LEFT_BUTTON) && (glutGetModifiers() & GLUT_ACTIVE_SHIFT) && manipulator == inspectCenterManipulator) {
        vec2f normpos = vec2f(pos.x / (float)windowSize.x, 1.0f - pos.y / (float)windowSize.y);
        OSPPickResult pick;
        ospPick(&pick, ospRenderer, (const osp::vec2f&)normpos);
        if(pick.hit) {
          vec3f delta = (ospray::vec3f&)pick.position - viewPort.at;
          vec3f right = cross(normalize(viewPort.at - viewPort.from), viewPort.up);
          vec3f offset = dot(delta, right) * right - dot(delta, viewPort.up) * viewPort.up;
          viewPort.at = (ospray::vec3f&)pick.position;
          viewPort.modified = true;
          computeFrame();
          forceRedraw();
        }
      }
    }

    void display() override
    {
      if (!fb || !renderer) return;

      static int frameID = 0;

      //{
      // note that the order of 'start' and 'end' here is
      // (intentionally) reversed: due to our asynchrounous rendering
      // you cannot place start() and end() _around_ the renderframe
      // call (which in itself will not do a lot other than triggering
      // work), but the average time between ttwo calls is roughly the
      // frame rate (including display overhead, of course)
      if (frameID > 0) fps.doneRender();
      fps.startRender();
      //}
      static double benchStart=0;
      static double fpsSum=0;
      if (g_benchFrames > 0 && frameID == g_benchWarmup)
        {
          benchStart = ospray::getSysTime();
        }
      if (g_benchFrames > 0 && frameID >= g_benchWarmup)
        fpsSum += fps.getFPS();
      if (g_benchFrames > 0 && frameID== g_benchWarmup+g_benchFrames)
        {
          double time = ospray::getSysTime()-benchStart;
          double avgFps = fpsSum/double(frameID-g_benchWarmup);
          printf("Benchmark: time: %f avg fps: %f avg frame time: %f\n", time, avgFps, time/double(frameID-g_benchWarmup));

          const uint32_t * p = (uint32_t*)ospMapFrameBuffer(fb, OSP_FB_COLOR);
          writePPM("benchmark.ppm", g_windowSize.x, g_windowSize.y, p);

          exit(0);
        }
      ++frameID;

      if (viewPort.modified) {
        static bool once = true;
        if(once) {
          g_viewPort = viewPort;
          once = false;
        }
        Assert2(camera,"ospray camera is null");
        ospSetVec3f(camera,"pos",(osp::vec3f&)viewPort.from);
        vec3f dir = viewPort.at-viewPort.from;
        ospSetVec3f(camera,"dir",(osp::vec3f&)dir);
        ospSetVec3f(camera,"up",(osp::vec3f&)viewPort.up);
        ospSetf(camera,"aspect",viewPort.aspect);
//        ospSetf(camera,"apertureRadius", 0.01);
//        ospSetf(camera,"focusDistance", viewPort."focusDistance");
        ospCommit(camera);
        viewPort.modified = false;
        accumID=0;
        ospFrameBufferClear(fb,OSP_FB_ACCUM);
        if (displayWall)
          ospFrameBufferClear(displayWall->fb,OSP_FB_ACCUM);
      }

      if (outFileName) {
        ospSet1i(renderer,"spp",numSPPinFileOutput);
        ospCommit(renderer);
        std::cout << "#ospModelViewer: Renderering offline image with " << numSPPinFileOutput << " samples per pixel per frame, and accumulation of " << numAccumsFrameInFileOutput << " such frames" << endl;
        for (int i=0;i<numAccumsFrameInFileOutput;i++) {
          ospRenderFrame(fb,renderer,OSP_FB_COLOR|OSP_FB_ACCUM);
          ucharFB = (uint32_t *) ospMapFrameBuffer(fb, OSP_FB_COLOR);
          std::cout << "#ospModelViewer: Saved rendered image (w/ "
                    << i << " accums) in " << outFileName << std::endl;
          writePPM(outFileName, g_windowSize.x, g_windowSize.y, ucharFB);
          ospUnmapFrameBuffer(ucharFB,fb);
        }
        // std::cout << "#ospModelViewer: Saved rendered image in " << outFileName << std::endl;
        // writePPM(outFileName, g_windowSize.x, g_windowSize.y, ucharFB);
        exit(0);
      }

      ospRenderFrame(fb,renderer,OSP_FB_COLOR|OSP_FB_ACCUM);
      if (displayWall)
        ospRenderFrame(displayWall->fb,renderer,OSP_FB_COLOR|OSP_FB_ACCUM);
      ++accumID;

      // set the glut3d widget's frame buffer to the opsray frame buffer, then display
      ucharFB = (uint32_t *) ospMapFrameBuffer(fb, OSP_FB_COLOR);
      frameBufferMode = Glut3DWidget::FRAMEBUFFER_UCHAR;

      Glut3DWidget::display();

      // that pointer is no longer valid, so set it to null
      ucharFB = NULL;

      char title[1000];

      if (alwaysRedraw) {
        sprintf(title,"OSPRay Model Viewer (%f fps)",
                fps.getFPS());
        setTitle(title);
        forceRedraw();
      } else if (accumID < maxAccum) {
        forceRedraw();
      } else {
        // sprintf(title,"OSPRay Model Viewer");
        // setTitle(title);
      }
    }

    OSPModel       model;
    OSPFrameBuffer fb;
    OSPRenderer    renderer;
    OSPCamera      camera;
    ospray::glut3D::FPSCounter fps;
  };

  void warnMaterial(const std::string &type)
  {
    static std::map<std::string,int> numOccurances;
    if (numOccurances[type] == 0)
      cout << "could not create material type '"<<type<<"'. Replacing with default material." << endl;
    numOccurances[type]++;
  }

  OSPMaterial createDefaultMaterial(OSPRenderer renderer)
  {
    if(!g_createDefaultMaterial) return NULL;
    static OSPMaterial ospMat = NULL;
    if (ospMat) return ospMat;

    ospMat = ospNewMaterial(renderer,"OBJMaterial");
    if (!ospMat)  {
      throw std::runtime_error("could not create default material 'OBJMaterial'");
      // cout << "given renderer does not know material type 'OBJMaterial'" << endl;
    }
    ospSet3f(ospMat, "Kd", .8f, 0.f, 0.f);
    ospCommit(ospMat);
    return ospMat;
  }

  OSPTexture2D createTexture2D(miniSG::Texture2D *msgTex)
  {
    if(msgTex == NULL) {
      static int numWarnings = 0;
      if (++numWarnings < 10)
        cout << "WARNING: material does not have Textures (only warning for the first 10 times)!" << endl;
      return NULL;
    }

    static std::map<miniSG::Texture2D*, OSPTexture2D> alreadyCreatedTextures;
    if (alreadyCreatedTextures.find(msgTex) != alreadyCreatedTextures.end())
      return alreadyCreatedTextures[msgTex];

    //TODO: We need to come up with a better way to handle different possible pixel layouts
    OSPTextureFormat type = OSP_TEXTURE_R8;

    if (msgTex->depth == 1) {
      if( msgTex->channels == 1 ) type = OSP_TEXTURE_R8;
      if( msgTex->channels == 3 )
        type = msgTex->prefereLinear ? OSP_TEXTURE_RGB8 : OSP_TEXTURE_SRGB;
      if( msgTex->channels == 4 )
        type = msgTex->prefereLinear ? OSP_TEXTURE_RGBA8 : OSP_TEXTURE_SRGBA;
    } else if (msgTex->depth == 4) {
      if( msgTex->channels == 1 ) type = OSP_TEXTURE_R32F;
      if( msgTex->channels == 3 ) type = OSP_TEXTURE_RGB32F;
      if( msgTex->channels == 4 ) type = OSP_TEXTURE_RGBA32F;
    }

    vec2i texSize(msgTex->width, msgTex->height);
    OSPTexture2D ospTex = ospNewTexture2D((osp::vec2i&)texSize,
                                          type,
                                          msgTex->data,
                                          0);

    alreadyCreatedTextures[msgTex] = ospTex;

    ospCommit(ospTex);

    return ospTex;
  }

  OSPMaterial createMaterial(OSPRenderer renderer,
                             miniSG::Material *mat)
  {
    if (mat == NULL) {
      static int numWarnings = 0;
      if (++numWarnings < 10)
        cout << "WARNING: model does not have materials! (assigning default)" << endl;
      return createDefaultMaterial(renderer);
    }
    static std::map<miniSG::Material *,OSPMaterial> alreadyCreatedMaterials;
    if (alreadyCreatedMaterials.find(mat) != alreadyCreatedMaterials.end())
      return alreadyCreatedMaterials[mat];

    const char *type = mat->getParam("type","OBJMaterial");
    assert(type);
    OSPMaterial ospMat = alreadyCreatedMaterials[mat] = ospNewMaterial(renderer,type);
    if (!ospMat)  {
      warnMaterial(type);
      return createDefaultMaterial(renderer);
    }
    const bool isOBJMaterial = !strcmp(type, "OBJMaterial");

    for (miniSG::Material::ParamMap::const_iterator it =  mat->params.begin();
         it !=  mat->params.end(); ++it) {
      const char *name = it->first.c_str();
      const miniSG::Material::Param *p = it->second.ptr;

      switch(p->type) {
      case miniSG::Material::Param::INT:
        ospSet1i(ospMat,name,p->i[0]);
        break;
      case miniSG::Material::Param::FLOAT: {
        float f = p->f[0];
        /* many mtl materials of obj models wrongly store the phong exponent
           'Ns' in range [0..1], whereas OSPRay's material implementations
           correctly interpret it to be in [0..inf), thus we map ranges here */
        if (isOBJMaterial && (!strcmp(name, "Ns") || !strcmp(name, "ns")) && f < 1.f)
          f = 1.f/(1.f - f) - 1.f;
        ospSet1f(ospMat,name,f);
      } break;
      case miniSG::Material::Param::FLOAT_3:
        ospSet3fv(ospMat,name,p->f);
        break;
      case miniSG::Material::Param::STRING:
        ospSetString(ospMat,name,p->s);
        break;
      case miniSG::Material::Param::TEXTURE:
        {
          miniSG::Texture2D *tex = (miniSG::Texture2D*)p->ptr;
          if (tex) {
            OSPTexture2D ospTex = createTexture2D(tex);
            assert(ospTex);
            ospCommit(ospTex);
            ospSetObject(ospMat, name, ospTex);
          }
          break;
        }
      default:
        throw std::runtime_error("unknown material parameter type");
      };
    }

    ospCommit(ospMat);
    return ospMat;
  }

  void ospModelViewerMain(int &ac, const char **&av)
  {
    msgModel = new miniSG::Model;

    cout << "#ospModelViewer: starting to process cmdline arguments" << endl;
    for (int i=1;i<ac;i++) {
      const std::string arg = av[i];
      if (arg == "--renderer") {
        assert(i+1 < ac);
        rendererType = av[++i];
      } else if (arg == "--always-redraw" || arg == "-fps") {
        alwaysRedraw = true;
      } else if (arg == "-o") {
        outFileName = strdup(av[++i]);
      } else if (arg == "-o:nacc") {
        numAccumsFrameInFileOutput = atoi(av[++i]);
      } else if (arg == "-o:spp") {
        numSPPinFileOutput = atoi(av[++i]);
      } else if (arg == "--max-objects") {
        maxObjectsToConsider = atoi(av[++i]);
      } else if (arg == "--spp" || arg == "-spp") {
        spp = atoi(av[++i]);
      } else if (arg == "--force-instancing") {
        forceInstancing = true;
      } else if (arg == "--pt") {
        // shortcut for '--renderer pathtracer'
        maxAccum = 1024;
        rendererType = "pathtracer";
      } else if (arg == "--sun-dir") {
        if (!strcmp(av[i+1],"none")) {
          defaultDirLight_direction = vec3f(0.f);
        } else {
          defaultDirLight_direction.x = atof(av[++i]);
          defaultDirLight_direction.y = atof(av[++i]);
          defaultDirLight_direction.z = atof(av[++i]);
        }
      } else if (arg == "--module" || arg == "--plugin") {
        assert(i+1 < ac);
        const char *moduleName = av[++i];
        cout << "loading ospray module '" << moduleName << "'" << endl;
        ospLoadModule(moduleName);
      } else if (arg == "--alpha") {
        g_alpha = true;
      } else if (arg == "--display-wall") {
        displayWall = new DisplayWall;
        displayWall->hostname = av[++i];
        displayWall->streamName = av[++i];
        displayWall->size.x = atof(av[++i]);
        displayWall->size.y = atof(av[++i]);
      } else if (arg == "-bench") {
        if (++i < ac)
          {
            std::string arg2(av[i]);
            size_t pos = arg2.find("x");
            if (pos != std::string::npos)
              {
                arg2.replace(pos, 1, " ");
                std::stringstream ss(arg2);
                ss >> g_benchWarmup >> g_benchFrames;
              }
          }
      } else if (arg == "--no-default-material") {
        g_createDefaultMaterial = false;
      } else if (av[i][0] == '-') {
        error("unknown commandline argument '"+arg+"'");
      } else {
        FileName fn = arg;
        if (fn.ext() == "stl") {
          miniSG::importSTL(*msgModel,fn);
        } else if (fn.ext() == "msg") {
          miniSG::importMSG(*msgModel,fn);
        } else if (fn.ext() == "tri") {
          miniSG::importTRI(*msgModel,fn);
        } else if (fn.ext() == "xml") {
          miniSG::importRIVL(*msgModel,fn);
        } else if (fn.ext() == "obj") {
          miniSG::importOBJ(*msgModel,fn);
        } else if (fn.ext() == "hbp") {
          miniSG::importHBP(*msgModel,fn);
        } else if (fn.ext() == "x3d") {
          miniSG::importX3D(*msgModel,fn);
        } else if (fn.ext() == "astl") {
          miniSG::importSTL(msgAnimation,fn);
        }
      }
    }

    ospLoadModule("multihit");

    // -------------------------------------------------------
    // done parsing
    // -------------------------------------------------------]
    cout << "#ospModelViewer: done parsing. found model with" << endl;
    cout << "  - num meshes   : " << msgModel->mesh.size() << " ";
    size_t numUniqueTris = 0;
    size_t numInstancedTris = 0;
    for (size_t  i=0;i<msgModel->mesh.size();i++) {
      if (i < 10)
        cout << "[" << msgModel->mesh[i]->size() << "]";
      else
        if (i == 10) cout << "...";
      numUniqueTris += msgModel->mesh[i]->size();
    }
    cout << endl;
    cout << "  - num instances: " << msgModel->instance.size() << " ";
    for (size_t  i=0;i<msgModel->instance.size();i++) {
      if (i < 10)
        cout << "[" << msgModel->mesh[msgModel->instance[i].meshID]->size() << "]";
      else
        if (i == 10) cout << "...";
      numInstancedTris += msgModel->mesh[msgModel->instance[i].meshID]->size();
    }
    cout << endl;
    cout << "  - num unique triangles   : " << numUniqueTris << endl;
    cout << "  - num instanced triangles: " << numInstancedTris << endl;

    if (numInstancedTris == 0 && msgAnimation.empty())
      error("no (valid) input files specified - model contains no triangles");

    // -------------------------------------------------------
    // create ospray model
    // -------------------------------------------------------
    ospModel = ospNewModel();

    ospRenderer = ospNewRenderer(rendererType.c_str());
    if (!ospRenderer)
      throw std::runtime_error("could not create ospRenderer '"+rendererType+"'");
    Assert(ospRenderer != NULL && "could not create ospRenderer");
    ospCommit(ospRenderer);

    // code does not yet do instancing ... check that the model doesn't contain instances
    bool doesInstancing = 0;

    if (forceInstancing) {
      std::cout << "#ospModelViewer: forced instancing - instances on." << std::endl;
      doesInstancing = true;
    } else if (msgModel->instance.size() > msgModel->mesh.size()) {
      std::cout << "#ospModelViewer: found more object instances than meshes - turning on instancing" << std::endl;
      doesInstancing = true;
    } else {
      std::cout << "#ospModelViewer: number of instances matches number of meshes, creating single model that contains all meshes" << std::endl;
      doesInstancing = false;
    }
    if (doesInstancing) {
      if (msgModel->instance.size() > maxObjectsToConsider) {
        cout << "cutting down on the number of meshes as requested on cmdline..." << endl;
        msgModel->instance.resize(maxObjectsToConsider);
      }
    } else {
      if (msgModel->instance.size() > maxObjectsToConsider) {
        cout << "cutting down on the number of meshes as requested on cmdline..." << endl;
        msgModel->instance.resize(maxObjectsToConsider);
        msgModel->mesh.resize(maxObjectsToConsider);
      }
    }


    cout << "#ospModelViewer: adding parsed geometries to ospray model" << endl;
    std::vector<OSPModel> instanceModels;

    for (size_t i=0;i<msgModel->mesh.size();i++) {
      printf("Mesh %li/%li\n",i,msgModel->mesh.size());
      Ref<miniSG::Mesh> msgMesh = msgModel->mesh[i];
      // DBG(PRINT(msgMesh.ptr));

      // create ospray mesh
      OSPGeometry ospMesh = ospNewGeometry("mhtriangles");

      // check if we have to transform the vertices:
      if (doesInstancing == false && msgModel->instance[i] != miniSG::Instance(i)) {
        // cout << "Transforming vertex array ..." << endl;
        // PRINT(msgMesh->position.size());
        for (size_t vID=0;vID<msgMesh->position.size();vID++) {
          msgMesh->position[vID] = xfmPoint(msgModel->instance[i].xfm,
                                            msgMesh->position[vID]);
        }
      }

      // add position array to mesh
      OSPData position = ospNewData(msgMesh->position.size(),OSP_FLOAT3A,
                                    &msgMesh->position[0],OSP_DATA_SHARED_BUFFER);
      ospSetData(ospMesh,"position",position);

      // add triangle index array to mesh
      if (!msgMesh->triangleMaterialId.empty()) {
        OSPData primMatID = ospNewData(msgMesh->triangleMaterialId.size(),OSP_INT,
                                       &msgMesh->triangleMaterialId[0],OSP_DATA_SHARED_BUFFER);
        ospSetData(ospMesh,"prim.materialID",primMatID);
      }

      // add triangle index array to mesh
      OSPData index = ospNewData(msgMesh->triangle.size(),OSP_INT3,
                                 &msgMesh->triangle[0],OSP_DATA_SHARED_BUFFER);
      assert(msgMesh->triangle.size() > 0);
      ospSetData(ospMesh,"index",index);

      // add normal array to mesh
      if (!msgMesh->normal.empty()) {
        OSPData normal = ospNewData(msgMesh->normal.size(),OSP_FLOAT3A,
                                    &msgMesh->normal[0],OSP_DATA_SHARED_BUFFER);
        assert(msgMesh->normal.size() > 0);
        ospSetData(ospMesh,"vertex.normal",normal);
      } else {
        // cout << "no vertex normals!" << endl;
      }

      // add color array to mesh
      if (!msgMesh->color.empty()) {
        OSPData color = ospNewData(msgMesh->color.size(),OSP_FLOAT3A,
                                   &msgMesh->color[0],OSP_DATA_SHARED_BUFFER);
        assert(msgMesh->color.size() > 0);
        ospSetData(ospMesh,"vertex.color",color);
      } else {
        // cout << "no vertex colors!" << endl;
      }

      // add texcoord array to mesh
      if (!msgMesh->texcoord.empty()) {
        OSPData texcoord = ospNewData(msgMesh->texcoord.size(), OSP_FLOAT2,
                                      &msgMesh->texcoord[0], OSP_DATA_SHARED_BUFFER);
        assert(msgMesh->texcoord.size() > 0);
        ospSetData(ospMesh,"vertex.texcoord",texcoord);
      }

      ospSet1i(ospMesh, "alpha_type", 0);
      ospSet1i(ospMesh, "alpha_component", 4);

      // add triangle material id array to mesh
      if (msgMesh->materialList.empty()) {
        // we have a single material for this mesh...
        OSPMaterial singleMaterial = createMaterial(ospRenderer, msgMesh->material.ptr);
        ospSetMaterial(ospMesh,singleMaterial);
      } else {
        // we have an entire material list, assign that list
        std::vector<OSPMaterial > materialList;
        std::vector<OSPTexture2D > alphaMaps;
        std::vector<float> alphas;
        for (int i=0;i<msgMesh->materialList.size();i++) {
          materialList.push_back(createMaterial(ospRenderer, msgMesh->materialList[i].ptr));

          for (miniSG::Material::ParamMap::const_iterator it =  msgMesh->materialList[i]->params.begin();
               it != msgMesh->materialList[i]->params.end(); it++) {
            const char *name = it->first.c_str();
            const miniSG::Material::Param *p = it->second.ptr;
            if(p->type == miniSG::Material::Param::TEXTURE) {
              if(!strcmp(name, "map_kd") || !strcmp(name, "map_Kd")) {
                miniSG::Texture2D *tex = (miniSG::Texture2D*)p->ptr;
                OSPTexture2D ospTex = createTexture2D(tex);
                ospCommit(ospTex);
                alphaMaps.push_back(ospTex);
              }
            } else if(p->type == miniSG::Material::Param::FLOAT) {
              if(!strcmp(name, "d")) alphas.push_back(p->f[0]);
            }
          }

          while(materialList.size() > alphaMaps.size()) {
            alphaMaps.push_back(NULL);
          }
          while(materialList.size() > alphas.size()) {
            alphas.push_back(0.f);
          }
        }
        OSPData ospMaterialList = ospNewData(materialList.size(), OSP_OBJECT, &materialList[0], 0);
        ospSetData(ospMesh,"materialList",ospMaterialList);

        // only set these if alpha aware mode enabled
        // this currently doesn't work on the MICs!
        if(g_alpha) {
          OSPData ospAlphaMapList = ospNewData(alphaMaps.size(), OSP_OBJECT, &alphaMaps[0], 0);
          ospSetData(ospMesh, "alpha_maps", ospAlphaMapList);

          OSPData ospAlphaList = ospNewData(alphas.size(), OSP_OBJECT, &alphas[0], 0);
          ospSetData(ospMesh, "alphas", ospAlphaList);
        }
      }

      ospCommit(ospMesh);

      if (doesInstancing) {
        OSPModel model_i = ospNewModel();
        ospAddGeometry(model_i,ospMesh);
        ospCommit(model_i);
        instanceModels.push_back(model_i);
      } else
        ospAddGeometry(ospModel,ospMesh);

    }
    if (doesInstancing) {
      for (int i=0;i<msgModel->instance.size();i++) {
        OSPGeometry inst = ospNewInstance(instanceModels[msgModel->instance[i].meshID],
                                          (const osp::affine3f&) msgModel->instance[i].xfm);
        msgModel->instance[i].ospGeometry = inst;
        ospAddGeometry(ospModel,inst);
      }
    }
    cout << "#ospModelViewer: committing model" << endl;
    ospCommit(ospModel);
    cout << "#ospModelViewer: done creating ospray model." << endl;

    //TODO: Need to figure out where we're going to read lighting data from
    //begin light test
    std::vector<OSPLight> lights;
    if (defaultDirLight_direction != vec3f(0.f)) {
      cout << "#ospModelViewer: Adding a hard coded directional light as the sun." << endl;
      OSPLight ospLight = ospNewLight(ospRenderer, "DirectionalLight");
      ospSetString(ospLight, "name", "sun" );
      ospSet3f(ospLight, "color", 1, 1, 1);
      ospSet3fv(ospLight, "direction", &defaultDirLight_direction.x);
      ospSet1f(ospLight, "angularDiameter", 0.53f);
      ospCommit(ospLight);
      lights.push_back(ospLight);
    }
    OSPData lightArray = ospNewData(lights.size(), OSP_OBJECT, &lights[0], 0);
    ospSetData(ospRenderer, "lights", lightArray);
    //end light test
    ospCommit(ospRenderer);

    // -------------------------------------------------------
    // create viewer window
    // -------------------------------------------------------
    MSGViewer window(ospModel,ospRenderer);
    window.create("ospModelViewer: OSPRay Mini-Scene Graph test viewer");
    printf("#ospModelViewer: done creating window. Press 'Q' to quit.\n");
    const box3f worldBounds(msgModel->getBBox());
    window.setWorldBounds(worldBounds);
    std::cout << "#ospModelViewer: set world bounds " << worldBounds << ", motion speed " << window.motionSpeed << std::endl;
    if (msgModel->camera.size() > 0) {
      window.setViewPort(msgModel->camera[0]->from,
                         msgModel->camera[0]->at,
                         msgModel->camera[0]->up);
    }
    ospray::glut3D::runGLUT();
  }
}


int main(int ac, const char **av)
{
  ospInit(&ac,av);
  ospray::glut3D::initGLUT(&ac,av);
  ospray::ospModelViewerMain(ac,av);
}
