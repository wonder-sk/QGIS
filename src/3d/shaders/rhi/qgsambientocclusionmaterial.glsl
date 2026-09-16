// The ambient occlusion material's own uniform block.
// Must stay in step with QgsAmbientOcclusionUniforms in
// src/3d/engine/materials/qgsambientocclusionmaterial3d.h.

#ifndef QGS_AMBIENT_OCCLUSION_MATERIAL_GLSL
#define QGS_AMBIENT_OCCLUSION_MATERIAL_GLSL

const int SSAO_KERNEL_SIZE = 64;
const int SSAO_NOISE_SIZE = 16;

layout( std140, binding = 3 ) uniform QgsAmbientOcclusionBlock
{
  // random sample vectors in a unit sphere, and a 4x4 pattern of random rotations. Both
  // are vec4 arrays rather than vec3 ones because std140 pads a vec3 array to a 16 byte
  // stride anyway, and spelling the padding out is what keeps the C++ struct plain.
  vec4 kernel[SSAO_KERNEL_SIZE];
  vec4 noise[SSAO_NOISE_SIZE];

  float intensity; // amplification of the shading
  float radius;    // radius of the neighbourhood sphere, in world units
  float threshold; // how much occlusion there has to be before anything darkens
  float tanHalfFov;
} material;

#endif // QGS_AMBIENT_OCCLUSION_MATERIAL_GLSL
