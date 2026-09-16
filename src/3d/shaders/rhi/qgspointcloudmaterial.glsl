// The point cloud material's own uniform block, shared by its vertex and fragment stages.
// Must stay in step with QgsPointCloudUniforms in
// src/3d/engine/materials/qgspointcloudmaterial3d.h.

#ifndef QGS_POINTCLOUD_MATERIAL_GLSL
#define QGS_POINTCLOUD_MATERIAL_GLSL

const uint QGS_POINTCLOUD_TRIANGULATED = 1u;

// where a point takes its colour from. Matches QgsPointCloudMaterial3D::Style, and also
// decides which vertex shader variant is baked, since each style reads a different set of
// vertex attributes.
const int QGS_POINTCLOUD_STYLE_SINGLE_COLOR = 0;
const int QGS_POINTCLOUD_STYLE_COLOR_RAMP = 1;
const int QGS_POINTCLOUD_STYLE_RGB = 2;
const int QGS_POINTCLOUD_STYLE_CLASSIFICATION = 3;

// how the colour ramp maps a value to a colour, matching Qgis::ShaderInterpolationMethod
const int QGS_RAMP_LINEAR = 0;
const int QGS_RAMP_DISCRETE = 1;
const int QGS_RAMP_EXACT = 2;

layout( std140, binding = 3 ) uniform QgsPointCloudBlock
{
  vec4 singleColor; // offset 0, the single colour style's colour

  float pointSize;    // offset 16, in pixels, ignored when points are triangulated
  int style;          // offset 20
  int colorRampType;  // offset 24
  int colorRampCount; // offset 28

  uint featureFlags; // offset 32

  // std140 would round the block up to 48 bytes anyway; spelling the padding out is what
  // lets the C++ struct be a plain copy of this with nothing left to guess at
  float unused0; // offset 36
  float unused1; // offset 40
  float unused2; // offset 44
} material;

bool qgsPointCloudHasFeature( const in uint flag )
{
  return ( material.featureFlags & flag ) != 0u;
}

#endif // QGS_POINTCLOUD_MATERIAL_GLSL
