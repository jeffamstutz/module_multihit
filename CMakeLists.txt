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

cmake_minimum_required(VERSION 2.8)

project(multihit_module)

find_package(ospray REQUIRED)
include(${OSPRAY_USE_FILE})

ospray_configure_ispc_isa()

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${OSPRAY_INCLUDE_DIRS}
)

include_directories_ispc(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${OSPRAY_INCLUDE_DIRS}
)

ospray_create_library(module_multihit
  MultiHitRenderer.cpp
  MultiHitRenderer.ispc
  MultiHitTriangles.cpp
  MultiHitTriangles.ispc
  LINK
  ${OSPRAY_LIBRARIES}
)

#ospray_create_application(MultiHitViewer
#  multihitViewer.cpp
#  LINK
#  ${OSPRAY_LIBRARIES}
#  ${OPENGL_LIBRARIES}
#  ${GLUT_LIBRARIES}
#)

