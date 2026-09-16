#############################################################
# sources

set(QGIS_3D_SRCS
  qgs3d.cpp
  qgs3daxis.cpp
  qgs3daxissettings.cpp
  qgsaabb.cpp
  qgsabstract3dengine.cpp
  qgsabstractfeaturebasedchunkedentity.cpp
  qgsabstractvectorlayer3drenderer.cpp
  qgsannotationlayer3drenderer.cpp
  qgsannotationlayerchunkloader_p.cpp
  qgs3danimationsettings.cpp
  qgs3dexportobject.cpp
  qgs3dhighlightfeaturehandler.cpp
  qgs3dmapexportsettings.cpp
  qgs3dmapscene.cpp
  qgs3dmapsettings.cpp
  qgs3dmaptool.cpp
  qgs3dmapcanvas.cpp
  qgscolorgradingsettings.cpp
  qgscrosssection.cpp
  qgs3drendercontext.cpp
  qgs3dsceneexporter.cpp
  qgs3dutils.cpp
  qgs3dwiredmesh_p.cpp
  qgsbloomsettings.cpp
  qgscameracontroller.cpp
  qgscamerapose.cpp
  qgscategorized3drenderer.cpp
  qgscategorizedchunkloader_p.cpp
  qgsenvironmentlight.cpp
  qgsfeature3dhandler_p.cpp
  qgsfonttextureatlasgenerator.cpp
  qgsgeotransform.cpp
  qgsglobechunkedentity.cpp
  qgsglobetilegeometry3d.cpp
  qgsgltf3dutils.cpp
  qgsgltfgeometry3d.cpp
  qgsimagetexture.cpp
  qgslayoutitem3dmap.cpp
  qgslayerstylewatcher.cpp
  qgsmapoverlayentity.cpp
  qgsmapoverlaytexturegenerator_p.cpp
  qgsobj3dutils.cpp
  qgsobjgeometry3d.cpp
  qgsoffscreen3dengine.cpp
  qgsraycastingutils.cpp
  qgsraycastcontext.cpp
  qgsraycasthit.cpp
  qgsraycastresult.cpp
  qgsrulebased3drenderer.cpp
  qgsrulebasedchunkloader_p.cpp
  qgstessellatedpolygongeometry.cpp
  qgstiledscenechunkloader_p.cpp
  qgstiledscenelayer3drenderer.cpp
  qgstilingscheme.cpp
  qgsvectorlayer3drenderer.cpp
  qgsvectorlayerchunkloader_p.cpp
  qgsmeshlayer3drenderer.cpp
  qgswindow3dengine.cpp
  qgsfixedgradientbackgroundsettings.cpp
  qgsgradientbackgroundentity.cpp
  qgsskyboxentity.cpp
  qgsskyboxsettings.cpp
  qgsshadowsettings.cpp
  qgscolorramptexture.cpp
  qgsrubberband3d.cpp
  qgsambientocclusionsettings.cpp
  qgstextureatlasgenerator.cpp

  qgspointcloudlayer3drenderer.cpp
  qgspointcloudlayerchunkloader_p.cpp
  qgsvirtualpointcloudentity_p.cpp

  chunks/qgschunkboundsentity_p.cpp
  chunks/qgschunkedentity.cpp
  chunks/qgschunklist_p.cpp
  chunks/qgschunkloader.cpp
  chunks/qgschunknode.cpp
  chunks/qgschunkqueuejob.cpp

  lights/qgsdirectionallightsettings.cpp
  lights/qgslightsource.cpp
  lights/qgspointlightsettings.cpp
  lights/qgssunlightsettings.cpp

  materials/qgsglobematerial.cpp
  materials/qgsgoochmaterial.cpp
  materials/qgsgoochmaterial3dhandler.cpp
  materials/qgsmaterial.cpp
  materials/qgsmaterial3dhandler.cpp
  materials/qgsmetalroughmaterial.cpp
  materials/qgsmetalroughmaterial3dhandler.cpp
  materials/qgsmetalroughtexturedmaterial3dhandler.cpp
  materials/qgsnullmaterial3dhandler.cpp
  materials/qgsphongmaterial.cpp
  materials/qgsphongmaterial3dhandler.cpp
  materials/qgsphongtexturedmaterial.cpp
  materials/qgsphongtexturedmaterial3dhandler.cpp
  materials/qgssimplelinematerial3dhandler.cpp
  materials/qgstexturematerial.cpp
  materials/qgsunlitmaterial.cpp
  materials/qgsunlitmaterial3dhandler.cpp

  processing/qgs3dalgorithms.cpp
  processing/qgsalgorithmtessellate.cpp

  framegraph/qgsabstractrenderview.cpp
  framegraph/qgs3daxisrenderview.cpp
  framegraph/qgsambientocclusionrenderentity.cpp
  framegraph/qgsambientocclusionrenderview.cpp
  framegraph/qgsambientocclusionblurentity.cpp
  framegraph/qgsbloomdownsampleentity.cpp
  framegraph/qgsbloomrenderview.cpp
  framegraph/qgsbloomupsampleentity.cpp
  framegraph/qgsdepthentity.cpp
  framegraph/qgsdepthrenderview.cpp
  framegraph/qgsforwardrenderview.cpp
  framegraph/qgshighlightsrenderview.cpp
  framegraph/qgsoverlaytextureentity.cpp
  framegraph/qgsoverlaytexturerenderview.cpp
  framegraph/qgspostprocessingentity.cpp
  framegraph/qgsrenderpassquad.cpp
  framegraph/qgsrubberbandrenderview.cpp
  framegraph/qgspostprocessingrenderview.cpp
  framegraph/qgsshadowrenderview.cpp
  framegraph/qgsframegraph.cpp
  framegraph/qgsframegraphutils.cpp

  symbols/qgs3dsymbolutils.cpp
  symbols/qgsbillboardgeometry.cpp
  symbols/qgsbillboardgeometry3d.cpp
  symbols/qgspointcloudgeometry3d.cpp
  symbols/qgsline3dsymbol.cpp
  symbols/qgsline3dsymbol_p.cpp
  symbols/qgslinematerial_p.cpp
  symbols/qgslinevertexdata_p.cpp
  symbols/qgsmesh3dsymbol.cpp
  symbols/qgspoint3dbillboardmaterial.cpp
  symbols/qgspoint3dsymbol.cpp
  symbols/qgspoint3dsymbol_p.cpp
  symbols/qgspolygon3dsymbol.cpp
  symbols/qgspolygon3dsymbol_p.cpp
  symbols/qgspointcloud3dsymbol.cpp
  symbols/qgspointcloud3dsymbol_p.cpp

  terrain/qgs3dterrainregistry.cpp
  terrain/qgsabstractterrainsettings.cpp
  terrain/qgsdemterraingenerator.cpp
  terrain/qgsdemterrainsettings.cpp
  terrain/qgsdemterraintilegeometry_p.cpp
  terrain/qgsdemterraintilegeometry3d.cpp
  terrain/qgsdemterraintileloader_p.cpp
  terrain/qgsflatterraingenerator.cpp
  terrain/qgsflatterrainsettings.cpp
  terrain/qgsmeshterrainsettings.cpp
  terrain/qgsonlineterraingenerator.cpp
  terrain/qgsonlinedemterrainsettings.cpp
  terrain/qgsquantizedmeshterraingenerator.cpp
  terrain/qgsquantizedmeshterrainsettings.cpp
  terrain/qgsterraindownloader.cpp
  terrain/qgsterrainentity.cpp
  terrain/qgsterraingenerator.cpp
  terrain/qgsterraintexturegenerator_p.cpp
  terrain/qgsterraintextureimage_p.cpp
  terrain/qgsterraintileloader.cpp

  mesh/qgsmesh3dgeometry_p.cpp
  mesh/qgsmeshgeometry3d.cpp
  mesh/qgsmesh3dentity_p.cpp
  mesh/qgsmesh3dmaterial_p.cpp
  mesh/qgsmeshterraingenerator.cpp

  engine/qgscamera3d.cpp
  engine/qgsgeometry3d.cpp
  engine/qgsgpubuffer.cpp
  engine/qgsgpurendertarget.cpp
  engine/qgsgputexture.cpp
  engine/qgsgraphicspipelinecache.cpp
  engine/qgsmaterial3d.cpp
  engine/qgsrenderpass.cpp
  engine/qgsrenderuniforms.cpp
  engine/qgsrhiengine.cpp
  engine/qgsrhishader.cpp
  engine/qgsoverlayquads.cpp
  engine/qgsscenerenderer.cpp
  engine/qgssceneobject3d.cpp
  engine/qgsscreenquad.cpp
  engine/qgstessellatedpolygongeometry3d.cpp
  engine/qgsuniformring.cpp

  engine/materials/qgsambientocclusionmaterial3d.cpp
  engine/materials/qgsbackgroundmaterial3d.cpp
  engine/materials/qgsbillboardmaterial3d.cpp
  engine/materials/qgsbloommaterial3d.cpp
  engine/materials/qgsdepthrendermaterial3d.cpp
  engine/materials/qgsglobematerial3d.cpp
  engine/materials/qgshighlightmaterial3d.cpp
  engine/materials/qgsgoochmaterial3d.cpp
  engine/materials/qgsmaterial3dutils.cpp
  engine/materials/qgsmeshmaterial3d.cpp
  engine/materials/qgsmetalroughmaterial3d.cpp
  engine/materials/qgsoverlaytexturematerial3d.cpp
  engine/materials/qgsphongmaterial3d.cpp
  engine/materials/qgsphongtexturedmaterial3d.cpp
  engine/materials/qgspostprocessingmaterial3d.cpp
  engine/materials/qgsshadowcastermaterial3d.cpp
  engine/materials/qgstexturematerial3d.cpp
  engine/materials/qgsunlitmaterial3d.cpp

  engine/passes/qgsambientocclusionrenderpass.cpp
  engine/passes/qgsbloomrenderpass.cpp
  engine/passes/qgsdepthrenderpass.cpp
  engine/passes/qgsforwardrenderpass.cpp
  engine/passes/qgshighlightsrenderpass.cpp
  engine/passes/qgspostprocessingrenderpass.cpp
  engine/passes/qgsshadowrenderpass.cpp

  geometry/qgsprimitivegeometry3d.cpp
  geometry/qgswiredmeshgeometry3d.cpp
)

