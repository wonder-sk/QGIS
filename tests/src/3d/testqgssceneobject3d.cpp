/***************************************************************************
     testqgssceneobject3d.cpp
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

#include "qgsgputexture.h"
#include "qgsprimitivegeometry3d.h"
#include "qgsrenderpass.h"
#include "qgssceneobject3d.h"
#include "qgstest.h"
#include "qgstexturematerial3d.h"
#include "qgsunlitmaterial3d.h"

#include <QImage>
#include <QMatrix4x4>
#include <QString>

using namespace Qt::StringLiterals;

class TestQgsSceneObject3D : public QgsTest
{
    Q_OBJECT

  public:
    TestQgsSceneObject3D()
      : QgsTest( u"QgsSceneObject3D"_s )
    {}

  private slots:
    void testEmptyObject();
    void testAddPart();
    void testCommandsCarryTheTransform();
    void testDisabledObjectDrawsNothing();
    void testShadowPassSkipsNonCasters();
    void testGeometrySourceIsOwned();
    void testGpuMemorySize();
};

void TestQgsSceneObject3D::testEmptyObject()
{
  const QgsSceneObject3D object;
  QVERIFY( object.parts().empty() );
  QVERIFY( object.isEnabled() );
  QCOMPARE( object.transform(), QMatrix4x4() );
  QCOMPARE( object.gpuMemorySize(), 0 );
}

void TestQgsSceneObject3D::testAddPart()
{
  QgsSceneObject3D object;

  QMatrix4x4 partTransform;
  partTransform.translate( 1, 2, 3 );

  QgsSceneObject3D::Part *part = object.addPart( QgsPrimitiveGeometry3D::cuboid(), std::make_unique<QgsUnlitMaterial3D>(), partTransform );

  QCOMPARE( object.parts().size(), size_t( 1 ) );
  QCOMPARE( object.parts()[0].get(), part );
  QVERIFY( part->geometry );
  QVERIFY( part->material );
  QCOMPARE( part->transform, partTransform );
  QVERIFY( part->castsShadows );
  QVERIFY( !part->isTransparent );

  // the caller keeps hold of the part so that it can reach back into its material
  static_cast<QgsUnlitMaterial3D *>( part->material.get() )->setColor( Qt::red );
}

void TestQgsSceneObject3D::testCommandsCarryTheTransform()
{
  QgsSceneObject3D object;

  QMatrix4x4 objectTransform;
  objectTransform.translate( 100, 0, 0 );
  object.setTransform( objectTransform );

  QMatrix4x4 partTransform;
  partTransform.scale( 2 );

  object.addPart( QgsPrimitiveGeometry3D::plane(), std::make_unique<QgsUnlitMaterial3D>(), partTransform );
  object.addPart( QgsPrimitiveGeometry3D::sphere(), std::make_unique<QgsUnlitMaterial3D>() );

  QgsRenderPass pass;
  object.collectCommands( pass, false, 7 );

  QCOMPARE( pass.commands().size(), size_t( 2 ) );

  // a command's world transform is the object's composed with the part's, which is what
  // the Qt3D transform hierarchy used to do
  QCOMPARE( pass.commands()[0].worldTransform, objectTransform * partTransform );
  QCOMPARE( pass.commands()[1].worldTransform, objectTransform );
  QCOMPARE( pass.commands()[0].sortKey, quint32( 7 ) );
  QCOMPARE( pass.commands()[0].geometry, object.parts()[0]->geometry );
  QCOMPARE( pass.commands()[0].material, object.parts()[0]->material.get() );
}

void TestQgsSceneObject3D::testDisabledObjectDrawsNothing()
{
  QgsSceneObject3D object;
  object.addPart( QgsPrimitiveGeometry3D::cuboid(), std::make_unique<QgsUnlitMaterial3D>() );

  QgsRenderPass pass;
  object.collectCommands( pass );
  QCOMPARE( pass.commands().size(), size_t( 1 ) );

  // this is what replaces QEntity::setEnabled() as a chunk goes in and out of view
  pass.clearCommands();
  object.setEnabled( false );
  object.collectCommands( pass );
  QVERIFY( pass.commands().empty() );
}

void TestQgsSceneObject3D::testShadowPassSkipsNonCasters()
{
  QgsSceneObject3D object;
  object.addPart( QgsPrimitiveGeometry3D::cuboid(), std::make_unique<QgsUnlitMaterial3D>() );
  QgsSceneObject3D::Part *flat = object.addPart( QgsPrimitiveGeometry3D::plane(), std::make_unique<QgsUnlitMaterial3D>() );
  flat->castsShadows = false;

  QgsRenderPass forwardPass;
  object.collectCommands( forwardPass, false );
  QCOMPARE( forwardPass.commands().size(), size_t( 2 ) );

  QgsRenderPass shadowPass;
  object.collectCommands( shadowPass, true );
  QCOMPARE( shadowPass.commands().size(), size_t( 1 ) );
  QCOMPARE( shadowPass.commands()[0].geometry, object.parts()[0]->geometry );
}

void TestQgsSceneObject3D::testGeometrySourceIsOwned()
{
  // the geometry a part points at belongs to the generator that built it, and the object
  // holds that generator; the part stays valid for as long as the object does
  const QgsGeometry3D *geometry = nullptr;
  {
    QgsSceneObject3D object;
    std::unique_ptr<QgsPrimitiveGeometry3D> source = QgsPrimitiveGeometry3D::cuboid();
    geometry = source->geometry();
    object.addPart( std::move( source ), std::make_unique<QgsUnlitMaterial3D>() );

    QCOMPARE( object.parts()[0]->geometry, geometry );
    QVERIFY( object.parts()[0]->geometry->vertexCount() > 0 );
  }
  // the object is gone and so is the generator; nothing to check but that we got here
  // without leaking, which the sanitizer builds are what really watch
  QVERIFY( geometry );
}

void TestQgsSceneObject3D::testGpuMemorySize()
{
  QgsSceneObject3D object;
  object.addPart( QgsPrimitiveGeometry3D::cuboid(), std::make_unique<QgsUnlitMaterial3D>() );

  const qint64 withOneShape = object.gpuMemorySize();
  QVERIFY( withOneShape > 0 );

  // a texture the object owns counts too, since that is what a terrain tile mostly is
  auto texture = std::make_unique<QgsGpuTexture>();
  QImage image( 64, 64, QImage::Format_RGBA8888 );
  image.fill( Qt::blue );
  texture->setImage( image );
  QgsGpuTexture *texturePtr = object.addTexture( std::move( texture ) );

  auto material = std::make_unique<QgsTextureMaterial3D>();
  material->setDiffuseTexture( texturePtr );
  object.addPart( QgsPrimitiveGeometry3D::plane(), std::move( material ) );

  QVERIFY( object.gpuMemorySize() >= withOneShape + 64 * 64 * 4 );
}

QGSTEST_MAIN( TestQgsSceneObject3D )
#include "testqgssceneobject3d.moc"
