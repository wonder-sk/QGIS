// The Gooch material's own uniform block, shared by its vertex and fragment stages.
// Must stay in step with QgsGoochUniforms in src/3d/engine/materials/qgsgoochmaterial3d.h.

layout( std140, binding = 3 ) uniform QgsGoochBlock
{
  vec4 diffuseColor;  // kd in the paper, rgb used
  vec4 specularColor; // ks, rgb used
  vec4 warmColor;     // kyellow, rgb used
  vec4 coolColor;     // kblue, rgb used

  float shininess;
  float alpha;   // fraction of the diffuse colour added to the cool tone
  float beta;    // fraction of the diffuse colour added to the warm tone
  float unused0; // the Gooch model has no opacity, this only pads out to a vec4

  // instancing only: mesh space to object space, often an axis flip or a glTF node
  // transform. Ignored when the material is not instanced.
  mat4 meshMatrix;
  mat3 meshNormalMatrix;

  // instancing only: the rotation and scale used for instances that do not carry their
  // own. w of symbolScale is unused.
  vec4 symbolRotation;
  vec4 symbolScale;
} material;
