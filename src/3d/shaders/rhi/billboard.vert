#version 440
#extension GL_GOOGLE_include_directive : require

#include "qgsview.glsl"
#include "qgsdraw.glsl"
#include "qgsbillboardmaterial.glsl"

// the four corners of a unit quad, from -0.5 to 0.5, drawn once per billboard
layout( location = 0 ) in vec2 vertexPosition;

// everything below advances once per instance
layout( location = 1 ) in vec3 instancePosition;

#ifdef PER_INSTANCE_SIZE
layout( location = 2 ) in vec2 instanceSize;
#endif

#ifdef TEXTURE_ATLAS
layout( location = 3 ) in vec2 atlasOffset; // top left corner of the sprite, normalised
layout( location = 4 ) in vec2 atlasSize;   // size of the sprite, normalised
#endif

#ifdef TEXTURE_ATLAS_PIXEL_OFFSETS
layout( location = 5 ) in ivec2 pixelOffset;
#endif

layout( location = 0 ) out vec2 UV;

void main()
{
#ifdef PER_INSTANCE_SIZE
  vec2 bbSize = instanceSize;
#else
  vec2 bbSize = material.billboardSize;
#endif

  vec2 offsetPos = vertexPosition;
  if ( qgsBillboardHasFeature( QGS_BILLBOARD_VERTICAL_OFFSET ) )
    offsetPos.y += material.verticalOffset;

  vec2 textureOffset = vec2( 0.0 );
  vec2 textureSize = vec2( 1.0 );
#ifdef TEXTURE_ATLAS
  textureOffset = atlasOffset;
  textureSize = atlasSize;
#endif

  if ( qgsBillboardHasFeature( QGS_BILLBOARD_PERSPECTIVE_SCALE ) )
  {
    // the quad is sized in world units, so it is offset in view space and the size it
    // ends up on screen follows from the projection
    vec4 viewPos = view.viewMatrix * draw.modelMatrix * vec4( instancePosition, 1.0 );
    viewPos.xy += offsetPos * bbSize;
    gl_Position = view.projectionMatrix * viewPos;
  }
  else
  {
    // the quad is sized in pixels, so it is offset after the perspective divide. The
    // factor of two is because clip space runs from -1 to 1 rather than 0 to 1.
    vec2 viewportSize = vec2( view.viewportWidth, view.viewportHeight );
    vec2 spritePixelSize = 2.0 * bbSize / viewportSize;

#ifdef TEXTURE_ATLAS
    // scale by the sprite's own size in the atlas, so that billboards keep the relative
    // sizes of the symbols they were rendered from
    spritePixelSize *= textureSize;
#endif

    vec4 P = draw.modelViewProjectionMatrix * vec4( instancePosition, 1.0 );
    P /= P.w;
    P.xy += offsetPos * spritePixelSize;

#ifdef TEXTURE_ATLAS_PIXEL_OFFSETS
    P.xy += 2.0 * vec2( pixelOffset ) / viewportSize;
#endif

    gl_Position = P;
  }

  UV = textureOffset + ( vertexPosition + vec2( 0.5 ) ) * textureSize;
}
