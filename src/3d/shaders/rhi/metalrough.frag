#version 440
#extension GL_GOOGLE_include_directive : require

#include "qgsview.glsl"
#include "qgsmetalroughmaterial.glsl"
#include "qgsmetalrough.glsl"

layout( location = 0 ) in vec3 worldPosition;
layout( location = 1 ) in vec3 worldNormal;
layout( location = 2 ) in vec4 worldTangent;
layout( location = 3 ) in vec2 texCoord;

#ifdef DATA_DEFINED
layout( location = 4 ) in vec3 vsBaseColor;
layout( location = 5 ) in vec3 vsEmissionColor;
#endif

layout( location = 0 ) out vec4 fragColor;

// Every map is always bound, because a shader resource binding the pipeline declares has
// to be provided whether the material uses it or not. QgsMetalRoughMaterial3D binds a
// single one pixel white texture to every slot it has no real map for.
layout( binding = 4 ) uniform sampler2D baseColorMap;
layout( binding = 5 ) uniform sampler2D metalnessMap;
layout( binding = 6 ) uniform sampler2D roughnessMap;
layout( binding = 7 ) uniform sampler2D ambientOcclusionMap;
layout( binding = 8 ) uniform sampler2D normalMap;
layout( binding = 9 ) uniform sampler2D heightMap;
layout( binding = 10 ) uniform sampler2D emissionMap;

mat3 calcTangentToWorldSpaceMatrix( const in vec3 wNormal, const in vec4 wTangent )
{
  vec3 N = normalize( wNormal );

  // make the tangent truly orthogonal to the normal with Gram-Schmidt, so that the
  // matrix below can be built by transposing an orthogonal matrix
  vec3 wFixedTangent = normalize( wTangent.xyz - dot( wTangent.xyz, N ) * N );

  // the fourth tangent coordinate orients the binormal: +1 for a right handed system,
  // -1 for a left handed one
  vec3 wBinormal = cross( N, wFixedTangent ) * wTangent.w;

  return mat3( wFixedTangent, wBinormal, N );
}

mat3 calcTangentSpace( const in vec3 wNormal, const in vec3 wPosition, const in vec2 uv )
{
  // the y derivatives are corrected for which way the framebuffer's rows run, or the
  // whole tangent frame comes out mirrored on every backend but OpenGL
  vec3 Q1 = dFdx( wPosition );
  vec3 Q2 = dFdy( wPosition ) * view.framebufferYSign;
  vec2 st1 = dFdx( uv );
  vec2 st2 = dFdy( uv ) * view.framebufferYSign;

  vec3 N = normalize( wNormal );
  vec3 T = normalize( Q1 * st2.t - Q2 * st1.t );
  T = normalize( T - dot( T, N ) * N );
  vec3 B = -normalize( cross( N, T ) );

  return mat3( T, B, N );
}

// contact refinement parallax mapping, as described at
// https://www.artstation.com/blogs/andreariccardi/3VPo/a-new-approach-for-parallax-mapping-presenting-the-contact-refinement-parallax-mapping-technique
vec2 applyContactRefinementParallaxCoords( const in vec2 uv, const in vec3 viewDirTangent )
{
  float maxHeight = material.parallaxScale;
  float minHeight = maxHeight * 0.5;

  int numSteps = 15;

  float viewCorrection = ( -viewDirTangent.z ) + 2.0;
  float stepSize = 1.0 / ( float( numSteps ) + 1.0 );

  vec2 tsOffset = viewDirTangent.xy * vec2( 1.0, -1.0 ) * viewCorrection;

  vec2 stepOffset = tsOffset * vec2( maxHeight, maxHeight ) * stepSize;
  vec2 lastOffset = tsOffset * vec2( minHeight, minHeight ) + uv;

  float lastRayDepth = 1.0;
  float lastHeight = 1.0;
  vec2 p1 = vec2( 0.0 );
  vec2 p2 = vec2( 0.0 );
  bool refine = false;

  while ( numSteps > 0 )
  {
    vec2 candidateOffset = lastOffset - stepOffset;
    float currentRayDepth = lastRayDepth - stepSize;

    float currentHeight = texture( heightMap, candidateOffset ).r;

    if ( currentHeight > currentRayDepth )
    {
      p1 = vec2( currentRayDepth, currentHeight );
      p2 = vec2( lastRayDepth, lastHeight );

      if ( refine )
      {
        lastHeight = currentHeight;
        break;
      }
      else
      {
        refine = true;
        lastRayDepth = p2.x;
        stepSize /= float( numSteps );
        stepOffset /= float( numSteps );
        continue;
      }
    }
    lastOffset = candidateOffset;
    lastRayDepth = currentRayDepth;
    lastHeight = currentHeight;
    numSteps -= 1;
  }

  float diff1 = p1.x - p1.y;
  float diff2 = p2.x - p2.y;
  float denominator = diff2 - diff1;

  float parallaxAmount = 0.0;
  if ( denominator != 0.0 )
    parallaxAmount = ( p1.x * diff2 - p2.x * diff1 ) / denominator;

  float offsetDepth = ( ( 1.0 - parallaxAmount ) * -maxHeight ) + minHeight;
  return tsOffset * offsetDepth + uv;
}

