#version 440
#extension GL_GOOGLE_include_directive : require

#include "qgsview.glsl"
#include "qgslights.glsl"
#include "qgspointcloudmaterial.glsl"

// Ported from shaders/pointcloud.frag.
//
// One fragment shader for every style and for both the point drawn and the triangulated
// path: the vertex stage writes all of the varyings whatever variant it is, so what is
// left here is a branch on material.style, which takes the same path for every fragment
// of a draw and so costs close to nothing.

layout( location = 0 ) in vec3 worldPosition;
layout( location = 1 ) in vec3 worldNormal;
layout( location = 2 ) in vec3 pointColor;
layout( location = 3 ) in float pointParameter;
layout( location = 4 ) flat in float classParameter;

layout( location = 0 ) out vec4 fragColor;

/**
 * The colour ramp, one texel per break point: the value in r, the colour in gba.
 *
 * Qt3D held this in a 1D texture, which QRhi has no equivalent of, so it is a texture one
 * texel high instead. It is deliberately not sRGB: the ramp is interpolated in sRGB space
 * and only converted to linear afterwards, or the result is not visually linear.
 */
layout( binding = 4 ) uniform sampler2D colorRampTexture;

int colorRampSize()
{
  return textureSize( colorRampTexture, 0 ).x;
}

vec4 colorRampTexel( const in int index )
{
  return texelFetch( colorRampTexture, ivec2( index, 0 ), 0 );
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

    if ( pointParameter <= first.x )
      return first.yzw;

    if ( pointParameter > first.x && pointParameter <= second.x )
      return mix( first.yzw, second.yzw, ( pointParameter - first.x ) / ( second.x - first.x ) );
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

    if ( isinf( texel.x ) || pointParameter < texel.x )
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
    if ( abs( pointParameter - texel.x ) < 0.01 )
      return texel.yzw;
  }

  return vec3( 0.0 );
}

vec4 colorRamp()
{
  if ( material.colorRampCount <= 0 )
    return vec4( 0.0, 0.0, 0.0, 1.0 );

  vec3 color = vec3( 0.0 );
  if ( material.colorRampType == QGS_RAMP_LINEAR )
    color = linearColorRamp();
  else if ( material.colorRampType == QGS_RAMP_DISCRETE )
    color = discreteColorRamp();
  else if ( material.colorRampType == QGS_RAMP_EXACT )
    color = exactColorRamp();

  return vec4( color, 1.0 );
}

//! The colour of the class this point belongs to, whose code indexes the ramp directly
vec4 classification()
{
  int index = int( classParameter ) - 1;
  if ( index < 0 || index >= colorRampSize() )
    return vec4( 0.0, 0.0, 0.0, 1.0 );

  return vec4( colorRampTexel( index ).yzw, 1.0 );
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

  if ( material.style == QGS_POINTCLOUD_STYLE_SINGLE_COLOR )
  {
    // already linear, converted when the material was configured
    color = material.singleColor;
  }
  else if ( material.style == QGS_POINTCLOUD_STYLE_RGB )
  {
    // linear too: the colours were linearised when the point data was built
    color = vec4( pointColor, 1.0 );
  }
  else
  {
    color = material.style == QGS_POINTCLOUD_STYLE_CLASSIFICATION ? classification() : colorRamp();

    // a ramp is interpolated in sRGB space so that it looks evenly spaced, so it has to
    // be converted to linear before it goes anywhere near the lighting
    color = vec4( pow( color.rgb, vec3( 2.2 ) ), color.a );
  }

  if ( qgsPointCloudHasFeature( QGS_POINTCLOUD_TRIANGULATED ) )
  {
    // a value found empirically, so that an unlit face of the surface is not pitch black
    const float ambianceFactor = 0.15;
    vec3 shadingNormal = normalize( worldNormal );
    color = vec4( color.rgb * ( diffuseLight( shadingNormal ) + ambianceFactor ), color.a );
  }

  fragColor = color;
}