set(QGIS_3D_HDRS
  processing/qgs3dalgorithms.h
  qgs3d.h
  qgs3danimationsettings.h
  qgs3daxis.h
  qgs3daxissettings.h
  qgs3dhighlightfeaturehandler.h
  qgs3dmapscene.h
  qgs3dmapsettings.h
  qgs3dmaptool.h
  qgs3drendercontext.h
  qgs3dmapsceneentity.h
  qgs3dsceneexporter.h
  qgs3dtypes.h
  qgs3dutils.h
  qgs3dmapcanvas.h
  qgscolorgradingsettings.h
  qgscrosssection.h
  qgs3dwiredmesh_p.h
  qgsaabb.h
  qgsabstract3dengine.h
  qgsabstractfeaturebasedchunkedentity.h
  qgsabstractvectorlayer3drenderer.h
  qgsannotationlayer3drenderer.h
  qgsannotationlayerchunkloader_p.h
  qgsbloomsettings.h
  qgscameracontroller.h
  qgscamerapose.h
  qgscategorized3drenderer.h
  qgscategorizedchunkloader_p.h
  qgsenvironmentlight.h
  qgsfonttextureatlasgenerator.h
  qgsgeotransform.h
  qgsglobechunkedentity.h
  qgsglobetilegeometry3d.h
  qgsgltf3dutils.h
  qgsgltfgeometry3d.h
  qgslayoutitem3dmap.h
  qgslayerstylewatcher.h
  qgsmapoverlayentity.h
  qgsmapoverlaytexturegenerator_p.h
  qgsmeshlayer3drenderer.h
  qgsobj3dutils.h
  qgsobjgeometry3d.h
  qgsoffscreen3dengine.h
  qgsraycastingutils.h
  qgsraycastcontext.h
  qgsraycasthit.h
  qgsraycastresult.h
  qgsrulebased3drenderer.h
  qgstessellatedpolygongeometry.h
  qgstiledscenechunkloader_p.h
  qgstiledscenelayer3drenderer.h
  qgstilingscheme.h
  qgsvectorlayer3drenderer.h
  qgswindow3dengine.h
  qgsabstract3dmapbackgroundsettings.h
  qgsfixedgradientbackgroundsettings.h
  qgsgradientbackgroundentity.h
  qgsskyboxentity.h
  qgsskyboxsettings.h
  qgsshadowsettings.h
  qgspointcloudlayer3drenderer.h
  qgsambientocclusionsettings.h
  qgstextureatlasgenerator.h

  lights/qgsdirectionallightsettings.h
  lights/qgslightsource.h
  lights/qgspointlightsettings.h
  lights/qgssunlightsettings.h

  materials/qgsglobematerial.h
  materials/qgsgoochmaterial.h
  materials/qgsgoochmaterial3dhandler.h
  materials/qgsmaterial.h
  materials/qgsmaterial3dhandler.cpp
  materials/qgsmetalroughmaterial.h
  materials/qgsmetalroughmaterial3dhandler.h
  materials/qgsmetalroughtexturedmaterial3dhandler.h
  materials/qgsnullmaterial3dhandler.h
  materials/qgsphongmaterial.h
  materials/qgsphongmaterial3dhandler.h
  materials/qgsphongtexturedmaterial.h
  materials/qgsphongtexturedmaterial3dhandler.h
  materials/qgssimplelinematerial3dhandler.h
  materials/qgstexturematerial.h
  materials/qgsunlitmaterial.h
  materials/qgsunlitmaterial3dhandler.h

  framegraph/qgsabstractrenderview.h
  framegraph/qgs3daxisrenderview.h
  framegraph/qgsambientocclusionrenderentity.h
  framegraph/qgsambientocclusionrenderview.h
  framegraph/qgsambientocclusionblurentity.h
  framegraph/qgsbloomdownsampleentity.h
  framegraph/qgsbloomrenderview.h
  framegraph/qgsbloomupsampleentity.h
  framegraph/qgsdepthentity.h
  framegraph/qgsdepthrenderview.h
  framegraph/qgsforwardrenderview.h
  framegraph/qgsframegraph.h
  framegraph/qgsframegraphutils.h
  framegraph/qgshighlightsrenderview.h
  framegraph/qgsoverlaytextureentity.h
  framegraph/qgsoverlaytexturerenderview.h
  framegraph/qgspostprocessingentity.h
  framegraph/qgsrenderpassquad.h
  framegraph/qgspostprocessingrenderview.h
  framegraph/qgsrubberbandrenderview.h
  framegraph/qgsshadowrenderview.h

  symbols/qgs3dsymbolutils.h
  symbols/qgsbillboardgeometry.h
  symbols/qgsbillboardgeometry3d.h
  symbols/qgspointcloudgeometry3d.h
  symbols/qgsline3dsymbol.h
  symbols/qgsmesh3dsymbol.h
  symbols/qgspoint3dbillboardmaterial.h
  symbols/qgspoint3dsymbol.h
  symbols/qgspolygon3dsymbol.h
  symbols/qgspointcloud3dsymbol.h

  terrain/qgs3dterrainregistry.h
  terrain/qgsabstractterrainsettings.h
  terrain/qgsdemterraingenerator.h
  terrain/qgsdemterrainsettings.h
  terrain/qgsflatterraingenerator.h
  terrain/qgsflatterrainsettings.h
  terrain/qgsmeshterrainsettings.h
  terrain/qgsonlineterraingenerator.h
  terrain/qgsonlinedemterrainsettings.h
  terrain/qgsquantizedmeshterraingenerator.h
  terrain/qgsquantizedmeshterrainsettings.h
  terrain/qgsterraindownloader.h
  terrain/qgsterraingenerator.h
  terrain/qgsterraintileloader.h
  terrain/qgsterrainentity.h

  chunks/qgschunkloader.h
  chunks/qgschunkqueuejob.h
  chunks/qgschunknode.h
  chunks/qgschunkedentity.h

  mesh/qgsmeshterraingenerator.h

  engine/qgscamera3d.h
  engine/qgsgeometry3d.h
  engine/qgsgpubuffer.h
  engine/qgsgpurendertarget.h
  engine/qgsgputexture.h
  engine/qgsgraphicspipelinecache.h
  engine/qgsmaterial3d.h
  engine/qgsrendercommand.h
  engine/qgsrenderpass.h
  engine/qgsrenderuniforms.h
  engine/qgsrhiengine.h
  engine/qgsrhishader.h
  engine/qgsoverlayquads.h
  engine/qgsscenerenderer.h
  engine/qgssceneobject3d.h
  engine/qgsscreenquad.h
  engine/qgstessellatedpolygongeometry3d.h
  engine/qgsuniformring.h
  engine/qgsvertexattribute.h

  engine/materials/qgsambientocclusionmaterial3d.h
  engine/materials/qgsbackgroundmaterial3d.h
  engine/materials/qgsbillboardmaterial3d.h
  engine/materials/qgsbloommaterial3d.h
  engine/materials/qgsdepthrendermaterial3d.h
  engine/materials/qgsglobematerial3d.h
  engine/materials/qgshighlightmaterial3d.h
  engine/materials/qgsgoochmaterial3d.h
  engine/materials/qgsmaterial3dutils.h
  engine/materials/qgsmeshmaterial3d.h
  engine/materials/qgsmetalroughmaterial3d.h
  engine/materials/qgsoverlaytexturematerial3d.h
  engine/materials/qgsphongmaterial3d.h
  engine/materials/qgsphongtexturedmaterial3d.h
  engine/materials/qgspostprocessingmaterial3d.h
  engine/materials/qgsshadowcastermaterial3d.h
  engine/materials/qgstexturematerial3d.h
  engine/materials/qgsunlitmaterial3d.h

  engine/passes/qgsambientocclusionrenderpass.h
  engine/passes/qgsbloomrenderpass.h
  engine/passes/qgsdepthrenderpass.h
  engine/passes/qgsforwardrenderpass.h
  engine/passes/qgshighlightsrenderpass.h
  engine/passes/qgspostprocessingrenderpass.h
  engine/passes/qgsshadowrenderpass.h

  geometry/qgsprimitivegeometry3d.h
  geometry/qgswiredmeshgeometry3d.h
)

