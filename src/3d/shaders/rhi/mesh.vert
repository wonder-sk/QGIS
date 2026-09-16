#version 440
#extension GL_GOOGLE_include_directive : require

#include "qgsview.glsl"
#include "qgsdraw.glsl"
#include "qgsmeshmaterial.glsl"

layout( location = 0 ) in vec3 vertexPosition;
layout( location = 1 ) in vec3 vertexNormal;

#ifdef HAS_SCALAR_MAGNITUDE
layout( location = 2 ) in float scalarMagnitude;
#endif

layout( location = 0 ) out vec3 worldPosition;
layout( location = 1 ) out vec3 worldNormal;
layout( location = 2 ) out float magnitude;
layout( location = 3 ) out vec3 barycentric;

// which corner of its triangle a vertex is, so that the fragment stage can work out how
// far it is from the nearest edge and draw the wireframe without any extra geometry. This
// only holds while triangles are drawn as separate triples of vertices, which is how
// QgsMesh3DGeometry builds them.
const vec3 BARYCENTRIC[3] = vec3[3]( vec3( 1.0, 0.0, 0.0 ), vec3( 0.0, 1.0, 0.0 ), vec3( 0.0, 0.0, 1.0 ) );

void main()
{
  worldPosition = vec3( draw.modelMatrix * vec4( vertexPosition, 1.0 ) );

  // the mesh's normals are already in world space, as they were under Qt3D
  worldNormal = vertexNormal;

  barycentric = BARYCENTRIC[gl_VertexIndex % 3];

#ifdef HAS_SCALAR_MAGNITUDE
  magnitude = scalarMagnitude;
#else
  magnitude = worldPosition.z;
#endif

  gl_Position = draw.modelViewProjectionMatrix * vec4( vertexPosition, 1.0 );
}
