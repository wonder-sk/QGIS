// The metal rough material's own uniform block, shared by its vertex and fragment stages.
// Must stay in step with QgsMetalRoughUniforms in
// src/3d/engine/materials/qgsmetalroughmaterial3d.h.
//
// featureFlags is what replaces the eleven #defines the Qt3D fragment shader was
// preprocessed with. Those were independent booleans, so spelling out their combinations
// the way every other material does would mean baking two thousand shaders; they are
// branched on at run time instead. The branches are uniform across a draw, so they cost
// close to nothing, and it means the whole feature set lives in one readable shader.
//
// What cannot move to a flag is anything that changes which vertex attributes are read: a
// shader input no attribute feeds makes the graphics pipeline fail to build. Those stay
// baked variants, which is why the vertex stage still has defines and the fragment stage
// has only one, for the data defined colours it reads as varyings.

#ifndef QGS_METALROUGH_MATERIAL_GLSL
#define QGS_METALROUGH_MATERIAL_GLSL

const uint QGS_MR_BASE_COLOR_MAP = 1u;
const uint QGS_MR_METALNESS_MAP = 2u;
const uint QGS_MR_ROUGHNESS_MAP = 4u;
const uint QGS_MR_AMBIENT_OCCLUSION_MAP = 8u;
const uint QGS_MR_NORMAL_MAP = 16u;
const uint QGS_MR_HEIGHT_MAP = 32u;
const uint QGS_MR_EMISSION_MAP = 64u;
const uint QGS_MR_FLAT_SHADING = 128u;
const uint QGS_MR_ANISOTROPY = 256u;
const uint QGS_MR_CLEAR_COAT = 512u;

layout( std140, binding = 3 ) uniform QgsMetalRoughBlock
{
  vec4 baseColor;     // offset 0
  vec4 emissiveColor; // offset 16, rgb used

  float metalness;           // offset 32
  float roughness;           // offset 36
  float reflectance;         // offset 40
  float opacity;             // offset 44
  float anisotropy;          // offset 48
  float anisotropyRotation;  // offset 52, in radians
  float clearCoatFactor;     // offset 56
  float clearCoatRoughness;  // offset 60
  float parallaxScale;       // offset 64
  float emissiveFactor;      // offset 68

  // how the maps are sampled. The rotation is in degrees, and both it and the scale are
  // applied about texCoordOffset.
  float texCoordScale;    // offset 72
  float texCoordRotation; // offset 76
  vec2 texCoordOffset;    // offset 80

  uint featureFlags; // offset 88
  float unused0;     // offset 92

  // instancing only: mesh space to object space, often an axis flip or a glTF node
  // transform. Ignored when the material is not instanced.
  mat4 meshMatrix;       // offset 96
  mat3 meshNormalMatrix; // offset 160

  // instancing only: the rotation and scale used for instances that do not carry their
  // own. w of symbolScale is unused.
  vec4 symbolRotation; // offset 208
  vec4 symbolScale;    // offset 224
} material;

bool qgsHasFeature( const in uint flag )
{
  return ( material.featureFlags & flag ) != 0u;
}

#endif // QGS_METALROUGH_MATERIAL_GLSL
