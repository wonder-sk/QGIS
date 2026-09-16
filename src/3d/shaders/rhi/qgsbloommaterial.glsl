// The bloom material's own uniform block.
// Must stay in step with QgsBloomUniforms in
// src/3d/engine/materials/qgsbloommaterial3d.h.

#ifndef QGS_BLOOM_MATERIAL_GLSL
#define QGS_BLOOM_MATERIAL_GLSL

layout( std140, binding = 3 ) uniform QgsBloomBlock
{
  // how far the upsampling tent filter reaches, in texture coordinates. The downsampling
  // stage works in texels and ignores both of these.
  float filterRadius;
  float aspectRatio;
} material;

#endif // QGS_BLOOM_MATERIAL_GLSL