set(QGIS_3D_PRIVATE_HDRS
  qgsrulebasedchunkloader_p.h
  qgsvectorlayerchunkloader_p.h
  qgspointcloudlayerchunkloader_p.h
  qgsvirtualpointcloudentity_p.h
  chunks/qgschunkboundsentity_p.h
  chunks/qgschunklist_p.h
  symbols/qgsline3dsymbol_p.h
  symbols/qgslinematerial_p.h
  symbols/qgslinevertexdata_p.h
  symbols/qgspoint3dsymbol_p.h
  symbols/qgspolygon3dsymbol_p.h
  symbols/qgspointcloud3dsymbol_p.h
  terrain/qgsdemterraintilegeometry_p.h
  terrain/qgsdemterraintilegeometry3d.h
  terrain/qgsdemterraintileloader_p.h
  terrain/qgsterraintexturegenerator_p.h
  terrain/qgsterraintextureimage_p.h
  terrain/qgsterraintileentity_p.h
  mesh/qgsmesh3dentity_p.h
  mesh/qgsmesh3dgeometry_p.h
  mesh/qgsmeshgeometry3d.h
  mesh/qgsmesh3dmaterial_p.h
  mesh/qgsmeshterraintileloader_p.h
  qgscolorramptexture.h
)

set (QGIS_3D_RCCS  shaders.qrc  ../../resources/3d/textures/textures.qrc)


