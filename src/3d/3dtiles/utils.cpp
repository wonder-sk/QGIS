
#include "3dtiles.h"

#include <Qt3DRender>
#include <Qt3DExtras>

#include "qgsvector3d.h"


// TODO: move to qgis_3d utils ?
Qt3DCore::QEntity *entityForOBB( const QVector<QgsOrientedBox3D> &obbs, CoordsContext &coordsCtx )
{
  int vertexCount = 24 * obbs.count();

  QByteArray vertexBufferData;
  vertexBufferData.resize( vertexCount * 3 * sizeof( float ) );
  float *rawVertexArray = reinterpret_cast<float *>( vertexBufferData.data() );

  int indexes[] =
  {
    0, 1, 2, 3, 0, 2, 1, 3,   // bottom rect
    4, 5, 6, 7, 4, 6, 5, 7,   // top rect
    0, 4, 1, 5, 2, 6, 3, 7,   // sides
  };
  int idx = 0;

  for ( int k = 0; k < obbs.count(); ++k )
  {
    const QgsOrientedBox3D &obb = obbs[k];

    // reproject corners from ECEF to planar CRS
    QVector<QgsVector3D> corners = obb.corners();
    for ( int i = 0; i < corners.count(); ++i )
    {
      corners[i] = coordsCtx.ecefToTargetCrs->transform( corners[i] ) - coordsCtx.sceneOriginTargetCrs;
    }

    for ( int i = 0; i < 24; ++i )
    {
      QgsVector3D &v = corners[indexes[i]];
      rawVertexArray[idx++] = v.x();
      rawVertexArray[idx++] = v.z();
      rawVertexArray[idx++] = -v.y();
    }
  }

  Qt3DRender::QBuffer *vertexBuffer = new Qt3DRender::QBuffer;
  vertexBuffer->setData( vertexBufferData );

  Qt3DRender::QAttribute *posAttr = new Qt3DRender::QAttribute;
  posAttr->setAttributeType( Qt3DRender::QAttribute::VertexAttribute );
  posAttr->setBuffer( vertexBuffer );
  posAttr->setVertexBaseType( Qt3DRender::QAttribute::Float );
  posAttr->setVertexSize( 3 );
  posAttr->setName( Qt3DRender::QAttribute::defaultPositionAttributeName() );

  Qt3DRender::QGeometry *g = new Qt3DRender::QGeometry;
  g->addAttribute( posAttr );

  Qt3DRender::QGeometryRenderer *r = new Qt3DRender::QGeometryRenderer;
  r->setInstanceCount( 1 );
  r->setIndexOffset( 0 );
  r->setFirstInstance( 0 );
  r->setPrimitiveType( Qt3DRender::QGeometryRenderer::Lines );
  r->setGeometry( g );
  r->setVertexCount( vertexCount );

  Qt3DExtras::QPhongMaterial *bboxesMaterial = new Qt3DExtras::QPhongMaterial;
  bboxesMaterial->setAmbient( Qt::green );

  Qt3DCore::QEntity *e = new Qt3DCore::QEntity;
  e->addComponent( r );
  e->addComponent( bboxesMaterial );
  return e;
}
