#ifndef QGSMATRIX4X4_H
#define QGSMATRIX4X4_H

#include "qgsvector3d.h"


class QgsMatrix4x4
{
  public:
    QgsMatrix4x4() { setToIdentity(); }
    QgsMatrix4x4( double m11, double m12, double m13, double m14,
                  double m21, double m22, double m23, double m24,
                  double m31, double m32, double m33, double m34,
                  double m41, double m42, double m43, double m44 );

    const double *constData() const { return *m; }
    double *data() { return *m; }

    QgsVector3D map( const QgsVector3D &vector ) const
    {
      return *this * vector;
    }

    bool isIdentity() const;
    void setToIdentity();

    friend QgsMatrix4x4 operator*( const QgsMatrix4x4 &m1, const QgsMatrix4x4 &m2 );
    friend QgsVector3D operator*( const QgsMatrix4x4 &matrix, const QgsVector3D &vector );

  private:
    double m[4][4];          // Column-major order to match OpenGL.

    // Construct without initializing identity matrix.
    explicit QgsMatrix4x4( int ) { }
};


#endif