#############################################################
# qgis_3d library


add_library(qgis_3d SHARED ${QGIS_3D_SRCS} ${QGIS_3D_HDRS} ${QGIS_3D_RCCS} ${QGIS_3D_PRIVATE_HDRS})
if(QGISPOSTFIX)
  set_target_properties(qgis_3d PROPERTIES
    RUNTIME_OUTPUT_NAME qgis_3d${QGISPOSTFIX}
    LIBRARY_OUTPUT_NAME qgis_3d${QGISPOSTFIX}
  )
endif()

#############################################################
# RHI shaders
#
# Shaders are written once as Vulkan style GLSL and baked by qsb at build time into
# SPIR-V, GLSL, HLSL and MSL, all inside a single .qsb file per shader. Nothing is
# translated at runtime, so there is no per-run cost and no shader cache to keep.
#
# Every variant is baked here rather than picked at runtime: the set of defines QGIS uses
# is small and fully known, and baking them all is what lets a missing variant be a build
# error instead of a scene that silently fails to draw.

# Every call below bakes the same source through the same qsb settings, so this wraps up
# everything they share and leaves only the file, the defines and the baked name.
function(qgis_add_rhi_shader name)
  cmake_parse_arguments(ARG "" "FILE;OUTPUT" "DEFINES" ${ARGN})

  set(_defines "")
  if(ARG_DEFINES)
    set(_defines DEFINES ${ARG_DEFINES})
  endif()

  set(_outputs "")
  if(ARG_OUTPUT)
    set(_outputs OUTPUTS ${ARG_OUTPUT})
  endif()

  qt6_add_shaders(qgis_3d "qgis_3d_rhi_${name}"
    SILENT
    PREFIX "/shaders/rhi"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/shaders/rhi"
    GLSL "120,150,330"
    HLSL 50
    MSL 12
    ${_defines}
    FILES shaders/rhi/${ARG_FILE}
    ${_outputs}
  )
