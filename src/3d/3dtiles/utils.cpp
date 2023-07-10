
#include "3dtiles.h"

#include <Qt3DRender>
#include <Qt3DExtras>


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
  QVector<VEC3D> corners = obb.cornersSceneCoords( ctx );

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
    VEC3D &v = corners[indexes[i]];
    rawVertexArray[idx++] = v.x;
    rawVertexArray[idx++] = v.z;
    rawVertexArray[idx++] = -v.y;
  }

  return lineEntity( vertexBufferData, vertexCount, Qt::red );
}

Qt3DCore::QEntity *entityForAABB( OBB &obb, SceneContext &ctx )
{
  AABB aabb = obb.aabb( ctx );
  QVector<VEC3D> corners
  {
    VEC3D( aabb.v0.x, aabb.v0.y, aabb.v0.z ),
    VEC3D( aabb.v1.x, aabb.v0.y, aabb.v0.z ),
    VEC3D( aabb.v0.x, aabb.v1.y, aabb.v0.z ),
    VEC3D( aabb.v1.x, aabb.v1.y, aabb.v0.z ),
    VEC3D( aabb.v0.x, aabb.v0.y, aabb.v1.z ),
    VEC3D( aabb.v1.x, aabb.v0.y, aabb.v1.z ),
    VEC3D( aabb.v0.x, aabb.v1.y, aabb.v1.z ),
    VEC3D( aabb.v1.x, aabb.v1.y, aabb.v1.z ),
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
    VEC3D &v = corners[indexes[i]];
    rawVertexArray[idx++] = v.x;
    rawVertexArray[idx++] = v.z;
    rawVertexArray[idx++] = -v.y;
  }

  return lineEntity( vertexBufferData, vertexCount, Qt::blue );
}


VEC3D reproject( PJ *transform, VEC3D v, bool inv )
{
  PJ_COORD a = proj_coord( v.x, v.y, v.z, 0 );
  PJ_COORD b = proj_trans( transform, inv ? PJ_INV : PJ_FWD, a );
  return VEC3D( b.v[0], b.v[1], b.v[2] );
}
