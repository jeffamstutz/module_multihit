## ======================================================================== ##
## Copyright 2015 SURVICE Engineering Company                               ##
##                                                                          ##
## Licensed under the Apache License, Version 2.0 (the "License");          ##
## you may not use this file except in compliance with the License.         ##
## You may obtain a copy of the License at                                  ##
##                                                                          ##
##     http://www.apache.org/licenses/LICENSE-2.0                           ##
##                                                                          ##
## Unless required by applicable law or agreed to in writing, software      ##
## distributed under the License is distributed on an "AS IS" BASIS,        ##
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. ##
## See the License for the specific language governing permissions and      ##
## limitations under the License.                                           ##
## ======================================================================== ##

option(OSPRAY_MODULE_MULTIHIT "Build 'multihit' module" ON)

if (OSPRAY_MODULE_MULTIHIT)

  include_directories(${CMAKE_CURRENT_SOURCE_DIR})

  include_directories_ispc(${CMAKE_CURRENT_SOURCE_DIR})

  ospray_create_library(module_multihit
    MultiHitRenderer.cpp
    MultiHitRenderer.ispc
    MultiHitTriangles.cpp
    MultiHitTriangles.ispc
    LINK
    ospray
    ospray_common
  )

  ospray_create_application(MultiHitViewer
    multihitViewer.cpp
    LINK
    ospray
    ospray_commandline
    ospray_common
    ospray_minisg
    ospray_glut3d
    ${OPENGL_LIBRARIES}
    ${GLUT_LIBRARIES}
  )

endif()