endfunction()

# Bakes the four instancing variants of a vertex shader: with and without a per-instance
# scale, with and without a per-instance rotation. A material picks one of them when it is
# constructed, and which attributes the shader reads is what differs, so these are
# genuinely different shaders rather than a flag. All four always exist, so a material can
# never ask for a combination that was not baked.
function(qgis_add_rhi_instanced_shaders base)
  cmake_parse_arguments(ARG "" "" "DEFINES" ${ARGN})

  qgis_add_rhi_shader(${base}_instanced
    FILE ${base}.vert DEFINES INSTANCING=1 ${ARG_DEFINES}
    OUTPUT ${base}_instanced.vert.qsb)
  qgis_add_rhi_shader(${base}_instanced_scale
    FILE ${base}.vert DEFINES INSTANCING=1 USE_INSTANCE_SCALE=1 ${ARG_DEFINES}
    OUTPUT ${base}_instanced_scale.vert.qsb)
  qgis_add_rhi_shader(${base}_instanced_rotation
    FILE ${base}.vert DEFINES INSTANCING=1 USE_INSTANCE_ROTATION=1 ${ARG_DEFINES}
    OUTPUT ${base}_instanced_rotation.vert.qsb)
  qgis_add_rhi_shader(${base}_instanced_scale_rotation
    FILE ${base}.vert DEFINES INSTANCING=1 USE_INSTANCE_SCALE=1 USE_INSTANCE_ROTATION=1 ${ARG_DEFINES}
    OUTPUT ${base}_instanced_scale_rotation.vert.qsb)
