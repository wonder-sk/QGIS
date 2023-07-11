
#include "3dtiles.h"

#include <Qt3DRender>
#include <Qt3DExtras>

#include "qgsvector3d.h"


Qt3DCore::QEntity *lineEntity( const QByteArray &vertexBufferData, int vertexCount, QColor color )
{
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
  bboxesMaterial->setAmbient( color );

  Qt3DCore::QEntity *e = new Qt3DCore::QEntity;
  e->addComponent( r );
  e->addComponent( bboxesMaterial );
  return e;
}

Qt3DCore::QEntity *entityForOBB( OBB &obb, SceneContext &ctx )
{
  QVector<QgsVector3D> corners = obb.cornersSceneCoords( ctx );

  int indexes[] =
  {
    0, 1, 2, 3, 0, 2, 1, 3,   // bottom rect
    4, 5, 6, 7, 4, 6, 5, 7,   // top rect
    0, 4, 1, 5, 2, 6, 3, 7,   // sides
  };
  int vertexCount = 24;

  QByteArray vertexBufferData;
  vertexBufferData.resize( vertexCount * 3 * sizeof( float ) );
  float *rawVertexArray = reinterpret_cast<float *>( vertexBufferData.data() );
  int idx = 0;

  for ( int i = 0; i < vertexCount; ++i )
  {
    QgsVector3D &v = corners[indexes[i]];
    rawVertexArray[idx++] = v.x();
    rawVertexArray[idx++] = v.z();
    rawVertexArray[idx++] = -v.y();
  }

  return lineEntity( vertexBufferData, vertexCount, Qt::red );
}

Qt3DCore::QEntity *entityForAABB( OBB &obb, SceneContext &ctx )
{
  QgsBox3d aabb = obb.aabb( ctx );
  QVector<QgsVector3D> corners
  {
    QgsVector3D( aabb.xMinimum(), aabb.yMinimum(), aabb.zMinimum() ),
    QgsVector3D( aabb.xMaximum(), aabb.yMinimum(), aabb.zMinimum() ),
    QgsVector3D( aabb.xMinimum(), aabb.yMaximum(), aabb.zMinimum() ),
    QgsVector3D( aabb.xMaximum(), aabb.yMaximum(), aabb.zMinimum() ),
    QgsVector3D( aabb.xMinimum(), aabb.yMinimum(), aabb.zMaximum() ),
    QgsVector3D( aabb.xMaximum(), aabb.yMinimum(), aabb.zMaximum() ),
    QgsVector3D( aabb.xMinimum(), aabb.yMaximum(), aabb.zMaximum() ),
    QgsVector3D( aabb.xMaximum(), aabb.yMaximum(), aabb.zMaximum() ),
  };

  int indexes[] =
  {
    0, 1, 2, 3, 0, 2, 1, 3,   // bottom rect
    4, 5, 6, 7, 4, 6, 5, 7,   // top rect
    0, 4, 1, 5, 2, 6, 3, 7,   // sides
  };
  int vertexCount = 24;

  QByteArray vertexBufferData;
  vertexBufferData.resize( vertexCount * 3 * sizeof( float ) );
  float *rawVertexArray = reinterpret_cast<float *>( vertexBufferData.data() );
  int idx = 0;

  for ( int i = 0; i < vertexCount; ++i )
  {
    QgsVector3D &v = corners[indexes[i]];
    rawVertexArray[idx++] = v.x();
    rawVertexArray[idx++] = v.z();
    rawVertexArray[idx++] = -v.y();
  }

  return lineEntity( vertexBufferData, vertexCount, Qt::blue );
}


QgsVector3D reproject( QgsCoordinateTransform &ct, QgsVector3D v, bool inv )
{
  QgsVector3D t = ct.transform( v, inv ? Qgis::TransformDirection::Reverse : Qgis::TransformDirection::Forward );
  return t;
}