void main()
{
  vec2 activeTexCoord = texCoord;
  vec3 worldView = normalize( view.eyePosition - worldPosition );

  bool needsTangentSpace = qgsHasFeature( QGS_MR_NORMAL_MAP ) || qgsHasFeature( QGS_MR_HEIGHT_MAP ) || qgsHasFeature( QGS_MR_ANISOTROPY );

  mat3 tangentToWorld = mat3( 1.0 );
  if ( needsTangentSpace )
  {
    if ( length( worldTangent.xyz ) > 0.001 )
    {
      // use the model's own tangents where it has them
      tangentToWorld = calcTangentToWorldSpaceMatrix( worldNormal, worldTangent );
    }
    else
    {
      // otherwise derive them, which is worse quality but always available
      tangentToWorld = calcTangentSpace( worldNormal, worldPosition, activeTexCoord );
    }
  }

  if ( qgsHasFeature( QGS_MR_HEIGHT_MAP ) )
  {
    // the view vector has to be in tangent space to step along the height field
    mat3 worldToTangent = transpose( tangentToWorld );
    vec3 tangentView = normalize( worldToTangent * worldView );
    activeTexCoord = applyContactRefinementParallaxCoords( texCoord, tangentView );
  }

#ifdef DATA_DEFINED
  vec4 c = vec4( vsBaseColor, 1.0 );
  vec3 emission = vsEmissionColor;
#else
  vec4 c = qgsHasFeature( QGS_MR_BASE_COLOR_MAP ) ? texture( baseColorMap, activeTexCoord ) : material.baseColor;
  vec3 emission = qgsHasFeature( QGS_MR_EMISSION_MAP ) ? texture( emissionMap, activeTexCoord ).rgb : material.emissiveColor.rgb;
#endif

  float m = qgsHasFeature( QGS_MR_METALNESS_MAP ) ? texture( metalnessMap, activeTexCoord ).r : material.metalness;
  float r = qgsHasFeature( QGS_MR_ROUGHNESS_MAP ) ? texture( roughnessMap, activeTexCoord ).r : material.roughness;
  float ao = qgsHasFeature( QGS_MR_AMBIENT_OCCLUSION_MAP ) ? texture( ambientOcclusionMap, activeTexCoord ).r : 1.0;

  vec3 n;
  if ( qgsHasFeature( QGS_MR_NORMAL_MAP ) )
  {
    vec3 mapN = texture( normalMap, activeTexCoord ).rgb * 2.0 - 1.0;
    n = normalize( tangentToWorld * mapN );
  }
  else if ( qgsHasFeature( QGS_MR_FLAT_SHADING ) )
  {
    // the facet's own normal, from how the world position changes across the screen. The
    // y derivative needs correcting for the framebuffer's row order, otherwise this comes
    // out pointing into the surface rather than out of it on everything but OpenGL.
    n = normalize( cross( dFdx( worldPosition ), dFdy( worldPosition ) * view.framebufferYSign ) );
  }
  else
  {
    n = normalize( worldNormal );
  }

  vec3 t = vec3( 0.0 );
  vec3 b = vec3( 0.0 );
  float aniso = 0.0;
  if ( qgsHasFeature( QGS_MR_ANISOTROPY ) )
  {
    vec3 anisotropyDirection = vec3( cos( material.anisotropyRotation ), sin( material.anisotropyRotation ), 0.0 );
    t = normalize( tangentToWorld * anisotropyDirection );
    t = normalize( t - dot( t, n ) * n );
    b = normalize( cross( n, t ) );
    aniso = material.anisotropy;
  }

  vec3 color = metalRoughFunction( c, m, r, ao, aniso, worldPosition, worldView, n, t, b, emission ).rgb;
  fragColor = vec4( color, material.opacity );
}