endfunction()

# QgsUnlitMaterial3D. The fragment stage has no variants at all: which of the uniform, the
# per-vertex and the data defined colour applies is resolved in the vertex stage.
qgis_add_rhi_shader(unlit_vert FILE unlit.vert)
qgis_add_rhi_shader(unlit_frag FILE unlit.frag)
qgis_add_rhi_shader(unlit_datadefined_vert
  FILE unlit.vert DEFINES DATA_DEFINED=1
  OUTPUT unlit_datadefined.vert.qsb)
qgis_add_rhi_shader(unlit_vertexcolor_vert
  FILE unlit.vert DEFINES VERTEX_COLOR=1
  OUTPUT unlit_vertexcolor.vert.qsb)
qgis_add_rhi_instanced_shaders(unlit)

# QgsTextureMaterial3D
qgis_add_rhi_shader(textured_vert FILE textured.vert)
qgis_add_rhi_shader(textured_frag FILE textured.frag)
qgis_add_rhi_instanced_shaders(textured)

# QgsPhongMaterial3D. Only where the colour comes from differs in the fragment stage, so
# that has two variants where the vertex stage has six.
qgis_add_rhi_shader(phong_vert FILE phong.vert)
qgis_add_rhi_shader(phong_frag FILE phong.frag)
qgis_add_rhi_shader(phong_datadefined_vert
  FILE phong.vert DEFINES DATA_DEFINED=1
  OUTPUT phong_datadefined.vert.qsb)
qgis_add_rhi_shader(phong_datadefined_frag
  FILE phong.frag DEFINES DATA_DEFINED=1
  OUTPUT phong_datadefined.frag.qsb)
qgis_add_rhi_instanced_shaders(phong)

# QgsGoochMaterial3D
qgis_add_rhi_shader(gooch_vert FILE gooch.vert)
qgis_add_rhi_shader(gooch_frag FILE gooch.frag)
qgis_add_rhi_shader(gooch_datadefined_vert
  FILE gooch.vert DEFINES DATA_DEFINED=1
  OUTPUT gooch_datadefined.vert.qsb)
qgis_add_rhi_shader(gooch_datadefined_frag
  FILE gooch.frag DEFINES DATA_DEFINED=1
  OUTPUT gooch_datadefined.frag.qsb)
qgis_add_rhi_instanced_shaders(gooch)

# QgsPhongTexturedMaterial3D. Here the data defined variant is the one whose texture
# transform comes from a vertex attribute rather than from the material.
qgis_add_rhi_shader(phongtextured_vert FILE phongtextured.vert)
qgis_add_rhi_shader(phongtextured_frag FILE phongtextured.frag)
qgis_add_rhi_shader(phongtextured_datadefined_vert
  FILE phongtextured.vert DEFINES DATA_DEFINED_TEXTURE_TRANSFORMS=1
  OUTPUT phongtextured_datadefined.vert.qsb)
qgis_add_rhi_instanced_shaders(phongtextured)

# QgsBillboardMaterial3D. What the geometry supplies per instance is baked, because those
# are vertex attributes; how the quad is sized and offset is a run time flag.
qgis_add_rhi_shader(billboard_vert FILE billboard.vert)
qgis_add_rhi_shader(billboard_frag FILE billboard.frag)
qgis_add_rhi_shader(billboard_size_vert
  FILE billboard.vert DEFINES PER_INSTANCE_SIZE=1
  OUTPUT billboard_size.vert.qsb)
qgis_add_rhi_shader(billboard_atlas_vert
  FILE billboard.vert DEFINES TEXTURE_ATLAS=1
  OUTPUT billboard_atlas.vert.qsb)
qgis_add_rhi_shader(billboard_pixeloffset_vert
  FILE billboard.vert DEFINES TEXTURE_ATLAS_PIXEL_OFFSETS=1
  OUTPUT billboard_pixeloffset.vert.qsb)
