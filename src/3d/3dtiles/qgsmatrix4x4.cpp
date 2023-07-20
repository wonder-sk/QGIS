
#include "qgsmatrix4x4.h"

// based on Qt's QMatrix4x4 implementation (simplified)


QgsMatrix4x4::QgsMatrix4x4( double m11, double m12, double m13, double m14, double m21, double m22, double m23, double m24, double m31, double m32, double m33, double m34, double m41, double m42, double m43, double m44 )
{
  m[0][0] = m11; m[0][1] = m21; m[0][2] = m31; m[0][3] = m41;
  m[1][0] = m12; m[1][1] = m22; m[1][2] = m32; m[1][3] = m42;
  m[2][0] = m13; m[2][1] = m23; m[2][2] = m33; m[2][3] = m43;
  m[3][0] = m14; m[3][1] = m24; m[3][2] = m34; m[3][3] = m44;
}


QgsVector3D operator*( const QgsMatrix4x4 &matrix, const QgsVector3D &vector )
{
  double x, y, z, w;

  x = vector.x() * matrix.m[0][0] +
      vector.y() * matrix.m[1][0] +
      vector.z() * matrix.m[2][0] +
      matrix.m[3][0];
  y = vector.x() * matrix.m[0][1] +
      vector.y() * matrix.m[1][1] +
      vector.z() * matrix.m[2][1] +
      matrix.m[3][1];
  z = vector.x() * matrix.m[0][2] +
      vector.y() * matrix.m[1][2] +
      vector.z() * matrix.m[2][2] +
      matrix.m[3][2];
  w = vector.x() * matrix.m[0][3] +
      vector.y() * matrix.m[1][3] +
      vector.z() * matrix.m[2][3] +
      matrix.m[3][3];
  if ( w == 1.0f )
    return QgsVector3D( x, y, z );
  else
    return QgsVector3D( x / w, y / w, z / w );
}

bool QgsMatrix4x4::isIdentity() const
{
  if ( m[0][0] != 1.0f || m[0][1] != 0.0f || m[0][2] != 0.0f )
    return false;
  if ( m[0][3] != 0.0f || m[1][0] != 0.0f || m[1][1] != 1.0f )
    return false;
  if ( m[1][2] != 0.0f || m[1][3] != 0.0f || m[2][0] != 0.0f )
    return false;
  if ( m[2][1] != 0.0f || m[2][2] != 1.0f || m[2][3] != 0.0f )
    return false;
  if ( m[3][0] != 0.0f || m[3][1] != 0.0f || m[3][2] != 0.0f )
    return false;
  return ( m[3][3] == 1.0f );
}

void QgsMatrix4x4::setToIdentity()
{
  m[0][0] = 1.0f;
  m[0][1] = 0.0f;
  m[0][2] = 0.0f;
  m[0][3] = 0.0f;
  m[1][0] = 0.0f;
  m[1][1] = 1.0f;
  m[1][2] = 0.0f;
  m[1][3] = 0.0f;
  m[2][0] = 0.0f;
  m[2][1] = 0.0f;
  m[2][2] = 1.0f;
  m[2][3] = 0.0f;
  m[3][0] = 0.0f;
  m[3][1] = 0.0f;
  m[3][2] = 0.0f;
  m[3][3] = 1.0f;
}


QgsMatrix4x4 operator*( const QgsMatrix4x4 &m1, const QgsMatrix4x4 &m2 )
{
  QgsMatrix4x4 m( 1 );
  m.m[0][0] = m1.m[0][0] * m2.m[0][0]
              + m1.m[1][0] * m2.m[0][1]
              + m1.m[2][0] * m2.m[0][2]
              + m1.m[3][0] * m2.m[0][3];
  m.m[0][1] = m1.m[0][1] * m2.m[0][0]
              + m1.m[1][1] * m2.m[0][1]
              + m1.m[2][1] * m2.m[0][2]
              + m1.m[3][1] * m2.m[0][3];
  m.m[0][2] = m1.m[0][2] * m2.m[0][0]
              + m1.m[1][2] * m2.m[0][1]
              + m1.m[2][2] * m2.m[0][2]
              + m1.m[3][2] * m2.m[0][3];
  m.m[0][3] = m1.m[0][3] * m2.m[0][0]
              + m1.m[1][3] * m2.m[0][1]
              + m1.m[2][3] * m2.m[0][2]
              + m1.m[3][3] * m2.m[0][3];

  m.m[1][0] = m1.m[0][0] * m2.m[1][0]
              + m1.m[1][0] * m2.m[1][1]
              + m1.m[2][0] * m2.m[1][2]
              + m1.m[3][0] * m2.m[1][3];
  m.m[1][1] = m1.m[0][1] * m2.m[1][0]
              + m1.m[1][1] * m2.m[1][1]
              + m1.m[2][1] * m2.m[1][2]
              + m1.m[3][1] * m2.m[1][3];
  m.m[1][2] = m1.m[0][2] * m2.m[1][0]
              + m1.m[1][2] * m2.m[1][1]
              + m1.m[2][2] * m2.m[1][2]
              + m1.m[3][2] * m2.m[1][3];
  m.m[1][3] = m1.m[0][3] * m2.m[1][0]
              + m1.m[1][3] * m2.m[1][1]
              + m1.m[2][3] * m2.m[1][2]
              + m1.m[3][3] * m2.m[1][3];

  m.m[2][0] = m1.m[0][0] * m2.m[2][0]
              + m1.m[1][0] * m2.m[2][1]
              + m1.m[2][0] * m2.m[2][2]
              + m1.m[3][0] * m2.m[2][3];
  m.m[2][1] = m1.m[0][1] * m2.m[2][0]
              + m1.m[1][1] * m2.m[2][1]
              + m1.m[2][1] * m2.m[2][2]
              + m1.m[3][1] * m2.m[2][3];
  m.m[2][2] = m1.m[0][2] * m2.m[2][0]
              + m1.m[1][2] * m2.m[2][1]
              + m1.m[2][2] * m2.m[2][2]
              + m1.m[3][2] * m2.m[2][3];
  m.m[2][3] = m1.m[0][3] * m2.m[2][0]
              + m1.m[1][3] * m2.m[2][1]
              + m1.m[2][3] * m2.m[2][2]
              + m1.m[3][3] * m2.m[2][3];

  m.m[3][0] = m1.m[0][0] * m2.m[3][0]
              + m1.m[1][0] * m2.m[3][1]
              + m1.m[2][0] * m2.m[3][2]
              + m1.m[3][0] * m2.m[3][3];
  m.m[3][1] = m1.m[0][1] * m2.m[3][0]
              + m1.m[1][1] * m2.m[3][1]
              + m1.m[2][1] * m2.m[3][2]
              + m1.m[3][1] * m2.m[3][3];
  m.m[3][2] = m1.m[0][2] * m2.m[3][0]
              + m1.m[1][2] * m2.m[3][1]
              + m1.m[2][2] * m2.m[3][2]
              + m1.m[3][2] * m2.m[3][3];
  m.m[3][3] = m1.m[0][3] * m2.m[3][0]
              + m1.m[1][3] * m2.m[3][1]
              + m1.m[2][3] * m2.m[3][2]
              + m1.m[3][3] * m2.m[3][3];
  return m;
}
