// The overlay texture material's own uniform block.
// Must stay in step with QgsOverlayTextureUniforms in
// src/3d/engine/materials/qgsoverlaytexturematerial3d.h.

#ifndef QGS_OVERLAY_TEXTURE_MATERIAL_GLSL
#define QGS_OVERLAY_TEXTURE_MATERIAL_GLSL

layout( std140, binding = 3 ) uniform QgsOverlayTextureBlock
{
  // where on screen the quad goes, in texture coordinates: the centre, and half the size
  vec2 centerTexCoords;
  vec2 sizeTexCoords;

  int isDepth;      // show the red channel as grey, for previewing a depth map
  int flipTextureY; // sample the texture bottom up
} material;

#endif // QGS_OVERLAY_TEXTURE_MATERIAL_GLSL
