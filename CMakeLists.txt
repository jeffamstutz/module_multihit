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

  configure_ospray()

#  set(OSPRAY_MODULE_MHTK_SORTING ON CACHE BOOL "Sort MHTK intersections")

#  if (OSPRAY_MODULE_MHTK_SORTING)
#    add_definitions_ispc(-DMHTK_SORTING)
#  endif()

  include_directories(
    ${PROJECT_SOURCE_DIR}/ospray
    ${PROJECT_SOURCE_DIR}/ospray/geometry
    ${PROJECT_SOURCE_DIR}/ospray/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${RAYFORCE_INCLUDE_DIR}
  )

  include_directories_ispc(
    ${PROJECT_SOURCE_DIR}/ospray
    ${PROJECT_SOURCE_DIR}/ospray/geometry
    ${PROJECT_SOURCE_DIR}/ospray/include
    ${CMAKE_CURRENT_SOURCE_DIR}
  )

  ospray_add_library(ospray_module_mhtk${OSPRAY_LIB_SUFFIX} SHARED
    MultiHitRenderer.cpp
    MultiHitRenderer.ispc
    MultiHitTriangles.cpp
    MultiHitTriangles.ispc
  )

  target_link_libraries(ospray_module_mhtk${OSPRAY_LIB_SUFFIX}
    ospray${OSPRAY_LIB_SUFFIX}
  )

endif()