qgis_add_rhi_shader(billboard_size_atlas_vert
  FILE billboard.vert DEFINES PER_INSTANCE_SIZE=1 TEXTURE_ATLAS=1
  OUTPUT billboard_size_atlas.vert.qsb)
qgis_add_rhi_shader(billboard_size_pixeloffset_vert
  FILE billboard.vert DEFINES PER_INSTANCE_SIZE=1 TEXTURE_ATLAS_PIXEL_OFFSETS=1
  OUTPUT billboard_size_pixeloffset.vert.qsb)
qgis_add_rhi_shader(billboard_atlas_pixeloffset_vert
  FILE billboard.vert DEFINES TEXTURE_ATLAS=1 TEXTURE_ATLAS_PIXEL_OFFSETS=1
  OUTPUT billboard_atlas_pixeloffset.vert.qsb)
qgis_add_rhi_shader(billboard_size_atlas_pixeloffset_vert
  FILE billboard.vert DEFINES PER_INSTANCE_SIZE=1 TEXTURE_ATLAS=1 TEXTURE_ATLAS_PIXEL_OFFSETS=1
  OUTPUT billboard_size_atlas_pixeloffset.vert.qsb)

# QgsGlobeMaterial3D
qgis_add_rhi_shader(globe_vert FILE globe.vert)
qgis_add_rhi_shader(globe_frag FILE globe.frag)

# QgsMeshMaterial3D. Whether the geometry carries a scalar magnitude is the one thing that
# has to be baked, because it is a vertex attribute rather than a value.
qgis_add_rhi_shader(mesh_vert FILE mesh.vert)
qgis_add_rhi_shader(mesh_frag FILE mesh.frag)
qgis_add_rhi_shader(mesh_scalar_vert
  FILE mesh.vert DEFINES HAS_SCALAR_MAGNITUDE=1
  OUTPUT mesh_scalar.vert.qsb)

# QgsMetalRoughMaterial3D. Its eleven feature #defines became run time flags, so what is
# left to bake is only what changes which vertex attributes are read - a shader input no
# attribute feeds makes the pipeline fail to build, so those cannot be flags.
qgis_add_rhi_shader(metalrough_vert FILE metalrough.vert)
qgis_add_rhi_shader(metalrough_frag FILE metalrough.frag)
qgis_add_rhi_shader(metalrough_texcoord_vert
  FILE metalrough.vert DEFINES HAS_TEXCOORD=1
  OUTPUT metalrough_texcoord.vert.qsb)
qgis_add_rhi_shader(metalrough_texcoord_tangent_vert
  FILE metalrough.vert DEFINES HAS_TEXCOORD=1 HAS_TANGENT=1
  OUTPUT metalrough_texcoord_tangent.vert.qsb)
qgis_add_rhi_shader(metalrough_datadefined_vert
  FILE metalrough.vert DEFINES DATA_DEFINED=1
  OUTPUT metalrough_datadefined.vert.qsb)
qgis_add_rhi_shader(metalrough_datadefined_frag
  FILE metalrough.frag DEFINES DATA_DEFINED=1
  OUTPUT metalrough_datadefined.frag.qsb)
qgis_add_rhi_shader(metalrough_ddtexture_vert
  FILE metalrough.vert DEFINES HAS_TEXCOORD=1 DATA_DEFINED_TEXTURE_TRANSFORMS=1
  OUTPUT metalrough_ddtexture.vert.qsb)
qgis_add_rhi_shader(metalrough_ddtexture_tangent_vert
  FILE metalrough.vert DEFINES HAS_TEXCOORD=1 HAS_TANGENT=1 DATA_DEFINED_TEXTURE_TRANSFORMS=1
  OUTPUT metalrough_ddtexture_tangent.vert.qsb)
qgis_add_rhi_instanced_shaders(metalrough DEFINES HAS_TEXCOORD=1 HAS_TANGENT=1)

# QgsShadowCasterMaterial3D. Position only, so the only thing to vary is instancing.
qgis_add_rhi_shader(shadowcaster_vert FILE shadowcaster.vert)
qgis_add_rhi_shader(shadowcaster_frag FILE shadowcaster.frag)
qgis_add_rhi_instanced_shaders(shadowcaster)

# QgsBackgroundMaterial3D, the gradient drawn behind the scene
qgis_add_rhi_shader(background_vert FILE background.vert)
qgis_add_rhi_shader(background_frag FILE background.frag)

