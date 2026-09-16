/***************************************************************************
  qgsdemterraintilegeometry_p.cpp
  --------------------------------------
  Date                 : July 2017
  Copyright            : (C) 2017 by Martin Dobias
  Email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsdemterraintilegeometry_p.h"

#include <cmath>
#include <limits>

#include "qgis.h"
#include "qgsdemterraintilegeometry3d.h"
#include "qgsray3d.h"
#include "qgsraycastcontext.h"
#include "qgsraycastingutils.h"

#include <QMatrix4x4>
#include <Qt3DCore/QAbstractFunctor>
#include <Qt3DCore/QAttribute>
#include <Qt3DCore/QBuffer>

#include "moc_qgsdemterraintilegeometry_p.cpp"

///@cond PRIVATE

using namespace Qt3DRender;

//! Generates vertex buffer for DEM terrain tiles
class PlaneVertexBufferFunctor : public Qt3DCore::QAbstractFunctor
{
  public:
    explicit PlaneVertexBufferFunctor( int resolution, float side, float vertScale, float skirtHeight, const QByteArray &heightMap )
      : mResolution( resolution )
      , mSide( side )
      , mVertScale( vertScale )
      , mSkirtHeight( skirtHeight )
      , mHeightMap( heightMap )
    {}

    QByteArray operator()() { return QgsDemTerrainTileGeometry3D::createVertexData( mResolution, mSide, mVertScale, mSkirtHeight, mHeightMap ); }

    qintptr id() const override { return reinterpret_cast<qintptr>( &Qt3DCore::FunctorType<PlaneVertexBufferFunctor>::id ); }

    bool operator==( const Qt3DCore::QAbstractFunctor &other ) const
    {
      const PlaneVertexBufferFunctor *otherFunctor = dynamic_cast<const PlaneVertexBufferFunctor *>( &other );
      if ( otherFunctor )
        return (
          otherFunctor->mResolution == mResolution && otherFunctor->mSide == mSide && otherFunctor->mVertScale == mVertScale && otherFunctor->mSkirtHeight == mSkirtHeight && otherFunctor->mHeightMap == mHeightMap
        );
      return false;
    }

  private:
    int mResolution;
    float mSide;
    float mVertScale;
    float mSkirtHeight;
    QByteArray mHeightMap;
};

//! Generates index buffer for DEM terrain tiles
class PlaneIndexBufferFunctor : public Qt3DCore::QAbstractFunctor
{
  public:
    explicit PlaneIndexBufferFunctor( int resolution, const QByteArray &heightMap )
      : mResolution( resolution )
      , mHeightMap( heightMap )
    {}

    QByteArray operator()() { return QgsDemTerrainTileGeometry3D::createIndexData( mResolution, mHeightMap ); }

    qintptr id() const override { return reinterpret_cast<qintptr>( &Qt3DCore::FunctorType<PlaneIndexBufferFunctor>::id ); }

    bool operator==( const Qt3DCore::QAbstractFunctor &other ) const
    {
      const PlaneIndexBufferFunctor *otherFunctor = dynamic_cast<const PlaneIndexBufferFunctor *>( &other );
      if ( otherFunctor )
        return ( otherFunctor->mResolution == mResolution );
      return false;
    }

  private:
    int mResolution;
    QByteArray mHeightMap;
};

// ------------


DemTerrainTileGeometry::DemTerrainTileGeometry( int resolution, float side, float vertScale, float skirtHeight, const QByteArray &heightMap, DemTerrainTileGeometry::QNode *parent )
  : QGeometry( parent )
  , mResolution( resolution )
  , mSide( side )
  , mVertScale( vertScale )
  , mSkirtHeight( skirtHeight )
  , mHeightMap( heightMap )
{
  init();
}

bool DemTerrainTileGeometry::rayIntersection( const QgsRay3D &ray, const QgsRayCastContext &context, const QMatrix4x4 &worldTransform, QVector3D &intersectionPoint )
{
  return QgsDemTerrainTileGeometry3D::rayIntersection( mVertexBuffer->data(), mIndexBuffer->data(), ray, context, worldTransform, intersectionPoint );
}

void DemTerrainTileGeometry::init()
{
  mPositionAttribute = new Qt3DCore::QAttribute( this );
  mNormalAttribute = new Qt3DCore::QAttribute( this );
  mTexCoordAttribute = new Qt3DCore::QAttribute( this );
  mIndexAttribute = new Qt3DCore::QAttribute( this );
  mVertexBuffer = new Qt3DCore::QBuffer( this );
  mIndexBuffer = new Qt3DCore::QBuffer( this );

  int nVertsX = mResolution + 2;
  int nVertsZ = mResolution + 2;
  const int nVerts = nVertsX * nVertsZ;
  const int stride = ( 3 + 2 + 3 ) * sizeof( float );
  const int faces = 2 * ( nVertsX - 1 ) * ( nVertsZ - 1 );

  mPositionAttribute->setName( Qt3DCore::QAttribute::defaultPositionAttributeName() );
  mPositionAttribute->setVertexBaseType( Qt3DCore::QAttribute::Float );
  mPositionAttribute->setVertexSize( 3 );
  mPositionAttribute->setAttributeType( Qt3DCore::QAttribute::VertexAttribute );
  mPositionAttribute->setBuffer( mVertexBuffer );
  mPositionAttribute->setByteStride( stride );
  mPositionAttribute->setCount( nVerts );

  mTexCoordAttribute->setName( Qt3DCore::QAttribute::defaultTextureCoordinateAttributeName() );
  mTexCoordAttribute->setVertexBaseType( Qt3DCore::QAttribute::Float );
  mTexCoordAttribute->setVertexSize( 2 );
  mTexCoordAttribute->setAttributeType( Qt3DCore::QAttribute::VertexAttribute );
  mTexCoordAttribute->setBuffer( mVertexBuffer );
  mTexCoordAttribute->setByteStride( stride );
  mTexCoordAttribute->setByteOffset( 3 * sizeof( float ) );
  mTexCoordAttribute->setCount( nVerts );

  mNormalAttribute->setName( Qt3DCore::QAttribute::defaultNormalAttributeName() );
  mNormalAttribute->setVertexBaseType( Qt3DCore::QAttribute::Float );
  mNormalAttribute->setVertexSize( 3 );
  mNormalAttribute->setAttributeType( Qt3DCore::QAttribute::VertexAttribute );
  mNormalAttribute->setBuffer( mVertexBuffer );
  mNormalAttribute->setByteStride( stride );
  mNormalAttribute->setByteOffset( 5 * sizeof( float ) );
  mNormalAttribute->setCount( nVerts );

  mIndexAttribute->setAttributeType( Qt3DCore::QAttribute::IndexAttribute );
  mIndexAttribute->setVertexBaseType( Qt3DCore::QAttribute::UnsignedInt );
  mIndexAttribute->setBuffer( mIndexBuffer );

  // Each primitive has 3 vertives
  mIndexAttribute->setCount( faces * 3 );

  // switched to setting data instead of just setting data generators because we also need the buffers
  // available for ray-mesh intersections and we can't access the private copy of data in Qt (if there is any)

  mVertexBuffer->setData( PlaneVertexBufferFunctor( mResolution, mSide, mVertScale, mSkirtHeight, mHeightMap )() );
  mIndexBuffer->setData( PlaneIndexBufferFunctor( mResolution, mHeightMap )() );

  addAttribute( mPositionAttribute );
  addAttribute( mTexCoordAttribute );
  addAttribute( mNormalAttribute );
  addAttribute( mIndexAttribute );
}

/// @endcond
