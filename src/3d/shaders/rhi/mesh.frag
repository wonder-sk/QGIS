#version 440
#extension GL_GOOGLE_include_directive : require

#include "qgsview.glsl"
#include "qgslights.glsl"
#include "qgsmeshmaterial.glsl"

layout( location = 0 ) in vec3 worldPosition;
layout( location = 1 ) in vec3 worldNormal;
layout( location = 2 ) in float magnitude;
layout( location = 3 ) in vec3 barycentric;

layout( location = 0 ) out vec4 fragColor;

/**
 * The colour ramp, one texel per break point: the value in r, the colour in gba.
 *
 * Qt3D held this in a 1D texture, which QRhi has no equivalent of, so it is a texture one
 * texel high instead. It is deliberately not sRGB: the ramp is interpolated in sRGB space
 * and only converted to linear afterwards, or the result is not visually linear.
 */
layout( binding = 4 ) uniform sampler2D colorRampTexture;

//! One texel per arrow: the length in r and the direction in g
layout( binding = 5 ) uniform sampler2D arrowsGridTexture;

//! The picture of an arrow, of which only the alpha channel is used
layout( binding = 6 ) uniform sampler2D arrowTexture;

int colorRampSize()
{
  return textureSize( colorRampTexture, 0 ).x;
}

vec4 colorRampTexel( const in int index )
{
  return texelFetch( colorRampTexture, ivec2( index, 0 ), 0 );
}

/**
 * How close this fragment is to the nearest edge of its triangle, as a wireframe coverage.
 *
 * The barycentric coordinate falls off towards each edge, and dividing it by how fast it
 * changes across the screen turns it into a distance in pixels, which is what makes the
 * line the same width however far away the triangle is.
 */
float edgeFactor()
{
  vec3 dBdx = dFdx( barycentric );
  vec3 dBdy = dFdy( barycentric );
  vec3 gradientLength = sqrt( dBdx * dBdx + dBdy * dBdy );
  vec3 d = barycentric / gradientLength;
  float dist = min( min( d.x, d.y ), d.z );

  if ( dist < material.lineWidth - 1.0 )
    return 0.0;
  if ( dist > material.lineWidth + 1.0 )
    return 1.0;

  float x = dist - ( material.lineWidth - 1.0 );
  return 1.0 - exp2( -2.0 * x * x );
}

vec3 linearColorRamp()
{
  int size = colorRampSize();

  if ( size == 1 )
    return colorRampTexel( 0 ).yzw;

  for ( int i = 0; i < size - 1; ++i )
  {
    vec4 first = colorRampTexel( i );
    vec4 second = colorRampTexel( i + 1 );

    if ( magnitude <= first.x )
      return first.yzw;

    if ( magnitude > first.x && magnitude <= second.x )
      return mix( first.yzw, second.yzw, ( magnitude - first.x ) / ( second.x - first.x ) );
  }

  // past the last break point, the last colour carries on
  return colorRampTexel( size - 1 ).yzw;
}

vec3 discreteColorRamp()
{
  int size = colorRampSize();
  vec3 color = vec3( 0.5 );

  for ( int i = 0; i < size; ++i )
  {
    vec4 texel = colorRampTexel( i );
    color = texel.yzw;

    if ( isinf( texel.x ) || magnitude < texel.x )
      return color;
  }

  return color;
}

vec3 exactColorRamp()
{
  int size = colorRampSize();

  for ( int i = 0; i < size; ++i )
  {
    vec4 texel = colorRampTexel( i );
    if ( abs( magnitude - texel.x ) < 0.01 )
      return texel.yzw;
  }

  return vec3( 0.5 );
}

vec4 colorRamp()
{
  if ( material.colorRampCount <= 0 )
    return vec4( 0.5, 0.5, 0.5, 1.0 );

  vec3 color = vec3( 0.5 );
  if ( material.colorRampType == QGS_RAMP_LINEAR )
    color = linearColorRamp();
  else if ( material.colorRampType == QGS_RAMP_DISCRETE )
    color = discreteColorRamp();
  else if ( material.colorRampType == QGS_RAMP_EXACT )
    color = exactColorRamp();

  return vec4( color, 1.0 );
}

//! How much of an arrow covers this fragment, from the grid cell it falls in
float arrows()
{
  ivec2 size = textureSize( arrowsGridTexture, 0 );

  float posX = ( worldPosition.x - material.arrowsMinCorner.x ) / material.arrowsSpacing + 0.5;
  float posY = ( worldPosition.y + material.arrowsMinCorner.y ) / material.arrowsSpacing + 0.5;
  int gridPosX = int( posX );
  int gridPosY = int( posY );

  if ( gridPosX < 0 || gridPosX > size.x || gridPosY < 0 || gridPosY > size.y )
    return 0.0;

  vec4 gridValue = texelFetch( arrowsGridTexture, ivec2( gridPosX, gridPosY ), 0 );
  float scale = gridValue.x;
  float angle = gridValue.y - 1.570796;

  vec2 arrowPosition = vec2( posX - gridPosX, posY - gridPosY );

  float s = sin( angle );
  float c = cos( angle );
  vec2 pivot = vec2( 0.5 );
  mat2 rotationMatrix = mat2( c, -s, s, c );

  arrowPosition = rotationMatrix * ( arrowPosition - pivot );
  arrowPosition /= scale;
  arrowPosition += pivot;

  return texture( arrowTexture, arrowPosition ).a;
}

//! The diffuse light reaching this fragment, with no specular term at all
vec3 diffuseLight( const in vec3 n )
{
  vec3 worldView = normalize( view.eyePosition - worldPosition );
  vec3 diffuseColor = vec3( 0.0 );

  for ( int i = 0; i < lighting.lightCount; ++i )
  {
    LightParams light = calculateLightParams( i, worldPosition, n, worldView );
    diffuseColor += light.visibilityFactor * light.att * lightIntensity( i ) * max( light.sDotN, 0.0 ) * lightColor( i );
  }

  return diffuseColor;
}

void main()
{
  vec4 color;
  if ( material.textureType == QGS_MESH_STYLE_SINGLE_COLOR )
  {
    color = material.meshColor;
  }
  else
  {
    color = colorRamp();

    // the ramp is interpolated in sRGB space so that it looks evenly spaced, so it has to
    // be converted to linear before it goes anywhere near the lighting
    color = vec4( pow( color.rgb, vec3( 2.2 ) ), color.a );
  }

  vec3 shadingNormal;
  if ( qgsMeshHasFeature( QGS_MESH_FLAT_TRIANGLES ) )
  {
    // the facet's own normal. The y derivative is corrected for the framebuffer's row
    // order, or this points into the surface on every backend but OpenGL.
    shadingNormal = normalize( cross( dFdx( worldPosition ), dFdy( worldPosition ) * view.framebufferYSign ) );
  }
  else
  {
    shadingNormal = normalize( worldNormal );
  }

  const float ambianceFactor = 0.15;
  color = vec4( color.rgb * ( diffuseLight( shadingNormal ) + ambianceFactor ), 1.0 );

  if ( qgsMeshHasFeature( QGS_MESH_WIREFRAME ) )
    color = mix( material.lineColor, color, edgeFactor() );

  if ( qgsMeshHasFeature( QGS_MESH_ARROWS ) )
  {
    float a = arrows();
    color = a * material.arrowsColor + ( 1.0 - a ) * color;
  }

  fragColor = color;
}