# The full screen passes. They all share screen.vert, which needs no variants because its
# only input is a position already in clip space.
qgis_add_rhi_shader(screen_vert FILE screen.vert)
qgis_add_rhi_shader(depthrender_frag FILE depthrender.frag)
qgis_add_rhi_shader(ambientocclusion_vert FILE ambientocclusion.vert)
qgis_add_rhi_shader(ambientocclusion_frag FILE ambientocclusion.frag)
qgis_add_rhi_shader(ambientocclusionblur_frag FILE ambientocclusionblur.frag)
qgis_add_rhi_shader(bloomdownsample_frag FILE bloomdownsample.frag)
qgis_add_rhi_shader(bloomupsample_frag FILE bloomupsample.frag)
qgis_add_rhi_shader(postprocessing_frag FILE postprocessing.frag)
qgis_add_rhi_shader(overlaytexture_vert FILE overlaytexture.vert)
qgis_add_rhi_shader(overlaytexture_frag FILE overlaytexture.frag)

# QgsHighlightsRenderPass. The mask shares the shadow caster's vertex stages, because both
# want a vertex's position and nothing else.
qgis_add_rhi_shader(highlightmask_frag FILE highlightmask.frag)
qgis_add_rhi_shader(highlightoutline_frag FILE highlightoutline.frag)

if (USE_PRECOMPILED_HEADERS)
  target_precompile_headers(qgis_3d PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${CMAKE_SOURCE_DIR}/src/core/qgis.h>)
endif()

set_target_properties(qgis_3d PROPERTIES UNITY_BUILD ${ENABLE_UNITY_BUILDS})

target_include_directories(qgis_3d SYSTEM PUBLIC
  ${QT6_3DEXTRA_INCLUDE_DIR}
)

target_include_directories(qgis_3d PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/chunks
  ${CMAKE_CURRENT_SOURCE_DIR}/engine
  ${CMAKE_CURRENT_SOURCE_DIR}/engine/materials
  ${CMAKE_CURRENT_SOURCE_DIR}/engine/passes
  ${CMAKE_CURRENT_SOURCE_DIR}/geometry
  ${CMAKE_CURRENT_SOURCE_DIR}/framegraph
  ${CMAKE_CURRENT_SOURCE_DIR}/lights
  ${CMAKE_CURRENT_SOURCE_DIR}/materials
  ${CMAKE_CURRENT_SOURCE_DIR}/mesh
  ${CMAKE_CURRENT_SOURCE_DIR}/processing
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders
  ${CMAKE_CURRENT_SOURCE_DIR}/symbols
  ${CMAKE_CURRENT_SOURCE_DIR}/terrain

  ${CMAKE_BINARY_DIR}/src/3d
  ${CMAKE_SOURCE_DIR}/external/delaunator-cpp
  ${CMAKE_SOURCE_DIR}/external/tinygltf
  ${CMAKE_SOURCE_DIR}/external/rectpack2D
  ${CMAKE_SOURCE_DIR}/external/tinyobj
  ${CMAKE_SOURCE_DIR}/external/earcut
)

target_link_libraries(qgis_3d
  # QRhi lives in QtGui's private headers, which is also how QRhiWidget, Qt Quick
  # and Qt Quick 3D consume it
  ${QT_VERSION_BASE}::GuiPrivate
  ${QT_VERSION_BASE}::3DCore
  ${QT_VERSION_BASE}::3DRender
  ${QT_VERSION_BASE}::3DInput
  ${QT_VERSION_BASE}::3DLogic
  ${QT_VERSION_BASE}::3DExtras
)

GENERATE_EXPORT_HEADER(
   qgis_3d
   BASE_NAME 3D
   EXPORT_FILE_NAME qgis_3d.h
)

set(QGIS_3D_HDRS ${QGIS_3D_HDRS} ${CMAKE_CURRENT_BINARY_DIR}/qgis_3d.h)


#generate unversioned libs for android
if (NOT ANDROID)
  set_target_properties(qgis_3d PROPERTIES
    VERSION ${COMPLETE_VERSION}
    SOVERSION ${COMPLETE_VERSION}
    )
endif()

target_link_libraries(qgis_3d qgis_core)

# clang-tidy
if(CLANG_TIDY_EXE)
  set_target_properties(
    qgis_3d PROPERTIES
    CXX_CLANG_TIDY "${DO_CLANG_TIDY}"
  )
endif()

# install
if(NOT QGIS_MAC_BUNDLE)
  install(FILES ${QGIS_3D_HDRS} DESTINATION ${QGIS_INCLUDE_DIR})
endif()

install(TARGETS qgis_3d
  RUNTIME DESTINATION ${QGIS_BIN_DIR}
  LIBRARY DESTINATION ${QGIS_LIB_DIR}
  ARCHIVE DESTINATION ${QGIS_LIB_DIR})
