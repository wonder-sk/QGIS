/***************************************************************************
     testqgscamera3d.cpp
     ----------------------
    Date                 : September 2026
    Copyright            : (C) 2026 by Martin Dobias
    Email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "qgs3dutils.h"
#include "qgscamera3d.h"
#include "qgscamerapose.h"
#include "qgstest.h"

#include <QMatrix4x4>
#include <QString>
#include <QVector3D>

using namespace Qt::StringLiterals;

class TestQgsCamera3D : public QgsTest
{
    Q_OBJECT

  public:
    TestQgsCamera3D()
      : QgsTest( u"QgsCamera3D"_s )
    {}

  private slots:
    void testDefaults();
    void testViewMatrix();
    void testPerspectiveProjection();
    void testOrthographicProjection();
    void testMatricesAreCached();
    void testCopy();
    void testCameraPose();
    void testGlobeCameraPose();
    void testScreenPointRoundTrip();
};

void TestQgsCamera3D::testDefaults()
{
  const QgsCamera3D camera;
  QCOMPARE( camera.projectionType(), Qgis::Map3DProjectionType::Perspective );
  QCOMPARE( camera.position(), QVector3D( 0, 0, 1 ) );
  QCOMPARE( camera.viewCenter(), QVector3D( 0, 0, 0 ) );
  QCOMPARE( camera.upVector(), QVector3D( 0, 1, 0 ) );
  QCOMPARE( camera.viewVector(), QVector3D( 0, 0, -1 ) );
}

void TestQgsCamera3D::testViewMatrix()
{
  QgsCamera3D camera;
  camera.setPosition( QVector3D( 0, 0, 100 ) );
  camera.setViewCenter( QVector3D( 0, 0, 0 ) );
  camera.setUpVector( QVector3D( 0, 1, 0 ) );

  // the point the camera looks at lands on the negative z axis at the camera's distance
  const QVector3D viewCenterInView = camera.viewMatrix() * QVector3D( 0, 0, 0 );
  QGSCOMPARENEARVECTOR3D( viewCenterInView, QVector3D( 0, 0, -100 ), 0.0001 );

  // and a point one unit up in the world is one unit up in view coordinates
  const QVector3D upInView = camera.viewMatrix() * QVector3D( 0, 1, 0 );
  QGSCOMPARENEARVECTOR3D( upInView, QVector3D( 0, 1, -100 ), 0.0001 );

  // matches what Qt3D's camera produced for the same arguments
  QMatrix4x4 expected;
  expected.lookAt( QVector3D( 0, 0, 100 ), QVector3D( 0, 0, 0 ), QVector3D( 0, 1, 0 ) );
  QCOMPARE( camera.viewMatrix(), expected );
}

void TestQgsCamera3D::testPerspectiveProjection()
{
  QgsCamera3D camera;
  camera.setPerspectiveProjection( 45.0f, 2.0f, 10.0f, 100.0f );

  QCOMPARE( camera.projectionType(), Qgis::Map3DProjectionType::Perspective );
  QCOMPARE( camera.fieldOfView(), 45.0f );
  QCOMPARE( camera.aspectRatio(), 2.0f );
  QCOMPARE( camera.nearPlane(), 10.0f );
  QCOMPARE( camera.farPlane(), 100.0f );

  QMatrix4x4 expected;
  expected.perspective( 45.0f, 2.0f, 10.0f, 100.0f );
  QCOMPARE( camera.projectionMatrix(), expected );

  // clip space z runs [-1,1] here, as OpenGL and Qt3D define it; the render pass is what
  // maps that onto whatever the current backend wants
  const QVector4D onNearPlane = camera.projectionMatrix() * QVector4D( 0, 0, -10, 1 );
  QGSCOMPARENEAR( onNearPlane.z() / onNearPlane.w(), -1.0, 0.0001 );
  const QVector4D onFarPlane = camera.projectionMatrix() * QVector4D( 0, 0, -100, 1 );
  QGSCOMPARENEAR( onFarPlane.z() / onFarPlane.w(), 1.0, 0.0001 );
}

void TestQgsCamera3D::testOrthographicProjection()
{
  QgsCamera3D camera;
  camera.setOrthographicProjection( -20.0f, 20.0f, -10.0f, 10.0f, 1.0f, 500.0f );

  QCOMPARE( camera.projectionType(), Qgis::Map3DProjectionType::Orthographic );
  QCOMPARE( camera.left(), -20.0f );
  QCOMPARE( camera.right(), 20.0f );
  QCOMPARE( camera.bottom(), -10.0f );
  QCOMPARE( camera.top(), 10.0f );
  QCOMPARE( camera.nearPlane(), 1.0f );
  QCOMPARE( camera.farPlane(), 500.0f );

  QMatrix4x4 expected;
  expected.ortho( -20.0f, 20.0f, -10.0f, 10.0f, 1.0f, 500.0f );
  QCOMPARE( camera.projectionMatrix(), expected );

  // switching a plane keeps the projection orthographic
  camera.setFarPlane( 800.0f );
  QCOMPARE( camera.projectionType(), Qgis::Map3DProjectionType::Orthographic );
  expected.setToIdentity();
  expected.ortho( -20.0f, 20.0f, -10.0f, 10.0f, 1.0f, 800.0f );
  QCOMPARE( camera.projectionMatrix(), expected );
}

void TestQgsCamera3D::testMatricesAreCached()
{
  QgsCamera3D camera;
  camera.setPerspectiveProjection( 45.0f, 1.0f, 1.0f, 100.0f );
  camera.setPosition( QVector3D( 5, 0, 0 ) );

  const QMatrix4x4 view = camera.viewMatrix();
  const QMatrix4x4 projection = camera.projectionMatrix();

  // reading twice gives the same answer
  QCOMPARE( camera.viewMatrix(), view );
  QCOMPARE( camera.projectionMatrix(), projection );

  // setting a value to what it already is does not disturb anything
  camera.setPosition( QVector3D( 5, 0, 0 ) );
  camera.setFieldOfView( 45.0f );
  QCOMPARE( camera.viewMatrix(), view );
  QCOMPARE( camera.projectionMatrix(), projection );

  // but a real change is picked up
  camera.setPosition( QVector3D( 0, 5, 0 ) );
  QVERIFY( camera.viewMatrix() != view );
  camera.setFieldOfView( 90.0f );
  QVERIFY( camera.projectionMatrix() != projection );
}

void TestQgsCamera3D::testCopy()
{
  // this is what replaces Qt3DUtils::copyCamera(), which had to clone an entity
  QgsCamera3D camera;
  camera.setPerspectiveProjection( 60.0f, 1.5f, 2.0f, 200.0f );
  camera.setPosition( QVector3D( 1, 2, 3 ) );
  camera.setViewCenter( QVector3D( 4, 5, 6 ) );
  camera.setUpVector( QVector3D( 0, 0, 1 ) );

  const QgsCamera3D copy = camera;
  QCOMPARE( copy.position(), camera.position() );
  QCOMPARE( copy.viewCenter(), camera.viewCenter() );
  QCOMPARE( copy.upVector(), camera.upVector() );
  QCOMPARE( copy.fieldOfView(), camera.fieldOfView() );
  QCOMPARE( copy.aspectRatio(), camera.aspectRatio() );
  QCOMPARE( copy.nearPlane(), camera.nearPlane() );
  QCOMPARE( copy.farPlane(), camera.farPlane() );
  QCOMPARE( copy.viewMatrix(), camera.viewMatrix() );
  QCOMPARE( copy.projectionMatrix(), camera.projectionMatrix() );

  // the copy is independent of the original
  camera.setPosition( QVector3D( 9, 9, 9 ) );
  QCOMPARE( copy.position(), QVector3D( 1, 2, 3 ) );
}

void TestQgsCamera3D::testCameraPose()
{
  QgsCamera3D camera;
  QgsCameraPose pose;
  pose.setCenterPoint( QgsVector3D( 10, 20, 0 ) );
  pose.setDistanceFromCenterPoint( 100 );
  pose.updateCamera( &camera );

  // looking straight down at the centre point from the given distance
  QCOMPARE( camera.viewCenter(), QVector3D( 10, 20, 0 ) );
  QGSCOMPARENEARVECTOR3D( camera.position(), QVector3D( 10, 20, 100 ), 0.0001 );
  QGSCOMPARENEARVECTOR3D( camera.upVector(), QVector3D( 0, 1, 0 ), 0.0001 );

  // pitched over to look from the side
  pose.setPitchAngle( 90 );
  pose.updateCamera( &camera );
  QCOMPARE( camera.viewCenter(), QVector3D( 10, 20, 0 ) );
  QGSCOMPARENEARVECTOR3D( camera.position(), QVector3D( 10, -80, 0 ), 0.0001 );
  QGSCOMPARENEARVECTOR3D( camera.upVector(), QVector3D( 0, 0, 1 ), 0.0001 );
}

void TestQgsCamera3D::testGlobeCameraPose()
{
  QgsCamera3D camera;
  QgsCameraPose pose;
  // a point on the equator at zero longitude, in ECEF, with the camera above it
  pose.setCenterPoint( QgsVector3D( 6378137, 0, 0 ) );
  pose.setDistanceFromCenterPoint( 1000000 );
  pose.updateCameraGlobe( &camera, 0, 0 );

  QGSCOMPARENEARVECTOR3D( camera.viewCenter(), QVector3D( 6378137, 0, 0 ), 1.0 );
  QGSCOMPARENEARVECTOR3D( camera.position(), QVector3D( 7378137, 0, 0 ), 1.0 );
  // up at the equator points at the north pole
  QGSCOMPARENEARVECTOR3D( camera.upVector(), QVector3D( 0, 0, 1 ), 0.0001 );
}

void TestQgsCamera3D::testScreenPointRoundTrip()
{
  QgsCamera3D camera;
  camera.setPerspectiveProjection( 45.0f, 1.0f, 1.0f, 1000.0f );
  camera.setPosition( QVector3D( 0, 0, 100 ) );
  camera.setViewCenter( QVector3D( 0, 0, 0 ) );
  camera.setUpVector( QVector3D( 0, 1, 0 ) );

  const QSize screen( 400, 400 );

  // the ray through the middle of the screen runs down the view vector
  const QgsRay3D ray = Qgs3DUtils::rayFromScreenPoint( QPoint( 200, 200 ), screen, &camera );
  QGSCOMPARENEARVECTOR3D( ray.origin(), QVector3D( 0, 0, 100 ), 0.001 );
  QGSCOMPARENEARVECTOR3D( ray.direction(), QVector3D( 0, 0, -1 ), 0.0001 );

  // and a world point projected to the screen comes back to itself
  const QVector3D world( 12, -7, 0 );
  const QVector4D clip = camera.projectionMatrix() * camera.viewMatrix() * QVector4D( world, 1.0f );
  const QVector3D ndc = clip.toVector3D() / clip.w();
  const QPoint screenPoint( static_cast<int>( std::round( ( ndc.x() + 1.0f ) * 0.5f * screen.width() ) ), static_cast<int>( std::round( ( 1.0f - ndc.y() ) * 0.5f * screen.height() ) ) );
  const double depth = ( ndc.z() + 1.0 ) * 0.5;
  const QVector3D back = Qgs3DUtils::screenPointToWorldPos( screenPoint, depth, screen, &camera );
  QGSCOMPARENEARVECTOR3D( back, world, 0.5 );
}

QGSTEST_MAIN( TestQgsCamera3D )
#include "testqgscamera3d.moc"
