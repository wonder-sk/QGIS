// The Phong shading model, ported from shaders/phong.inc.frag, which in turn came from
// qt3d/src/extras/shaders/gl3/phong.inc.frag.

#include "qgslights.glsl"

void adsModel( const in vec3 worldPos,
               const in vec3 worldNormal,
               const in vec3 worldView,
               const in float shininess,
               out vec3 diffuseColor,
               out vec3 specularColor )
{
  diffuseColor = vec3( 0.0 );
  specularColor = vec3( 0.0 );

  // all work happens in world space
  vec3 n = normalize( worldNormal );

  for ( int i = 0; i < lighting.lightCount; ++i )
  {
    LightParams light = calculateLightParams( i, worldPos, n, worldView );

    float diffuse = max( light.sDotN, 0.0 );

    float specular = 0.0;
    if ( diffuse > 0.0 && shininess > 0.0 )
    {
      float normFactor = ( shininess + 2.0 ) / ( 2.0 * 3.14159 );
      vec3 r = reflect( -light.s, n );
      specular = normFactor * pow( max( dot( r, worldView ), 0.0 ), shininess );
    }

    float weight = light.visibilityFactor * light.att * lightIntensity( i );
    diffuseColor += weight * diffuse * lightColor( i );
    specularColor += weight * specular * lightColor( i );
  }
}

vec4 phongFunction( const in vec4 ambient,
                    const in vec4 diffuse,
                    const in vec4 specular,
                    const in float shin,
                    const in vec3 worldPosition,
                    const in vec3 worldView,
                    const in vec3 worldNormal )
{
  vec3 diffuseColor, specularColor;
  adsModel( worldPosition, worldNormal, worldView, shin, diffuseColor, specularColor );

  vec3 color = ambient.rgb
             + diffuseColor * diffuse.rgb
             + specularColor * specular.rgb;

  return vec4( color, diffuse.a );
}
