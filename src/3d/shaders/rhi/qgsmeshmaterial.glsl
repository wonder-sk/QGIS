// The mesh layer material's own uniform block, shared by its vertex and fragment stages.
// Must stay in step with QgsMeshUniforms in src/3d/engine/materials/qgsmeshmaterial3d.h.

#ifndef QGS_MESH_MATERIAL_GLSL
#define QGS_MESH_MATERIAL_GLSL

const uint QGS_MESH_FLAT_TRIANGLES = 1u;
const uint QGS_MESH_WIREFRAME = 2u;
const uint QGS_MESH_ARROWS = 4u;

// how a fragment's colour is chosen, matching QgsMesh3DSymbol::RenderingStyle
const int QGS_MESH_STYLE_SINGLE_COLOR = 0;
const int QGS_MESH_STYLE_COLOR_RAMP = 1;
const int QGS_MESH_STYLE_COLOR_RAMP_2D = 2;

// how the colour ramp maps a value to a colour, matching Qgis::ShaderInterpolationMethod
const int QGS_RAMP_LINEAR = 0;
const int QGS_RAMP_DISCRETE = 1;
const int QGS_RAMP_EXACT = 2;

layout( std140, binding = 3 ) uniform QgsMeshBlock
{
  vec4 meshColor;   // offset 0, the single colour style's colour
  vec4 lineColor;   // offset 16, the wireframe's colour
  vec4 arrowsColor; // offset 32

  // where the arrow grid's first cell sits, and how far apart its cells are, in map units
  vec2 arrowsMinCorner; // offset 48
  float arrowsSpacing;  // offset 56
  float lineWidth;      // offset 60

  int textureType;    // offset 64
  int colorRampType;  // offset 68
  int colorRampCount; // offset 72
  uint featureFlags;  // offset 76
} material;

bool qgsMeshHasFeature( const in uint flag )
{
  return ( material.featureFlags & flag ) != 0u;
}

#endif // QGS_MESH_MATERIAL_GLSL
