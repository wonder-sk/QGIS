/***************************************************************************
     testqgsmaterials3d.cpp
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

#include <cmath>

#include "qgsbillboardmaterial3d.h"
#include "qgscolorrampshader.h"
#include "qgsgeometry3d.h"
#include "qgsglobematerial3d.h"
#include "qgsgoochmaterial3d.h"
#include "qgsgpubuffer.h"
#include "qgsgputexture.h"
#include "qgsgraphicspipelinecache.h"
#include "qgsmeshmaterial3d.h"
#include "qgsmetalroughmaterial3d.h"
#include "qgsphongtexturedmaterial3d.h"
#include "qgsrendercommand.h"
#include "qgsrenderpass.h"
#include "qgsrenderuniforms.h"
#include "qgsrhiengine.h"
#include "qgsrhishader.h"
#include "qgstest.h"
#include "qgstexturematerial3d.h"
#include "qgsunlitmaterial3d.h"

#include <QColor>
#include <QImage>
#include <QString>

using namespace Qt::StringLiterals;

namespace
{
  QColor centerPixel( const QImage &image )
  {
    return image.pixelColor( image.width() / 2, image.height() / 2 );
  }

  bool colorsClose( const QColor &a, const QColor &b, int tolerance = 4 )
  {
    return std::abs( a.red() - b.red() ) <= tolerance && std::abs( a.green() - b.green() ) <= tolerance && std::abs( a.blue() - b.blue() ) <= tolerance && std::abs( a.alpha() - b.alpha() ) <= tolerance;
  }
} // namespace

/**
 * \ingroup UnitTests
 * Unit tests for the QRhi materials ported after QgsPhongMaterial3D: the unlit material,
 * the unlit textured material, the Gooch material and the textured Phong material.
 *
 * They are tested together because they share all of their scaffolding - one engine, one
 * flat square, one top down camera - and differ only in what they shade it with. What each
 * material has to get right is the same three things: that every baked shader variant it
 * can ask for exists and reads the attributes it claims to, that its uniform struct matches
 * its GLSL block, and that the values it sets actually reach the pixels.
 */
class TestQgsMaterials3D : public QgsTest
{
    Q_OBJECT
  public:
    TestQgsMaterials3D()
      : QgsTest( u"QRhi material tests"_s )
    {}

  private slots:
    void initTestCase();
    void cleanupTestCase();

    void testUnlitVariantsExist();
    void testUnlitUniformBlockLayout();
    void testUnlitSolidColor();
    void testUnlitDataDefined();
    void testUnlitInstanced();

    void testTexturedVariantsExist();
    void testTexturedUniformBlockLayout();
    void testTexturedQuad();
    void testTexturedOpacity();

    void testGoochVariantsExist();
    void testGoochUniformBlockLayout();
    void testGoochTones();
    void testGoochDataDefined();

    void testGlobeUniformBlockLayout();
    void testGlobeShading();

    void testBillboardVariantsExist();
    void testBillboardUniformBlockLayout();
    void testBillboardFixedPixelSize();
    void testBillboardPerspectiveScale();

    void testMeshVariantsExist();
    void testMeshUniformBlockLayout();
    void testMeshSingleColor();
    void testMeshColorRamp();
    void testMeshWireframe();
    void testMeshArrows();

    void testMetalRoughVariantsExist();
    void testMetalRoughUniformBlockLayout();
    void testMetalRoughFeatureFlags();
    void testMetalRoughEmission();
    void testMetalRoughLitBaseColor();
    void testMetalRoughBaseColorMap();
    void testMetalRoughDataDefined();
    void testMetalRoughFlatShading();

    void testPhongTexturedVariantsExist();
    void testPhongTexturedUniformBlockLayout();
    void testPhongTexturedQuad();
    void testPhongTexturedTextureTransform();

  private:
    std::unique_ptr<QgsRhiEngine> makeEngine( QSize size = QSize( 64, 64 ) );

    /**
     * Points the pass's camera straight down at the flat square the tests draw.
     *
     * The square lies in the XY plane from (0,0) to (10,10) with its normal along +Z, so
     * a light along -Z shines straight onto it.
     */
    void setTopDownCamera( QgsRenderPass &pass );

    /**
     * Fills \a buffer with a 10x10 square lying flat in the XY plane, as six vertices of
     * position, normal, texture coordinate and tangent interleaved in that order.
     *
     * \a normal is what every vertex claims its normal is, which is the surface normal
     * unless a test wants to see what happens when it is not.
     */
    void makeQuad( QgsGpuBuffer &buffer, const QVector3D &normal = QVector3D( 0.0f, 0.0f, 1.0f ) );

    /**
     * Builds the mesh geometry the mesh material tests draw: the same flat square, plus
     * a "scalarMagnitude" attribute holding \a magnitude at every vertex.
     */
    void makeMeshGeometry( QgsGpuBuffer &vertexBuffer, QgsGpuBuffer &magnitudeBuffer, QgsGeometry3D &geometry, float magnitude, const QVector3D &normal = QVector3D( 0.0f, 0.0f, 1.0f ) );

    //! Adds one white directional light shining straight down, which fully lights the square
    void setOverheadLight( QgsRenderPass &pass );

    bool skipIfNoPixels() const { return mBackend == QRhi::Null; }

    //! Checks that each of \a variants loads against \a fragment, and reads vertexPosition
    void checkVariantsExist( const QStringList &variants, const QString &fragment );

    QRhi::Implementation mBackend = QRhi::Null;
};

std::unique_ptr<QgsRhiEngine> TestQgsMaterials3D::makeEngine( QSize size )
{
  auto engine = std::make_unique<QgsRhiEngine>();
  if ( !engine->createOffscreen( size, mBackend ) )
    return nullptr;
  return engine;
}

void TestQgsMaterials3D::setTopDownCamera( QgsRenderPass &pass )
{
  const QVector3D eye( 5.0f, 5.0f, 30.0f );

  QMatrix4x4 view;
  view.lookAt( eye, QVector3D( 5.0f, 5.0f, 0.0f ), QVector3D( 0.0f, 1.0f, 0.0f ) );

  QMatrix4x4 projection;
  projection.perspective( 45.0f, 1.0f, 1.0f, 100.0f );

  pass.setCamera( view, projection );
  pass.setEyePosition( eye );
}

void TestQgsMaterials3D::makeQuad( QgsGpuBuffer &buffer, const QVector3D &normal )
{
  const float corners[4][2] = { { 0.0f, 0.0f }, { 10.0f, 0.0f }, { 10.0f, 10.0f }, { 0.0f, 10.0f } };
  const int order[6] = { 0, 1, 2, 0, 2, 3 };

  QVector<float> vertices;
  for ( const int corner : order )
  {
    vertices << corners[corner][0] << corners[corner][1] << 0.0f;
    vertices << normal.x() << normal.y() << normal.z();
    vertices << corners[corner][0] / 10.0f << corners[corner][1] / 10.0f;
    vertices << 1.0f << 0.0f << 0.0f << 1.0f;
  }

  buffer.setData( QByteArray( reinterpret_cast<const char *>( vertices.constData() ), vertices.size() * int( sizeof( float ) ) ) );
}

void TestQgsMaterials3D::makeMeshGeometry( QgsGpuBuffer &vertexBuffer, QgsGpuBuffer &magnitudeBuffer, QgsGeometry3D &geometry, float magnitude, const QVector3D &normal )
{
  makeQuad( vertexBuffer, normal );

  QVector<float> magnitudes( 6, magnitude );
  magnitudeBuffer.setData( QByteArray( reinterpret_cast<const char *>( magnitudes.constData() ), magnitudes.size() * int( sizeof( float ) ) ) );

  const quint32 stride = 12 * sizeof( float );
  geometry.addAttribute( { u"vertexPosition"_s, &vertexBuffer, QRhiVertexInputAttribute::Float3, 0, stride, 0 } );
  geometry.addAttribute( { u"vertexNormal"_s, &vertexBuffer, QRhiVertexInputAttribute::Float3, 3 * sizeof( float ), stride, 0 } );
  geometry.addAttribute( { u"scalarMagnitude"_s, &magnitudeBuffer, QRhiVertexInputAttribute::Float, 0, 0, 0 } );
  geometry.setVertexCount( 6 );
}

void TestQgsMaterials3D::setOverheadLight( QgsRenderPass &pass )
{
  QgsLightUniforms lights;
  lights.addDirectionalLight( QVector3D( 0.0f, 0.0f, -1.0f ), QColor( 255, 255, 255 ), 1.0f );
  pass.setLights( lights );
}

void TestQgsMaterials3D::checkVariantsExist( const QStringList &variants, const QString &fragment )
{
  for ( const QString &variant : variants )
  {
    const std::shared_ptr<QgsRhiShader> shader = QgsRhiShader::load( u":/shaders/rhi/%1.vert.qsb"_s.arg( variant ), fragment );
    QVERIFY2( shader, variant.toUtf8() );

    // whatever else it reads, every variant still draws from a position attribute
    QCOMPARE( shader->inputLocation( u"vertexPosition"_s ), 0 );

    // and its two stages have to agree about every varying by name as well as by
    // location, or it links on every backend except OpenGL
    QString error;
    QVERIFY2( shader->checkStageInterface( &error ), u"%1: %2"_s.arg( variant, error ).toUtf8() );
  }
}

void TestQgsMaterials3D::initTestCase()
{
  mBackend = QgsRhiEngine::defaultBackend();

  QgsRhiEngine probe;
  if ( !probe.createOffscreen( QSize( 4, 4 ), mBackend ) )
    QSKIP( "No usable QRhi device for the default backend" );
}

void TestQgsMaterials3D::cleanupTestCase()
{
  QgsRhiShader::clearCache();
  QgsApplication::exitQgis();
}

//
// QgsUnlitMaterial3D
//

void TestQgsMaterials3D::testUnlitVariantsExist()
{
  // a missing variant would otherwise show up much later as a material that draws nothing
  const QString fragment = u":/shaders/rhi/unlit.frag.qsb"_s;
  checkVariantsExist( { u"unlit"_s, u"unlit_datadefined"_s, u"unlit_vertexcolor"_s, u"unlit_instanced"_s, u"unlit_instanced_scale"_s, u"unlit_instanced_rotation"_s, u"unlit_instanced_scale_rotation"_s }, fragment );

  const std::shared_ptr<QgsRhiShader> dataDefined = QgsRhiShader::load( u":/shaders/rhi/unlit_datadefined.vert.qsb"_s, fragment );
  QCOMPARE( dataDefined->inputLocation( u"dataDefinedBaseColor"_s ), 2 );

  // the plain variant must not read the optional attributes, otherwise every geometry
  // drawn with it would have to supply them
  const std::shared_ptr<QgsRhiShader> plain = QgsRhiShader::load( u":/shaders/rhi/unlit.vert.qsb"_s, fragment );
  QCOMPARE( plain->inputLocation( u"dataDefinedBaseColor"_s ), -1 );
  QCOMPARE( plain->inputLocation( u"instanceTranslation"_s ), -1 );

  const std::shared_ptr<QgsRhiShader> instanced = QgsRhiShader::load( u":/shaders/rhi/unlit_instanced_scale_rotation.vert.qsb"_s, fragment );
  QCOMPARE( instanced->inputLocation( u"instanceTranslation"_s ), 3 );
  QCOMPARE( instanced->inputLocation( u"instanceScale"_s ), 4 );
  QCOMPARE( instanced->inputLocation( u"instanceRotation"_s ), 5 );

  // an unlit material is not lit, so it must not be pulling in the light block at all
  QCOMPARE( plain->uniformBlockSize( QgsRhiBinding::Lights ), -1 );
}

void TestQgsMaterials3D::testUnlitUniformBlockLayout()
{
  const std::shared_ptr<QgsRhiShader> shader = QgsRhiShader::load( u":/shaders/rhi/unlit.vert.qsb"_s, u":/shaders/rhi/unlit.frag.qsb"_s );
  QVERIFY( shader );

  QCOMPARE( shader->uniformBlockSize( QgsRhiBinding::Material ), int( sizeof( QgsUnlitUniforms ) ) );

  QString error;
  const QVector<QgsUniformMember> members {
    { u"materialColor"_s, offsetof( QgsUnlitUniforms, materialColor ), 16 },
    { u"meshMatrix"_s, offsetof( QgsUnlitUniforms, meshMatrix ), 64 },
    { u"symbolRotation"_s, offsetof( QgsUnlitUniforms, symbolRotation ), 16 },
    { u"symbolScale"_s, offsetof( QgsUnlitUniforms, symbolScale ), 16 },
  };
  QVERIFY2( shader->checkUniformBlockLayout( QgsRhiBinding::Material, members, &error ), error.toUtf8() );
}

void TestQgsMaterials3D::testUnlitSolidColor()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  const std::unique_ptr<QgsRhiEngine> engine = makeEngine();
  QVERIFY( engine );
  QgsGraphicsPipelineCache cache;

  const float vertices[] = { 0.0f, 0.0f, 0.0f, 10.0f, 0.0f, 0.0f, 10.0f, 10.0f, 0.0f, 0.0f, 0.0f, 0.0f, 10.0f, 10.0f, 0.0f, 0.0f, 10.0f, 0.0f };
  QgsGpuBuffer vertexBuffer( QgsGpuBuffer::Role::Vertex );
  vertexBuffer.setData( QByteArray( reinterpret_cast<const char *>( vertices ), sizeof( vertices ) ) );

  QgsGeometry3D geometry;
  geometry.addAttribute( { u"vertexPosition"_s, &vertexBuffer, QRhiVertexInputAttribute::Float3, 0, 0, 0 } );
  geometry.setVertexCount( 6 );

  QgsUnlitMaterial3D material;
  QVERIFY( !material.isDataDefined() );
  QVERIFY( !material.isInstanced() );
  material.setCullMode( QRhiGraphicsPipeline::None );
  material.setColor( QColor( 255, 0, 0 ) );

  QgsRenderPass pass( u"unlit"_s );
  setTopDownCamera( pass );
  pass.setClearColor( QColor( 0, 0, 255 ) );

  // a light pointing away from the square, which a lit material would darken it with and
  // this one must ignore completely
  QgsLightUniforms lights;
  lights.addDirectionalLight( QVector3D( 0.0f, 0.0f, 1.0f ), QColor( 255, 255, 255 ), 1.0f );
  pass.setLights( lights );

  QgsRenderCommand command;
  command.geometry = &geometry;
  command.material = &material;
  pass.addCommand( command );

  engine->setReadbackEnabled( true );
  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();

  const QImage image = engine->readbackImage();
  QVERIFY2( colorsClose( centerPixel( image ), QColor( 255, 0, 0 ) ), centerPixel( image ).name().toUtf8() );
  QVERIFY2( colorsClose( image.pixelColor( 1, 1 ), QColor( 0, 0, 255 ) ), "the background outside the square was overdrawn" );

  pass.releaseGpuResources();
  material.releaseGpuResources();
  cache.clear();
}

void TestQgsMaterials3D::testUnlitDataDefined()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  const std::unique_ptr<QgsRhiEngine> engine = makeEngine();
  QVERIFY( engine );
  QgsGraphicsPipelineCache cache;

  // position and per-vertex colour interleaved, every vertex green so that the
  // interpolated centre is exactly what was written
  const float vertices[] = { 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 10.0f, 0.0f,  0.0f, 0.0f, 1.0f, 0.0f, 10.0f, 10.0f, 0.0f, 0.0f, 1.0f, 0.0f,
                             0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 10.0f, 10.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,  10.0f, 0.0f, 0.0f, 1.0f, 0.0f };
  QgsGpuBuffer vertexBuffer( QgsGpuBuffer::Role::Vertex );
  vertexBuffer.setData( QByteArray( reinterpret_cast<const char *>( vertices ), sizeof( vertices ) ) );

  const quint32 stride = 6 * sizeof( float );
  QgsGeometry3D geometry;
  geometry.addAttribute( { u"vertexPosition"_s, &vertexBuffer, QRhiVertexInputAttribute::Float3, 0, stride, 0 } );
  geometry.addAttribute( { u"dataDefinedBaseColor"_s, &vertexBuffer, QRhiVertexInputAttribute::Float3, 3 * sizeof( float ), stride, 0 } );
  geometry.setVertexCount( 6 );

  QgsUnlitMaterial3D material( true );
  QVERIFY( material.isDataDefined() );
  material.setCullMode( QRhiGraphicsPipeline::None );

  // the uniform colour must be ignored entirely by this variant
  material.setColor( QColor( 255, 0, 0 ) );

  QgsRenderPass pass( u"unlit data defined"_s );
  setTopDownCamera( pass );
  pass.setClearColor( QColor( 0, 0, 255 ) );

  QgsRenderCommand command;
  command.geometry = &geometry;
  command.material = &material;
  pass.addCommand( command );

  engine->setReadbackEnabled( true );
  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();

  QVERIFY2( colorsClose( centerPixel( engine->readbackImage() ), QColor( 0, 255, 0 ) ), centerPixel( engine->readbackImage() ).name().toUtf8() );

  pass.releaseGpuResources();
  material.releaseGpuResources();
  cache.clear();
}

void TestQgsMaterials3D::testUnlitInstanced()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  const std::unique_ptr<QgsRhiEngine> engine = makeEngine();
  QVERIFY( engine );
  if ( !engine->rhi()->isFeatureSupported( QRhi::Instancing ) )
    QSKIP( "The device does not support instanced drawing" );

  QgsGraphicsPipelineCache cache;

  // a small quad in the XY plane, drawn once per instance
  const float vertices[] = { -1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f, -1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f, -1.0f, 1.0f, 0.0f };
  QgsGpuBuffer vertexBuffer( QgsGpuBuffer::Role::Vertex );
  vertexBuffer.setData( QByteArray( reinterpret_cast<const char *>( vertices ), sizeof( vertices ) ) );

  const float translations[] = { 2.0f, 2.0f, 0.0f, 8.0f, 8.0f, 0.0f };
  QgsGpuBuffer instanceBuffer( QgsGpuBuffer::Role::Vertex );
  instanceBuffer.setData( QByteArray( reinterpret_cast<const char *>( translations ), sizeof( translations ) ) );

  QgsGeometry3D geometry;
  geometry.addAttribute( { u"vertexPosition"_s, &vertexBuffer, QRhiVertexInputAttribute::Float3, 0, 0, 0 } );
  geometry.addAttribute( { u"instanceTranslation"_s, &instanceBuffer, QRhiVertexInputAttribute::Float3, 0, 0, 1 } );
  geometry.setVertexCount( 6 );

  QgsUnlitMaterial3D material( false, true );
  QVERIFY( material.isInstanced() );
  material.setCullMode( QRhiGraphicsPipeline::None );
  material.setColor( QColor( 255, 0, 0 ) );

  QgsRenderPass pass( u"unlit instanced"_s );
  setTopDownCamera( pass );
  pass.setClearColor( Qt::black );

  QgsRenderCommand command;
  command.geometry = &geometry;
  command.material = &material;
  command.instanceCount = 2;
  pass.addCommand( command );

  engine->setReadbackEnabled( true );
  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();

  const QImage image = engine->readbackImage();

  int redPixels = 0;
  for ( int y = 0; y < image.height(); ++y )
  {
    for ( int x = 0; x < image.width(); ++x )
    {
      if ( image.pixelColor( x, y ).red() > 200 )
        ++redPixels;
    }
  }
  QVERIFY2( redPixels > 0, "neither instance was drawn" );

  // the two instances sit on opposite sides of the centre, which stays empty
  QVERIFY2( colorsClose( centerPixel( image ), QColor( 0, 0, 0 ) ), "nothing should sit at the centre" );

  pass.releaseGpuResources();
  material.releaseGpuResources();
  cache.clear();
}

//
// QgsTextureMaterial3D
//

void TestQgsMaterials3D::testTexturedVariantsExist()
{
  const QString fragment = u":/shaders/rhi/textured.frag.qsb"_s;
  checkVariantsExist( { u"textured"_s, u"textured_instanced"_s, u"textured_instanced_scale"_s, u"textured_instanced_rotation"_s, u"textured_instanced_scale_rotation"_s }, fragment );

  const std::shared_ptr<QgsRhiShader> plain = QgsRhiShader::load( u":/shaders/rhi/textured.vert.qsb"_s, fragment );
  QCOMPARE( plain->inputLocation( u"vertexTexCoord"_s ), 1 );
  QCOMPARE( plain->inputLocation( u"instanceTranslation"_s ), -1 );

  const std::shared_ptr<QgsRhiShader> instanced = QgsRhiShader::load( u":/shaders/rhi/textured_instanced_scale_rotation.vert.qsb"_s, fragment );
  QCOMPARE( instanced->inputLocation( u"instanceTranslation"_s ), 2 );
  QCOMPARE( instanced->inputLocation( u"instanceScale"_s ), 3 );
  QCOMPARE( instanced->inputLocation( u"instanceRotation"_s ), 4 );

  // the sampler has to sit where the material binds it
  const QVector<QShaderDescription::InOutVariable> samplers = plain->samplers();
  QCOMPARE( samplers.size(), 1 );
  QCOMPARE( samplers.at( 0 ).binding, QgsTextureMaterial3D::DiffuseTextureBinding );
}

void TestQgsMaterials3D::testTexturedUniformBlockLayout()
{
  const std::shared_ptr<QgsRhiShader> shader = QgsRhiShader::load( u":/shaders/rhi/textured.vert.qsb"_s, u":/shaders/rhi/textured.frag.qsb"_s );
  QVERIFY( shader );

  QCOMPARE( shader->uniformBlockSize( QgsRhiBinding::Material ), int( sizeof( QgsTexturedUniforms ) ) );

  QString error;
  const QVector<QgsUniformMember> members {
    { u"opacity"_s, offsetof( QgsTexturedUniforms, opacity ), 4 },
    { u"meshMatrix"_s, offsetof( QgsTexturedUniforms, meshMatrix ), 64 },
    { u"symbolRotation"_s, offsetof( QgsTexturedUniforms, symbolRotation ), 16 },
    { u"symbolScale"_s, offsetof( QgsTexturedUniforms, symbolScale ), 16 },
  };
  QVERIFY2( shader->checkUniformBlockLayout( QgsRhiBinding::Material, members, &error ), error.toUtf8() );
}

void TestQgsMaterials3D::testTexturedQuad()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  const std::unique_ptr<QgsRhiEngine> engine = makeEngine();
  QVERIFY( engine );
  QgsGraphicsPipelineCache cache;

  // position and texture coordinate interleaved
  const float vertices[] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 10.0f, 0.0f,  0.0f, 1.0f, 0.0f, 10.0f, 10.0f, 0.0f, 1.0f, 1.0f,
                             0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 10.0f, 10.0f, 0.0f, 1.0f, 1.0f, 0.0f,  10.0f, 0.0f, 0.0f, 1.0f };
  QgsGpuBuffer vertexBuffer( QgsGpuBuffer::Role::Vertex );
  vertexBuffer.setData( QByteArray( reinterpret_cast<const char *>( vertices ), sizeof( vertices ) ) );

  const quint32 stride = 5 * sizeof( float );
  QgsGeometry3D geometry;
  geometry.addAttribute( { u"vertexPosition"_s, &vertexBuffer, QRhiVertexInputAttribute::Float3, 0, stride, 0 } );
  geometry.addAttribute( { u"vertexTexCoord"_s, &vertexBuffer, QRhiVertexInputAttribute::Float2, 3 * sizeof( float ), stride, 0 } );
  geometry.setVertexCount( 6 );

  QImage source( 4, 4, QImage::Format_RGBA8888 );
  source.fill( QColor( 20, 180, 90 ) );
  QgsGpuTexture texture;
  texture.setImage( source );

  QgsTextureMaterial3D material;
  QVERIFY( !material.isInstanced() );
  material.setCullMode( QRhiGraphicsPipeline::None );
  material.setDiffuseTexture( &texture );
  QCOMPARE( material.diffuseTexture(), &texture );

  QgsRenderPass pass( u"textured"_s );
  setTopDownCamera( pass );
  pass.setClearColor( QColor( 0, 0, 255 ) );

  QgsRenderCommand command;
  command.geometry = &geometry;
  command.material = &material;
  pass.addCommand( command );

  engine->setReadbackEnabled( true );
  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();

  // the texture is not colour converted on the way in, so its own values come straight out
  QVERIFY2( colorsClose( centerPixel( engine->readbackImage() ), QColor( 20, 180, 90 ) ), centerPixel( engine->readbackImage() ).name().toUtf8() );

  pass.releaseGpuResources();
  material.releaseGpuResources();
  texture.releaseGpuResources();
  cache.clear();
}

void TestQgsMaterials3D::testTexturedOpacity()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  const std::unique_ptr<QgsRhiEngine> engine = makeEngine();
  QVERIFY( engine );
  QgsGraphicsPipelineCache cache;

  const float vertices[] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 10.0f, 0.0f,  0.0f, 1.0f, 0.0f, 10.0f, 10.0f, 0.0f, 1.0f, 1.0f,
                             0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 10.0f, 10.0f, 0.0f, 1.0f, 1.0f, 0.0f,  10.0f, 0.0f, 0.0f, 1.0f };
  QgsGpuBuffer vertexBuffer( QgsGpuBuffer::Role::Vertex );
  vertexBuffer.setData( QByteArray( reinterpret_cast<const char *>( vertices ), sizeof( vertices ) ) );

  const quint32 stride = 5 * sizeof( float );
  QgsGeometry3D geometry;
  geometry.addAttribute( { u"vertexPosition"_s, &vertexBuffer, QRhiVertexInputAttribute::Float3, 0, stride, 0 } );
  geometry.addAttribute( { u"vertexTexCoord"_s, &vertexBuffer, QRhiVertexInputAttribute::Float2, 3 * sizeof( float ), stride, 0 } );
  geometry.setVertexCount( 6 );

  QImage source( 4, 4, QImage::Format_RGBA8888 );
  source.fill( QColor( 255, 0, 0 ) );
  QgsGpuTexture texture;
  texture.setImage( source );

  QgsTextureMaterial3D material;
  material.setCullMode( QRhiGraphicsPipeline::None );
  material.setDiffuseTexture( &texture );
  material.setOpacity( 0.5f );

  QRhiGraphicsPipeline::TargetBlend blend;
  blend.enable = true;
  blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
  blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
  material.setBlend( blend );
  material.setDepthWrite( false );

  QgsRenderPass pass( u"textured opacity"_s );
  setTopDownCamera( pass );
  pass.setClearColor( QColor( 0, 0, 255 ) );

  QgsRenderCommand command;
  command.geometry = &geometry;
  command.material = &material;
  pass.addCommand( command );

  engine->setReadbackEnabled( true );
  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();

  // half red over blue
  QVERIFY2( colorsClose( centerPixel( engine->readbackImage() ), QColor( 128, 0, 128 ), 3 ), centerPixel( engine->readbackImage() ).name().toUtf8() );

  pass.releaseGpuResources();
  material.releaseGpuResources();
  texture.releaseGpuResources();
  cache.clear();
}

//
// QgsGoochMaterial3D
//

void TestQgsMaterials3D::testGoochVariantsExist()
{
  const QString fragment = u":/shaders/rhi/gooch.frag.qsb"_s;
  checkVariantsExist( { u"gooch"_s, u"gooch_datadefined"_s, u"gooch_instanced"_s, u"gooch_instanced_scale"_s, u"gooch_instanced_rotation"_s, u"gooch_instanced_scale_rotation"_s }, fragment );

  const std::shared_ptr<QgsRhiShader> dataDefined = QgsRhiShader::load( u":/shaders/rhi/gooch_datadefined.vert.qsb"_s, u":/shaders/rhi/gooch_datadefined.frag.qsb"_s );
  QVERIFY( dataDefined );
  QCOMPARE( dataDefined->inputLocation( u"dataDefinedDiffuseColor"_s ), 2 );
  QCOMPARE( dataDefined->inputLocation( u"dataDefinedWarmColor"_s ), 3 );
  QCOMPARE( dataDefined->inputLocation( u"dataDefinedCoolColor"_s ), 4 );
  QCOMPARE( dataDefined->inputLocation( u"dataDefinedSpecularColor"_s ), 5 );

  const std::shared_ptr<QgsRhiShader> plain = QgsRhiShader::load( u":/shaders/rhi/gooch.vert.qsb"_s, fragment );
  QCOMPARE( plain->inputLocation( u"vertexNormal"_s ), 1 );
  QCOMPARE( plain->inputLocation( u"dataDefinedWarmColor"_s ), -1 );
  QCOMPARE( plain->inputLocation( u"instanceTranslation"_s ), -1 );

  const std::shared_ptr<QgsRhiShader> instanced = QgsRhiShader::load( u":/shaders/rhi/gooch_instanced_scale_rotation.vert.qsb"_s, fragment );
  QCOMPARE( instanced->inputLocation( u"instanceTranslation"_s ), 6 );
  QCOMPARE( instanced->inputLocation( u"instanceScale"_s ), 7 );
  QCOMPARE( instanced->inputLocation( u"instanceRotation"_s ), 8 );
}

void TestQgsMaterials3D::testGoochUniformBlockLayout()
{
  const std::shared_ptr<QgsRhiShader> shader = QgsRhiShader::load( u":/shaders/rhi/gooch.vert.qsb"_s, u":/shaders/rhi/gooch.frag.qsb"_s );
  QVERIFY( shader );

  QCOMPARE( shader->uniformBlockSize( QgsRhiBinding::Material ), int( sizeof( QgsGoochUniforms ) ) );

  QString error;
  const QVector<QgsUniformMember> members {
    { u"diffuseColor"_s, offsetof( QgsGoochUniforms, diffuseColor ), 16 },
    { u"specularColor"_s, offsetof( QgsGoochUniforms, specularColor ), 16 },
    { u"warmColor"_s, offsetof( QgsGoochUniforms, warmColor ), 16 },
    { u"coolColor"_s, offsetof( QgsGoochUniforms, coolColor ), 16 },
    { u"shininess"_s, offsetof( QgsGoochUniforms, shininess ), 4 },
    { u"alpha"_s, offsetof( QgsGoochUniforms, alpha ), 4 },
    { u"beta"_s, offsetof( QgsGoochUniforms, beta ), 4 },
    { u"meshMatrix"_s, offsetof( QgsGoochUniforms, meshMatrix ), 64 },
    { u"meshNormalMatrix"_s, offsetof( QgsGoochUniforms, meshNormalMatrix ), 48 },
    { u"symbolRotation"_s, offsetof( QgsGoochUniforms, symbolRotation ), 16 },
    { u"symbolScale"_s, offsetof( QgsGoochUniforms, symbolScale ), 16 },
  };
  QVERIFY2( shader->checkUniformBlockLayout( QgsRhiBinding::Material, members, &error ), error.toUtf8() );
}

void TestQgsMaterials3D::testGoochTones()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  const std::unique_ptr<QgsRhiEngine> engine = makeEngine();
  QVERIFY( engine );
  QgsGraphicsPipelineCache cache;

  // position and normal interleaved, the square lying flat with its normal along +Z
  const float vertices[] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 10.0f, 0.0f,  0.0f, 0.0f, 0.0f, 1.0f, 10.0f, 10.0f, 0.0f, 0.0f, 0.0f, 1.0f,
                             0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 10.0f, 10.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,  10.0f, 0.0f, 0.0f, 0.0f, 1.0f };
  QgsGpuBuffer vertexBuffer( QgsGpuBuffer::Role::Vertex );
  vertexBuffer.setData( QByteArray( reinterpret_cast<const char *>( vertices ), sizeof( vertices ) ) );

  const quint32 stride = 6 * sizeof( float );
  QgsGeometry3D geometry;
  geometry.addAttribute( { u"vertexPosition"_s, &vertexBuffer, QRhiVertexInputAttribute::Float3, 0, stride, 0 } );
  geometry.addAttribute( { u"vertexNormal"_s, &vertexBuffer, QRhiVertexInputAttribute::Float3, 3 * sizeof( float ), stride, 0 } );
  geometry.setVertexCount( 6 );

  // with a black diffuse colour the two tones are used as they are, so what comes out is
  // exactly the warm or the cool colour rather than a blend of them with the diffuse
  QgsGoochMaterial3D material;
  QVERIFY( !material.isDataDefined() );
  material.setCullMode( QRhiGraphicsPipeline::None );
  material.setDiffuse( Qt::black );
  material.setSpecular( Qt::black );
  material.setWarm( QColor( 255, 0, 0 ) );
  material.setCool( QColor( 0, 0, 255 ) );

  QgsRenderPass pass( u"gooch"_s );
  setTopDownCamera( pass );
  pass.setClearColor( QColor( 0, 255, 0 ) );

  QgsRenderCommand command;
  command.geometry = &geometry;
  command.material = &material;
  pass.addCommand( command );

  engine->setReadbackEnabled( true );

  // a light shining onto the face gives the warm tone
  setOverheadLight( pass );
  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();
  QVERIFY2( colorsClose( centerPixel( engine->readbackImage() ), QColor( 255, 0, 0 ) ), centerPixel( engine->readbackImage() ).name().toUtf8() );

  // and one shining away from it gives the cool tone, where a Phong surface would be black.
  // This is the whole point of the model, so it is worth pinning down.
  QgsLightUniforms lights;
  lights.addDirectionalLight( QVector3D( 0.0f, 0.0f, 1.0f ), QColor( 255, 255, 255 ), 1.0f );
  pass.setLights( lights );

  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();
  QVERIFY2( colorsClose( centerPixel( engine->readbackImage() ), QColor( 0, 0, 255 ) ), centerPixel( engine->readbackImage() ).name().toUtf8() );

  // beta mixes the diffuse colour into the warm tone, alpha into the cool one
  material.setWarm( Qt::black );
  material.setDiffuse( QColor( 255, 255, 255 ) );
  material.setBeta( 0.5f );
  setOverheadLight( pass );

  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();
  QVERIFY2( colorsClose( centerPixel( engine->readbackImage() ), QColor( 128, 128, 128 ), 2 ), centerPixel( engine->readbackImage() ).name().toUtf8() );

  pass.releaseGpuResources();
  material.releaseGpuResources();
  cache.clear();
}

void TestQgsMaterials3D::testGoochDataDefined()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  const std::unique_ptr<QgsRhiEngine> engine = makeEngine();
  QVERIFY( engine );
  QgsGraphicsPipelineCache cache;

  const float vertices[] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 10.0f, 0.0f,  0.0f, 0.0f, 0.0f, 1.0f, 10.0f, 10.0f, 0.0f, 0.0f, 0.0f, 1.0f,
                             0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 10.0f, 10.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,  10.0f, 0.0f, 0.0f, 0.0f, 1.0f };
  QgsGpuBuffer vertexBuffer( QgsGpuBuffer::Role::Vertex );
  vertexBuffer.setData( QByteArray( reinterpret_cast<const char *>( vertices ), sizeof( vertices ) ) );

  // diffuse black, warm green, cool black and specular black, repeated for all six vertices
  QVector<float> colors;
  for ( int i = 0; i < 6; ++i )
    colors
      << 0.0f
      << 0.0f
      << 0.0f // diffuse
      << 0.0f
      << 1.0f
      << 0.0f // warm
      << 0.0f
      << 0.0f
      << 0.0f // cool
      << 0.0f
      << 0.0f
      << 0.0f; // specular

  QgsGpuBuffer colorBuffer( QgsGpuBuffer::Role::Vertex );
  colorBuffer.setData( QByteArray( reinterpret_cast<const char *>( colors.constData() ), colors.size() * int( sizeof( float ) ) ) );

  const quint32 stride = 6 * sizeof( float );
  const quint32 colorStride = 12 * sizeof( float );

  QgsGeometry3D geometry;
  geometry.addAttribute( { u"vertexPosition"_s, &vertexBuffer, QRhiVertexInputAttribute::Float3, 0, stride, 0 } );
  geometry.addAttribute( { u"vertexNormal"_s, &vertexBuffer, QRhiVertexInputAttribute::Float3, 3 * sizeof( float ), stride, 0 } );
  geometry.addAttribute( { u"dataDefinedDiffuseColor"_s, &colorBuffer, QRhiVertexInputAttribute::Float3, 0, colorStride, 0 } );
  geometry.addAttribute( { u"dataDefinedWarmColor"_s, &colorBuffer, QRhiVertexInputAttribute::Float3, 3 * sizeof( float ), colorStride, 0 } );
  geometry.addAttribute( { u"dataDefinedCoolColor"_s, &colorBuffer, QRhiVertexInputAttribute::Float3, 6 * sizeof( float ), colorStride, 0 } );
  geometry.addAttribute( { u"dataDefinedSpecularColor"_s, &colorBuffer, QRhiVertexInputAttribute::Float3, 9 * sizeof( float ), colorStride, 0 } );
  geometry.setVertexCount( 6 );

  QgsGoochMaterial3D material( true );
  QVERIFY( material.isDataDefined() );
  material.setCullMode( QRhiGraphicsPipeline::None );

  // the uniform colours must be ignored entirely by this variant
  material.setWarm( QColor( 255, 0, 0 ) );
  material.setCool( QColor( 255, 0, 0 ) );
  material.setDiffuse( QColor( 255, 0, 0 ) );

  QgsRenderPass pass( u"gooch data defined"_s );
  setTopDownCamera( pass );
  pass.setClearColor( QColor( 0, 0, 255 ) );
  setOverheadLight( pass );

  QgsRenderCommand command;
  command.geometry = &geometry;
  command.material = &material;
  pass.addCommand( command );

  engine->setReadbackEnabled( true );
  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();

  QVERIFY2( colorsClose( centerPixel( engine->readbackImage() ), QColor( 0, 255, 0 ) ), centerPixel( engine->readbackImage() ).name().toUtf8() );

  pass.releaseGpuResources();
  material.releaseGpuResources();
  cache.clear();
}

//
// QgsGlobeMaterial3D
//

void TestQgsMaterials3D::testGlobeUniformBlockLayout()
{
  const std::shared_ptr<QgsRhiShader> shader = QgsRhiShader::load( u":/shaders/rhi/globe.vert.qsb"_s, u":/shaders/rhi/globe.frag.qsb"_s );
  QVERIFY( shader );

  QCOMPARE( shader->uniformBlockSize( QgsRhiBinding::Material ), int( sizeof( QgsGlobeUniforms ) ) );
  QCOMPARE( shader->inputLocation( u"vertexNormal"_s ), 1 );
  QCOMPARE( shader->inputLocation( u"vertexTexCoord"_s ), 2 );

  QString error;
  const QVector<QgsUniformMember> members {
    { u"texCoordScale"_s, offsetof( QgsGlobeUniforms, texCoordScale ), 4 },
  };
  QVERIFY2( shader->checkUniformBlockLayout( QgsRhiBinding::Material, members, &error ), error.toUtf8() );
  QVERIFY2( shader->checkStageInterface( &error ), error.toUtf8() );

  const QVector<QShaderDescription::InOutVariable> samplers = shader->samplers();
  QCOMPARE( samplers.size(), 1 );
  QCOMPARE( samplers.at( 0 ).binding, QgsGlobeMaterial3D::DiffuseTextureBinding );
}

void TestQgsMaterials3D::testGlobeShading()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  const std::unique_ptr<QgsRhiEngine> engine = makeEngine();
  QVERIFY( engine );
  QgsGraphicsPipelineCache cache;

  QgsGpuBuffer vertexBuffer( QgsGpuBuffer::Role::Vertex );
  makeQuad( vertexBuffer );

  const quint32 stride = 12 * sizeof( float );
  QgsGeometry3D geometry;
  geometry.addAttribute( { u"vertexPosition"_s, &vertexBuffer, QRhiVertexInputAttribute::Float3, 0, stride, 0 } );
  geometry.addAttribute( { u"vertexNormal"_s, &vertexBuffer, QRhiVertexInputAttribute::Float3, 3 * sizeof( float ), stride, 0 } );
  geometry.addAttribute( { u"vertexTexCoord"_s, &vertexBuffer, QRhiVertexInputAttribute::Float2, 6 * sizeof( float ), stride, 0 } );
  geometry.setVertexCount( 6 );

  QImage source( 4, 4, QImage::Format_RGBA8888 );
  source.fill( QColor( 200, 200, 200 ) );
  QgsGpuTexture texture;
  texture.setImage( source );

  QgsGlobeMaterial3D material;
  material.setCullMode( QRhiGraphicsPipeline::None );
  material.setDiffuseTexture( &texture );
  QCOMPARE( material.diffuseTexture(), &texture );

  QgsRenderPass pass( u"globe"_s );
  setTopDownCamera( pass );
  pass.setClearColor( QColor( 0, 0, 255 ) );

  QgsRenderCommand command;
  command.geometry = &geometry;
  command.material = &material;
  pass.addCommand( command );

  // the camera is 30 units away, far inside the distance at which the edge darkening
  // starts, so the texture comes through at the shader's unshaded 30% floor plus the
  // full 70% a surface facing the camera keeps
  engine->setReadbackEnabled( true );
  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();
  QVERIFY2( colorsClose( centerPixel( engine->readbackImage() ), QColor( 200, 200, 200 ) ), centerPixel( engine->readbackImage() ).name().toUtf8() );

  // now blow the same square up to globe scale and back the camera off past the distance
  // at which the shader starts darkening. Facing the camera head on it still keeps all of
  // its colour, which is the half of the shading that depends on the angle.
  const float scale = 1.0e5f;
  QMatrix4x4 worldTransform;
  worldTransform.scale( scale );

  QMatrix4x4 view;
  const QVector3D farEye( 5.0f * scale, 5.0f * scale, 2.0e6f );
  view.lookAt( farEye, QVector3D( 5.0f * scale, 5.0f * scale, 0.0f ), QVector3D( 0.0f, 1.0f, 0.0f ) );
  QMatrix4x4 projection;
  projection.perspective( 45.0f, 1.0f, 1.0e3f, 1.0e7f );
  pass.setCamera( view, projection );
  pass.setEyePosition( farEye );

  pass.clearCommands();
  command.worldTransform = worldTransform;
  pass.addCommand( command );

  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();
  QVERIFY2( colorsClose( centerPixel( engine->readbackImage() ), QColor( 200, 200, 200 ) ), centerPixel( engine->readbackImage() ).name().toUtf8() );

  // and one turned side on to it is darkened all the way to the shader's 30% floor. This
  // is also what says the fragment shader reads the camera out of the view block's
  // inverse view matrix, where the Qt3D material had a uniform of its own for it.
  QgsGpuBuffer sideBuffer( QgsGpuBuffer::Role::Vertex );
  makeQuad( sideBuffer, QVector3D( 0.0f, 1.0f, 0.0f ) );

  QgsGeometry3D sideGeometry;
  sideGeometry.addAttribute( { u"vertexPosition"_s, &sideBuffer, QRhiVertexInputAttribute::Float3, 0, stride, 0 } );
  sideGeometry.addAttribute( { u"vertexNormal"_s, &sideBuffer, QRhiVertexInputAttribute::Float3, 3 * sizeof( float ), stride, 0 } );
  sideGeometry.addAttribute( { u"vertexTexCoord"_s, &sideBuffer, QRhiVertexInputAttribute::Float2, 6 * sizeof( float ), stride, 0 } );
  sideGeometry.setVertexCount( 6 );

  pass.clearCommands();
  command.geometry = &sideGeometry;
  pass.addCommand( command );

  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();
  // the shader scales the sampled colour as a whole, alpha included, exactly as the Qt3D
  // one did, so the floor shows up in the alpha channel too
  QVERIFY2( colorsClose( centerPixel( engine->readbackImage() ), QColor( 60, 60, 60, 77 ), 2 ), centerPixel( engine->readbackImage() ).name( QColor::HexArgb ).toUtf8() );

  pass.releaseGpuResources();
  material.releaseGpuResources();
  texture.releaseGpuResources();
  cache.clear();
}

//
// QgsBillboardMaterial3D
//

void TestQgsMaterials3D::testBillboardVariantsExist()
{
  using Attribute = QgsBillboardMaterial3D::InstanceAttribute;
  using Attributes = QgsBillboardMaterial3D::InstanceAttributes;

  const QString fragment = u":/shaders/rhi/billboard.frag.qsb"_s;

  // all eight combinations of the three per-instance attributes, because a shader input
  // the geometry does not supply makes the whole draw fail rather than go unused
  for ( int bits = 0; bits < 8; ++bits )
  {
    const Attributes attributes( bits );
    const QString variant = QgsBillboardMaterial3D::shaderVariant( attributes );
    const QString path = u":/shaders/rhi/billboard%1.vert.qsb"_s.arg( variant );

    const std::shared_ptr<QgsRhiShader> shader = QgsRhiShader::load( path, fragment );
    QVERIFY2( shader, path.toUtf8() );

    QCOMPARE( shader->inputLocation( u"vertexPosition"_s ), 0 );
    QCOMPARE( shader->inputLocation( u"instancePosition"_s ), 1 );
    QCOMPARE( shader->inputLocation( u"instanceSize"_s ) >= 0, attributes.testFlag( Attribute::Size ) );
    QCOMPARE( shader->inputLocation( u"atlasOffset"_s ) >= 0, attributes.testFlag( Attribute::TextureData ) );
    QCOMPARE( shader->inputLocation( u"atlasSize"_s ) >= 0, attributes.testFlag( Attribute::TextureData ) );
    QCOMPARE( shader->inputLocation( u"pixelOffset"_s ) >= 0, attributes.testFlag( Attribute::PixelOffsets ) );

    QString error;
    QVERIFY2( shader->checkStageInterface( &error ), error.toUtf8() );
  }

  const std::shared_ptr<QgsRhiShader> plain = QgsRhiShader::load( u":/shaders/rhi/billboard.vert.qsb"_s, fragment );
  const QVector<QShaderDescription::InOutVariable> samplers = plain->samplers();
  QCOMPARE( samplers.size(), 1 );
  QCOMPARE( samplers.at( 0 ).binding, QgsBillboardMaterial3D::SpriteTextureBinding );
}

void TestQgsMaterials3D::testBillboardUniformBlockLayout()
{
  const std::shared_ptr<QgsRhiShader> shader = QgsRhiShader::load( u":/shaders/rhi/billboard.vert.qsb"_s, u":/shaders/rhi/billboard.frag.qsb"_s );
  QVERIFY( shader );

  QCOMPARE( shader->uniformBlockSize( QgsRhiBinding::Material ), int( sizeof( QgsBillboardUniforms ) ) );

  QString error;
  const QVector<QgsUniformMember> members {
    { u"billboardSize"_s, offsetof( QgsBillboardUniforms, billboardSize ), 8 },
    { u"verticalOffset"_s, offsetof( QgsBillboardUniforms, verticalOffset ), 4 },
    { u"featureFlags"_s, offsetof( QgsBillboardUniforms, featureFlags ), 4 },
  };
  QVERIFY2( shader->checkUniformBlockLayout( QgsRhiBinding::Material, members, &error ), error.toUtf8() );
}

void TestQgsMaterials3D::testBillboardFixedPixelSize()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  const std::unique_ptr<QgsRhiEngine> engine = makeEngine();
  QVERIFY( engine );
  if ( !engine->rhi()->isFeatureSupported( QRhi::Instancing ) )
    QSKIP( "The device does not support instanced drawing" );

  QgsGraphicsPipelineCache cache;

  // the unit quad every billboard is drawn from, as a triangle strip
  const float corners[] = { -0.5f, -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f };
  QgsGpuBuffer quadBuffer( QgsGpuBuffer::Role::Vertex );
  quadBuffer.setData( QByteArray( reinterpret_cast<const char *>( corners ), sizeof( corners ) ) );

  // one billboard, at the point the camera is looking at
  const float positions[] = { 5.0f, 5.0f, 0.0f };
  QgsGpuBuffer instanceBuffer( QgsGpuBuffer::Role::Vertex );
  instanceBuffer.setData( QByteArray( reinterpret_cast<const char *>( positions ), sizeof( positions ) ) );

  QgsGeometry3D geometry;
  geometry.addAttribute( { u"vertexPosition"_s, &quadBuffer, QRhiVertexInputAttribute::Float2, 0, 0, 0 } );
  geometry.addAttribute( { u"instancePosition"_s, &instanceBuffer, QRhiVertexInputAttribute::Float3, 0, 0, 1 } );
  geometry.setVertexCount( 4 );

  QImage sprite( 4, 4, QImage::Format_RGBA8888 );
  sprite.fill( QColor( 255, 0, 0, 255 ) );
  QgsGpuTexture texture;
  texture.setImage( sprite );

  QgsBillboardMaterial3D material;
  QCOMPARE( material.instanceAttributes(), QgsBillboardMaterial3D::InstanceAttributes() );
  QCOMPARE( material.topology(), QRhiGraphicsPipeline::TriangleStrip );
  material.setSpriteTexture( &texture );
  QCOMPARE( material.spriteTexture(), &texture );

  // twenty pixels across in a sixty four pixel viewport
  material.setSize( QSizeF( 20, 20 ) );

  QgsRenderPass pass( u"billboard"_s );
  setTopDownCamera( pass );
  pass.setClearColor( QColor( 0, 0, 255 ) );

  QgsRenderCommand command;
  command.geometry = &geometry;
  command.material = &material;
  command.instanceCount = 1;
  pass.addCommand( command );

  engine->setReadbackEnabled( true );
  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();

  const QImage image = engine->readbackImage();
  QVERIFY2( colorsClose( centerPixel( image ), QColor( 255, 0, 0 ) ), centerPixel( image ).name().toUtf8() );

  // the quad is twenty pixels wide whatever the geometry's own scale is, so it covers the
  // middle of the image and nothing near its edge
  QVERIFY2( colorsClose( image.pixelColor( 40, 32 ), QColor( 255, 0, 0 ) ), "the billboard is narrower than twenty pixels" );
  QVERIFY2( colorsClose( image.pixelColor( 56, 32 ), QColor( 0, 0, 255 ) ), "the billboard is wider than twenty pixels" );

  // shifting it up by half its height takes it off the centre altogether
  material.setVerticalOffset( 0.5f );
  material.setVerticalOffsetEnabled( true );

  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();
  QVERIFY2( colorsClose( centerPixel( engine->readbackImage() ), QColor( 0, 0, 255 ) ), "the vertical offset did not move the billboard" );

  pass.releaseGpuResources();
  material.releaseGpuResources();
  texture.releaseGpuResources();
  cache.clear();
}

void TestQgsMaterials3D::testBillboardPerspectiveScale()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  const std::unique_ptr<QgsRhiEngine> engine = makeEngine();
  QVERIFY( engine );
  if ( !engine->rhi()->isFeatureSupported( QRhi::Instancing ) )
    QSKIP( "The device does not support instanced drawing" );

  QgsGraphicsPipelineCache cache;

  const float corners[] = { -0.5f, -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f };
  QgsGpuBuffer quadBuffer( QgsGpuBuffer::Role::Vertex );
  quadBuffer.setData( QByteArray( reinterpret_cast<const char *>( corners ), sizeof( corners ) ) );

  // one billboard carrying its own size, which is what the baked variant is for
  const float instances[] = { 5.0f, 5.0f, 0.0f, 4.0f, 4.0f };
  QgsGpuBuffer instanceBuffer( QgsGpuBuffer::Role::Vertex );
  instanceBuffer.setData( QByteArray( reinterpret_cast<const char *>( instances ), sizeof( instances ) ) );

  const quint32 stride = 5 * sizeof( float );
  QgsGeometry3D geometry;
  geometry.addAttribute( { u"vertexPosition"_s, &quadBuffer, QRhiVertexInputAttribute::Float2, 0, 0, 0 } );
  geometry.addAttribute( { u"instancePosition"_s, &instanceBuffer, QRhiVertexInputAttribute::Float3, 0, stride, 1 } );
  geometry.addAttribute( { u"instanceSize"_s, &instanceBuffer, QRhiVertexInputAttribute::Float2, 3 * sizeof( float ), stride, 1 } );
  geometry.setVertexCount( 4 );

  QImage sprite( 4, 4, QImage::Format_RGBA8888 );
  sprite.fill( QColor( 255, 0, 0, 255 ) );
  QgsGpuTexture texture;
  texture.setImage( sprite );

  QgsBillboardMaterial3D material( QgsBillboardMaterial3D::InstanceAttribute::Size );
  material.setSpriteTexture( &texture );

  // sized in world units now, so the four unit quad covers a known part of the view
  material.setPerspectiveScaleEnabled( true );

  // and the material's own size must be ignored in favour of the instance's
  material.setSize( QSizeF( 1000, 1000 ) );

  QgsRenderPass pass( u"billboard perspective"_s );
  setTopDownCamera( pass );
  pass.setClearColor( QColor( 0, 0, 255 ) );

  QgsRenderCommand command;
  command.geometry = &geometry;
  command.material = &material;
  command.instanceCount = 1;
  pass.addCommand( command );

  engine->setReadbackEnabled( true );
  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();

  const QImage image = engine->readbackImage();
  QVERIFY2( colorsClose( centerPixel( image ), QColor( 255, 0, 0 ) ), centerPixel( image ).name().toUtf8() );

  // the camera is thirty units out with a forty five degree field of view, so it sees
  // about twenty five units across: a four unit quad is a small part of that and leaves
  // the edges of the image alone
  QVERIFY2( colorsClose( image.pixelColor( 2, 32 ), QColor( 0, 0, 255 ) ), "a four unit quad should not reach the edge of the view" );

  pass.releaseGpuResources();
  material.releaseGpuResources();
  texture.releaseGpuResources();
  cache.clear();
}

//
// QgsMeshMaterial3D
//

void TestQgsMaterials3D::testMeshVariantsExist()
{
  const QString fragment = u":/shaders/rhi/mesh.frag.qsb"_s;
  checkVariantsExist( { u"mesh"_s, u"mesh_scalar"_s }, fragment );

  // the magnitude is a vertex attribute in one variant and the vertex height in the other,
  // which is exactly the kind of difference that cannot be a run time flag
  const std::shared_ptr<QgsRhiShader> plain = QgsRhiShader::load( u":/shaders/rhi/mesh.vert.qsb"_s, fragment );
  QCOMPARE( plain->inputNames().size(), 2 );
  QCOMPARE( plain->inputLocation( u"scalarMagnitude"_s ), -1 );

  const std::shared_ptr<QgsRhiShader> scalar = QgsRhiShader::load( u":/shaders/rhi/mesh_scalar.vert.qsb"_s, fragment );
  QCOMPARE( scalar->inputLocation( u"scalarMagnitude"_s ), 2 );

  const QVector<QShaderDescription::InOutVariable> samplers = plain->samplers();
  QCOMPARE( samplers.size(), 3 );
  QCOMPARE( samplers.at( 0 ).binding, int( QgsMeshMaterial3D::ColorRampTextureBinding ) );
  QCOMPARE( samplers.at( 1 ).binding, int( QgsMeshMaterial3D::ArrowsGridTextureBinding ) );
  QCOMPARE( samplers.at( 2 ).binding, int( QgsMeshMaterial3D::ArrowTextureBinding ) );
}

void TestQgsMaterials3D::testMeshUniformBlockLayout()
{
  const std::shared_ptr<QgsRhiShader> shader = QgsRhiShader::load( u":/shaders/rhi/mesh.vert.qsb"_s, u":/shaders/rhi/mesh.frag.qsb"_s );
  QVERIFY( shader );

  QCOMPARE( shader->uniformBlockSize( QgsRhiBinding::Material ), int( sizeof( QgsMeshUniforms ) ) );

  QString error;
  const QVector<QgsUniformMember> members {
    { u"meshColor"_s, offsetof( QgsMeshUniforms, meshColor ), 16 },
    { u"lineColor"_s, offsetof( QgsMeshUniforms, lineColor ), 16 },
    { u"arrowsColor"_s, offsetof( QgsMeshUniforms, arrowsColor ), 16 },
    { u"arrowsMinCorner"_s, offsetof( QgsMeshUniforms, arrowsMinCorner ), 8 },
    { u"arrowsSpacing"_s, offsetof( QgsMeshUniforms, arrowsSpacing ), 4 },
    { u"lineWidth"_s, offsetof( QgsMeshUniforms, lineWidth ), 4 },
    { u"textureType"_s, offsetof( QgsMeshUniforms, textureType ), 4 },
    { u"colorRampType"_s, offsetof( QgsMeshUniforms, colorRampType ), 4 },
    { u"colorRampCount"_s, offsetof( QgsMeshUniforms, colorRampCount ), 4 },
    { u"featureFlags"_s, offsetof( QgsMeshUniforms, featureFlags ), 4 },
  };
  QVERIFY2( shader->checkUniformBlockLayout( QgsRhiBinding::Material, members, &error ), error.toUtf8() );

  // the rendering styles and the ramp types are the shader's own copy of two QGIS enums,
  // so a value that moved would be caught here rather than by a wrongly coloured surface
  QCOMPARE( static_cast<int>( QgsMeshMaterial3D::Style::SingleColor ), 0 );
  QCOMPARE( static_cast<int>( Qgis::ShaderInterpolationMethod::Linear ), 0 );
  QCOMPARE( static_cast<int>( Qgis::ShaderInterpolationMethod::Discrete ), 1 );
  QCOMPARE( static_cast<int>( Qgis::ShaderInterpolationMethod::Exact ), 2 );
}

void TestQgsMaterials3D::testMeshSingleColor()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  const std::unique_ptr<QgsRhiEngine> engine = makeEngine();
  QVERIFY( engine );
  QgsGraphicsPipelineCache cache;

  QgsGpuBuffer vertexBuffer( QgsGpuBuffer::Role::Vertex );
  QgsGpuBuffer magnitudeBuffer( QgsGpuBuffer::Role::Vertex );
  QgsGeometry3D geometry;
  makeMeshGeometry( vertexBuffer, magnitudeBuffer, geometry, 0.0f );

  QgsMeshMaterial3D material( true );
  QVERIFY( material.hasScalarMagnitude() );
  QCOMPARE( material.features(), QgsMeshMaterial3D::Features() );
  material.setCullMode( QRhiGraphicsPipeline::None );
  material.setStyle( QgsMeshMaterial3D::Style::SingleColor );
  material.setMeshColor( QColor( 255, 255, 255 ) );

  QgsRenderPass pass( u"mesh"_s );
  setTopDownCamera( pass );
  pass.setClearColor( QColor( 0, 0, 255 ) );

  QgsRenderCommand command;
  command.geometry = &geometry;
  command.material = &material;
  pass.addCommand( command );

  engine->setReadbackEnabled( true );

  // unlit, the surface keeps only the shader's fixed 15% ambient term
  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();
  QVERIFY2( colorsClose( centerPixel( engine->readbackImage() ), QColor( 38, 38, 38 ), 2 ), centerPixel( engine->readbackImage() ).name().toUtf8() );

  // and fully lit it is over saturated by that same term, so white stays white
  setOverheadLight( pass );
  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();
  QVERIFY2( colorsClose( centerPixel( engine->readbackImage() ), QColor( 255, 255, 255 ) ), centerPixel( engine->readbackImage() ).name().toUtf8() );

  // shading the triangle with its own normal instead of the interpolated one is a run
  // time flag, unlike the magnitude
  material.setFlatTrianglesEnabled( true );
  QVERIFY( material.features().testFlag( QgsMeshMaterial3D::Feature::FlatTriangles ) );

  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();
  QVERIFY2( colorsClose( centerPixel( engine->readbackImage() ), QColor( 255, 255, 255 ) ), "the derived normal should face the light just as the interpolated one does" );

  pass.releaseGpuResources();
  material.releaseGpuResources();
  cache.clear();
}

void TestQgsMaterials3D::testMeshColorRamp()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  const std::unique_ptr<QgsRhiEngine> engine = makeEngine();
  QVERIFY( engine );
  QgsGraphicsPipelineCache cache;

  // every vertex halfway along the ramp, so the interpolated centre is exactly the middle
  QgsGpuBuffer vertexBuffer( QgsGpuBuffer::Role::Vertex );
  QgsGpuBuffer magnitudeBuffer( QgsGpuBuffer::Role::Vertex );
  QgsGeometry3D geometry;
  makeMeshGeometry( vertexBuffer, magnitudeBuffer, geometry, 5.0f );

  QgsColorRampShader ramp;
  ramp.setColorRampType( Qgis::ShaderInterpolationMethod::Linear );
  ramp.setColorRampItemList( { QgsColorRampShader::ColorRampItem( 0.0, QColor( 255, 0, 0 ) ), QgsColorRampShader::ColorRampItem( 10.0, QColor( 0, 0, 255 ) ) } );

  QgsMeshMaterial3D material( true );
  material.setCullMode( QRhiGraphicsPipeline::None );
  material.setStyle( QgsMeshMaterial3D::Style::ColorRamp );
  material.setColorRamp( ramp );
  QCOMPARE( material.uniforms().colorRampCount, 2 );
  QCOMPARE( material.uniforms().colorRampType, int( Qgis::ShaderInterpolationMethod::Linear ) );

  QgsRenderPass pass( u"mesh colour ramp"_s );
  setTopDownCamera( pass );
  pass.setClearColor( QColor( 0, 255, 0 ) );
  setOverheadLight( pass );

  QgsRenderCommand command;
  command.geometry = &geometry;
  command.material = &material;
  pass.addCommand( command );

  engine->setReadbackEnabled( true );
  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();

  // the ramp is mixed in sRGB and only then converted to linear, which is what keeps it
  // looking evenly spaced: halfway between red and blue is sRGB 0.5, or linear 0.5^2.2,
  // and the 15% ambient on top of a fully lit surface scales that by 1.15
  const int expected = static_cast<int>( std::round( std::pow( 0.5, 2.2 ) * 1.15 * 255.0 ) );
  QVERIFY2( colorsClose( centerPixel( engine->readbackImage() ), QColor( expected, 0, expected ), 3 ), centerPixel( engine->readbackImage() ).name().toUtf8() );

  // an empty ramp falls back to grey rather than to whatever was last in the texture
  material.setColorRamp( QgsColorRampShader() );
  QCOMPARE( material.uniforms().colorRampCount, 0 );

  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();
  const int grey = static_cast<int>( std::round( std::pow( 0.5, 2.2 ) * 1.15 * 255.0 ) );
  QVERIFY2( colorsClose( centerPixel( engine->readbackImage() ), QColor( grey, grey, grey ), 3 ), centerPixel( engine->readbackImage() ).name().toUtf8() );

  pass.releaseGpuResources();
  material.releaseGpuResources();
  cache.clear();
}

void TestQgsMaterials3D::testMeshWireframe()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  const std::unique_ptr<QgsRhiEngine> engine = makeEngine();
  QVERIFY( engine );
  QgsGraphicsPipelineCache cache;

  QgsGpuBuffer vertexBuffer( QgsGpuBuffer::Role::Vertex );
  QgsGpuBuffer magnitudeBuffer( QgsGpuBuffer::Role::Vertex );
  QgsGeometry3D geometry;
  makeMeshGeometry( vertexBuffer, magnitudeBuffer, geometry, 0.0f );

  QgsMeshMaterial3D material( true );
  material.setCullMode( QRhiGraphicsPipeline::None );
  material.setStyle( QgsMeshMaterial3D::Style::SingleColor );
  material.setMeshColor( QColor( 255, 255, 255 ) );
  material.setWireframeColor( QColor( 255, 0, 0 ) );

  QgsRenderPass pass( u"mesh wireframe"_s );
  setTopDownCamera( pass );
  pass.setClearColor( QColor( 0, 0, 255 ) );
  setOverheadLight( pass );

  QgsRenderCommand command;
  command.geometry = &geometry;
  command.material = &material;
  pass.addCommand( command );

  engine->setReadbackEnabled( true );

  // a line so wide that every fragment counts as being on an edge, which turns the whole
  // surface into the wireframe's colour. The line colour is not shaded, so it comes out
  // exactly as it was set.
  material.setWireframeEnabled( true );
  material.setWireframeLineWidth( 1000.0f );
  QVERIFY( material.features().testFlag( QgsMeshMaterial3D::Feature::Wireframe ) );

  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();
  QVERIFY2( colorsClose( centerPixel( engine->readbackImage() ), QColor( 255, 0, 0 ) ), centerPixel( engine->readbackImage() ).name().toUtf8() );

  // a hairline leaves the middle of a triangle alone
  material.setWireframeLineWidth( 0.0f );
  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();
  QVERIFY2( colorsClose( centerPixel( engine->readbackImage() ), QColor( 255, 255, 255 ) ), centerPixel( engine->readbackImage() ).name().toUtf8() );

  pass.releaseGpuResources();
  material.releaseGpuResources();
  cache.clear();
}

void TestQgsMaterials3D::testMeshArrows()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  const std::unique_ptr<QgsRhiEngine> engine = makeEngine();
  QVERIFY( engine );
  QgsGraphicsPipelineCache cache;

  QgsGpuBuffer vertexBuffer( QgsGpuBuffer::Role::Vertex );
  QgsGpuBuffer magnitudeBuffer( QgsGpuBuffer::Role::Vertex );
  QgsGeometry3D geometry;
  makeMeshGeometry( vertexBuffer, magnitudeBuffer, geometry, 0.0f );

  // an arrow that covers its whole cell, so that wherever the arrow texture is sampled the
  // arrow colour wins outright and the plumbing is what is being measured
  QImage arrow( 4, 4, QImage::Format_RGBA8888 );
  arrow.fill( QColor( 255, 255, 255, 255 ) );
  QgsGpuTexture arrowTexture;
  arrowTexture.setImage( arrow );

  QgsMeshMaterial3D material( true );
  material.setCullMode( QRhiGraphicsPipeline::None );
  material.setStyle( QgsMeshMaterial3D::Style::SingleColor );
  material.setMeshColor( QColor( 255, 255, 255 ) );
  material.setArrowTexture( &arrowTexture );
  material.setArrowsColor( QColor( 255, 0, 0 ) );

  // one cell, wide enough that the whole square falls inside it
  material.setArrowsGrid( { QgsVector( 1.0, 0.0 ) }, QSize( 1, 1 ), true, 1.0 );
  material.setArrowsPlacement( QPointF( 0.0, 0.0 ), 100.0f );

  QgsRenderPass pass( u"mesh arrows"_s );
  setTopDownCamera( pass );
  pass.setClearColor( QColor( 0, 0, 255 ) );
  setOverheadLight( pass );

  QgsRenderCommand command;
  command.geometry = &geometry;
  command.material = &material;
  pass.addCommand( command );

  engine->setReadbackEnabled( true );

  // switched off, the arrow grid is bound but never read
  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();
  QVERIFY2( colorsClose( centerPixel( engine->readbackImage() ), QColor( 255, 255, 255 ) ), centerPixel( engine->readbackImage() ).name().toUtf8() );

  material.setArrowsEnabled( true );
  QVERIFY( material.features().testFlag( QgsMeshMaterial3D::Feature::Arrows ) );

  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();
  QVERIFY2( colorsClose( centerPixel( engine->readbackImage() ), QColor( 255, 0, 0 ) ), centerPixel( engine->readbackImage() ).name().toUtf8() );

  pass.releaseGpuResources();
  material.releaseGpuResources();
  arrowTexture.releaseGpuResources();
  cache.clear();
}

//
// QgsMetalRoughMaterial3D
//

void TestQgsMaterials3D::testMetalRoughVariantsExist()
{
  const QString fragment = u":/shaders/rhi/metalrough.frag.qsb"_s;
  checkVariantsExist(
    { u"metalrough"_s,
      u"metalrough_texcoord"_s,
      u"metalrough_texcoord_tangent"_s,
      u"metalrough_ddtexture"_s,
      u"metalrough_ddtexture_tangent"_s,
      u"metalrough_instanced"_s,
      u"metalrough_instanced_scale"_s,
      u"metalrough_instanced_rotation"_s,
      u"metalrough_instanced_scale_rotation"_s },
    fragment
  );
  checkVariantsExist( { u"metalrough_datadefined"_s }, u":/shaders/rhi/metalrough_datadefined.frag.qsb"_s );

  // what the material asks for and what CMake bakes are written in two different places,
  // so walk every combination the constructor can produce and check it resolves
  using Mode = QgsMetalRoughMaterial3D::Mode;
  using Attribute = QgsMetalRoughMaterial3D::VertexAttribute;
  using Attributes = QgsMetalRoughMaterial3D::VertexAttributes;

  const QVector<Attributes> attributeSets { Attributes(), Attributes( Attribute::TextureCoords ), Attributes( Attribute::TextureCoords ) | Attribute::Tangents };
  for ( const Mode mode : { Mode::Uniform, Mode::DataDefinedColors, Mode::DataDefinedTextureTransform, Mode::Instanced } )
  {
    for ( const Attributes &attributes : attributeSets )
    {
      for ( const Qgis::InstancedMaterialFlags flags :
            { Qgis::InstancedMaterialFlags(),
              Qgis::InstancedMaterialFlags( Qgis::InstancedMaterialFlag::DataDefinedScale ),
              Qgis::InstancedMaterialFlags( Qgis::InstancedMaterialFlag::DataDefinedRotation ),
              Qgis::InstancedMaterialFlag::DataDefinedScale | Qgis::InstancedMaterialFlag::DataDefinedRotation } )
      {
        const QString variant = QgsMetalRoughMaterial3D::shaderVariant( mode, attributes, flags );
        const QString path = u":/shaders/rhi/metalrough%1.vert.qsb"_s.arg( variant );
        QVERIFY2( QgsRhiShader::load( path, fragment ), path.toUtf8() );
      }
    }
  }

  // the plain variant has no optional attributes at all, so it can draw geometry that
  // carries nothing but positions and normals
  const std::shared_ptr<QgsRhiShader> plain = QgsRhiShader::load( u":/shaders/rhi/metalrough.vert.qsb"_s, fragment );
  QCOMPARE( plain->inputNames().size(), 2 );
  QCOMPARE( plain->inputLocation( u"vertexNormal"_s ), 1 );
  QCOMPARE( plain->inputLocation( u"vertexTexCoord"_s ), -1 );
  QCOMPARE( plain->inputLocation( u"vertexTangent"_s ), -1 );

  const std::shared_ptr<QgsRhiShader> textured = QgsRhiShader::load( u":/shaders/rhi/metalrough_texcoord_tangent.vert.qsb"_s, fragment );
  QCOMPARE( textured->inputLocation( u"vertexTexCoord"_s ), 2 );
  QCOMPARE( textured->inputLocation( u"vertexTangent"_s ), 3 );

  const std::shared_ptr<QgsRhiShader> dataDefined = QgsRhiShader::load( u":/shaders/rhi/metalrough_datadefined.vert.qsb"_s, u":/shaders/rhi/metalrough_datadefined.frag.qsb"_s );
  QCOMPARE( dataDefined->inputLocation( u"dataDefinedBaseColor"_s ), 4 );
  QCOMPARE( dataDefined->inputLocation( u"dataDefinedEmissionColor"_s ), 5 );

  const std::shared_ptr<QgsRhiShader> ddTexture = QgsRhiShader::load( u":/shaders/rhi/metalrough_ddtexture.vert.qsb"_s, fragment );
  QCOMPARE( ddTexture->inputLocation( u"ddTextureTransform"_s ), 6 );

  const std::shared_ptr<QgsRhiShader> instanced = QgsRhiShader::load( u":/shaders/rhi/metalrough_instanced_scale_rotation.vert.qsb"_s, fragment );
  QCOMPARE( instanced->inputLocation( u"instanceTranslation"_s ), 7 );
  QCOMPARE( instanced->inputLocation( u"instanceScale"_s ), 8 );
  QCOMPARE( instanced->inputLocation( u"instanceRotation"_s ), 9 );

  // all seven maps are declared unconditionally, which is what the placeholder texture is for
  const QVector<QShaderDescription::InOutVariable> samplers = plain->samplers();
  QCOMPARE( samplers.size(), 7 );
  QCOMPARE( samplers.at( 0 ).binding, int( QgsMetalRoughMaterial3D::BaseColorMapBinding ) );
  QCOMPARE( samplers.at( 6 ).binding, int( QgsMetalRoughMaterial3D::EmissionMapBinding ) );
}

void TestQgsMaterials3D::testMetalRoughUniformBlockLayout()
{
  const std::shared_ptr<QgsRhiShader> shader = QgsRhiShader::load( u":/shaders/rhi/metalrough.vert.qsb"_s, u":/shaders/rhi/metalrough.frag.qsb"_s );
  QVERIFY( shader );

  QCOMPARE( shader->uniformBlockSize( QgsRhiBinding::Material ), int( sizeof( QgsMetalRoughUniforms ) ) );

  QString error;
  const QVector<QgsUniformMember> members {
    { u"baseColor"_s, offsetof( QgsMetalRoughUniforms, baseColor ), 16 },
    { u"emissiveColor"_s, offsetof( QgsMetalRoughUniforms, emissiveColor ), 16 },
    { u"metalness"_s, offsetof( QgsMetalRoughUniforms, metalness ), 4 },
    { u"roughness"_s, offsetof( QgsMetalRoughUniforms, roughness ), 4 },
    { u"reflectance"_s, offsetof( QgsMetalRoughUniforms, reflectance ), 4 },
    { u"opacity"_s, offsetof( QgsMetalRoughUniforms, opacity ), 4 },
    { u"anisotropy"_s, offsetof( QgsMetalRoughUniforms, anisotropy ), 4 },
    { u"anisotropyRotation"_s, offsetof( QgsMetalRoughUniforms, anisotropyRotation ), 4 },
    { u"clearCoatFactor"_s, offsetof( QgsMetalRoughUniforms, clearCoatFactor ), 4 },
    { u"clearCoatRoughness"_s, offsetof( QgsMetalRoughUniforms, clearCoatRoughness ), 4 },
    { u"parallaxScale"_s, offsetof( QgsMetalRoughUniforms, parallaxScale ), 4 },
    { u"emissiveFactor"_s, offsetof( QgsMetalRoughUniforms, emissiveFactor ), 4 },
    { u"texCoordScale"_s, offsetof( QgsMetalRoughUniforms, texCoordScale ), 4 },
    { u"texCoordRotation"_s, offsetof( QgsMetalRoughUniforms, texCoordRotation ), 4 },
    { u"texCoordOffset"_s, offsetof( QgsMetalRoughUniforms, texCoordOffset ), 8 },
    { u"featureFlags"_s, offsetof( QgsMetalRoughUniforms, featureFlags ), 4 },
    { u"meshMatrix"_s, offsetof( QgsMetalRoughUniforms, meshMatrix ), 64 },
    { u"meshNormalMatrix"_s, offsetof( QgsMetalRoughUniforms, meshNormalMatrix ), 48 },
    { u"symbolRotation"_s, offsetof( QgsMetalRoughUniforms, symbolRotation ), 16 },
    { u"symbolScale"_s, offsetof( QgsMetalRoughUniforms, symbolScale ), 16 },
  };
  QVERIFY2( shader->checkUniformBlockLayout( QgsRhiBinding::Material, members, &error ), error.toUtf8() );
}

void TestQgsMaterials3D::testMetalRoughFeatureFlags()
{
  using Feature = QgsMetalRoughMaterial3D::Feature;

  QgsMetalRoughMaterial3D material;
  QCOMPARE( material.mode(), QgsMetalRoughMaterial3D::Mode::Uniform );
  QCOMPARE( material.features(), QgsMetalRoughMaterial3D::Features() );

  QgsGpuTexture texture;
  QImage image( 1, 1, QImage::Format_RGBA8888 );
  image.fill( Qt::white );
  texture.setImage( image );

  material.setBaseColorTexture( &texture );
  QVERIFY( material.features().testFlag( Feature::BaseColorMap ) );
  material.setBaseColorTexture( nullptr );
  QVERIFY( !material.features().testFlag( Feature::BaseColorMap ) );

  material.setNormalTexture( &texture );
  material.setHeightTexture( &texture );
  QVERIFY( material.features().testFlag( Feature::NormalMap ) );
  QVERIFY( material.features().testFlag( Feature::HeightMap ) );

  // anisotropy and the clear coat follow their own values rather than being set directly,
  // which is how the Qt3D material decided whether to inject their defines
  material.setAnisotropy( 0.5f );
  QVERIFY( material.features().testFlag( Feature::Anisotropy ) );
  material.setAnisotropy( 0.0f );
  QVERIFY( !material.features().testFlag( Feature::Anisotropy ) );

  material.setClearCoatFactor( 0.25f );
  QVERIFY( material.features().testFlag( Feature::ClearCoat ) );
  material.setClearCoatFactor( 0.0f );
  QVERIFY( !material.features().testFlag( Feature::ClearCoat ) );

  material.setFlatShadingEnabled( true );
  QVERIFY( material.features().testFlag( Feature::FlatShading ) );

  // degrees in, radians out, because the shader takes the sine of this directly
  material.setAnisotropyRotation( 180.0f );
  QGSCOMPARENEAR( material.uniforms().anisotropyRotation, 3.14159f, 0.0001 );
}

void TestQgsMaterials3D::testMetalRoughEmission()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  const std::unique_ptr<QgsRhiEngine> engine = makeEngine();
  QVERIFY( engine );
  QgsGraphicsPipelineCache cache;

  QgsGpuBuffer vertexBuffer( QgsGpuBuffer::Role::Vertex );
  makeQuad( vertexBuffer );

  const quint32 stride = 12 * sizeof( float );
  QgsGeometry3D geometry;
  geometry.addAttribute( { u"vertexPosition"_s, &vertexBuffer, QRhiVertexInputAttribute::Float3, 0, stride, 0 } );
  geometry.addAttribute( { u"vertexNormal"_s, &vertexBuffer, QRhiVertexInputAttribute::Float3, 3 * sizeof( float ), stride, 0 } );
  geometry.setVertexCount( 6 );

  QgsMetalRoughMaterial3D material;
  material.setCullMode( QRhiGraphicsPipeline::None );
  material.setEmissionColor( QColor( 255, 0, 0 ) );

  QgsRenderPass pass( u"metal rough emission"_s );
  setTopDownCamera( pass );
  pass.setClearColor( QColor( 0, 0, 255 ) );

  QgsRenderCommand command;
  command.geometry = &geometry;
  command.material = &material;
  pass.addCommand( command );

  // with no lights the only term left is the emission, so the result is exact, and it
  // also says the material drew at all with nothing but placeholder textures bound
  engine->setReadbackEnabled( true );
  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();
  QVERIFY2( colorsClose( centerPixel( engine->readbackImage() ), QColor( 255, 0, 0 ) ), centerPixel( engine->readbackImage() ).name().toUtf8() );

  // and the emission factor scales it
  material.setEmissionFactor( 0.5f );
  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();
  QVERIFY2( colorsClose( centerPixel( engine->readbackImage() ), QColor( 128, 0, 0 ), 2 ), centerPixel( engine->readbackImage() ).name().toUtf8() );

  pass.releaseGpuResources();
  material.releaseGpuResources();
  cache.clear();
}

void TestQgsMaterials3D::testMetalRoughLitBaseColor()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  const std::unique_ptr<QgsRhiEngine> engine = makeEngine();
  QVERIFY( engine );
  QgsGraphicsPipelineCache cache;

  QgsGpuBuffer vertexBuffer( QgsGpuBuffer::Role::Vertex );
  makeQuad( vertexBuffer );

  const quint32 stride = 12 * sizeof( float );
  QgsGeometry3D geometry;
  geometry.addAttribute( { u"vertexPosition"_s, &vertexBuffer, QRhiVertexInputAttribute::Float3, 0, stride, 0 } );
  geometry.addAttribute( { u"vertexNormal"_s, &vertexBuffer, QRhiVertexInputAttribute::Float3, 3 * sizeof( float ), stride, 0 } );
  geometry.setVertexCount( 6 );

  QgsMetalRoughMaterial3D material;
  material.setCullMode( QRhiGraphicsPipeline::None );
  material.setBaseColor( QColor( 255, 0, 0 ) );
  material.setMetalness( 0.0f );
  material.setRoughness( 1.0f );

  QgsRenderPass pass( u"metal rough"_s );
  setTopDownCamera( pass );
  pass.setClearColor( QColor( 0, 0, 255 ) );

  QgsRenderCommand command;
  command.geometry = &geometry;
  command.material = &material;
  pass.addCommand( command );

  engine->setReadbackEnabled( true );

  // unlit, and with no emission, a metal rough surface is black
  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();
  const QColor unlit = centerPixel( engine->readbackImage() );
  QVERIFY2( colorsClose( unlit, QColor( 0, 0, 0 ) ), unlit.name().toUtf8() );

  // lit, a red dielectric reflects red and next to nothing else. The exact value depends
  // on the whole BRDF, so what is pinned down here is the hue and that it is lit at all.
  setOverheadLight( pass );
  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();
  const QColor lit = centerPixel( engine->readbackImage() );
  QVERIFY2( lit.red() > 40, lit.name().toUtf8() );
  QVERIFY2( lit.green() < lit.red() / 2, lit.name().toUtf8() );
  QVERIFY2( lit.blue() < lit.red() / 2, lit.name().toUtf8() );

  // a metal takes its specular colour from the base colour and has no diffuse term, so it
  // is darker than the dielectric under a light this far off the mirror direction
  material.setMetalness( 1.0f );
  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();
  const QColor metal = centerPixel( engine->readbackImage() );
  QVERIFY2( metal.red() < lit.red(), u"metal %1 should be darker than dielectric %2"_s.arg( metal.name(), lit.name() ).toUtf8() );

  pass.releaseGpuResources();
  material.releaseGpuResources();
  cache.clear();
}

void TestQgsMaterials3D::testMetalRoughBaseColorMap()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  const std::unique_ptr<QgsRhiEngine> engine = makeEngine();
  QVERIFY( engine );
  QgsGraphicsPipelineCache cache;

  QgsGpuBuffer vertexBuffer( QgsGpuBuffer::Role::Vertex );
  makeQuad( vertexBuffer );

  const quint32 stride = 12 * sizeof( float );
  QgsGeometry3D geometry;
  geometry.addAttribute( { u"vertexPosition"_s, &vertexBuffer, QRhiVertexInputAttribute::Float3, 0, stride, 0 } );
  geometry.addAttribute( { u"vertexNormal"_s, &vertexBuffer, QRhiVertexInputAttribute::Float3, 3 * sizeof( float ), stride, 0 } );
  geometry.addAttribute( { u"vertexTexCoord"_s, &vertexBuffer, QRhiVertexInputAttribute::Float2, 6 * sizeof( float ), stride, 0 } );
  geometry.setVertexCount( 6 );

  QImage source( 4, 4, QImage::Format_RGBA8888 );
  source.fill( QColor( 0, 255, 0 ) );
  QgsGpuTexture baseColorMap;
  baseColorMap.setImage( source );

  QgsMetalRoughMaterial3D material( QgsMetalRoughMaterial3D::Mode::Uniform, QgsMetalRoughMaterial3D::VertexAttribute::TextureCoords );
  material.setCullMode( QRhiGraphicsPipeline::None );
  material.setMetalness( 0.0f );
  material.setRoughness( 1.0f );

  // the uniform base colour must lose to the map
  material.setBaseColor( QColor( 255, 0, 0 ) );
  material.setBaseColorTexture( &baseColorMap );

  QgsRenderPass pass( u"metal rough base colour map"_s );
  setTopDownCamera( pass );
  pass.setClearColor( QColor( 0, 0, 255 ) );
  setOverheadLight( pass );

  QgsRenderCommand command;
  command.geometry = &geometry;
  command.material = &material;
  pass.addCommand( command );

  engine->setReadbackEnabled( true );
  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();

  const QColor color = centerPixel( engine->readbackImage() );
  QVERIFY2( color.green() > 40, color.name().toUtf8() );
  QVERIFY2( color.red() < color.green() / 2, color.name().toUtf8() );

  pass.releaseGpuResources();
  material.releaseGpuResources();
  baseColorMap.releaseGpuResources();
  cache.clear();
}

void TestQgsMaterials3D::testMetalRoughDataDefined()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  const std::unique_ptr<QgsRhiEngine> engine = makeEngine();
  QVERIFY( engine );
  QgsGraphicsPipelineCache cache;

  QgsGpuBuffer vertexBuffer( QgsGpuBuffer::Role::Vertex );
  makeQuad( vertexBuffer );

  // base colour green and emission red, repeated for all six vertices
  QVector<float> colors;
  for ( int i = 0; i < 6; ++i )
    colors << 0.0f << 1.0f << 0.0f << 1.0f << 0.0f << 0.0f;

  QgsGpuBuffer colorBuffer( QgsGpuBuffer::Role::Vertex );
  colorBuffer.setData( QByteArray( reinterpret_cast<const char *>( colors.constData() ), colors.size() * int( sizeof( float ) ) ) );

  const quint32 stride = 12 * sizeof( float );
  const quint32 colorStride = 6 * sizeof( float );

  QgsGeometry3D geometry;
  geometry.addAttribute( { u"vertexPosition"_s, &vertexBuffer, QRhiVertexInputAttribute::Float3, 0, stride, 0 } );
  geometry.addAttribute( { u"vertexNormal"_s, &vertexBuffer, QRhiVertexInputAttribute::Float3, 3 * sizeof( float ), stride, 0 } );
  geometry.addAttribute( { u"dataDefinedBaseColor"_s, &colorBuffer, QRhiVertexInputAttribute::Float3, 0, colorStride, 0 } );
  geometry.addAttribute( { u"dataDefinedEmissionColor"_s, &colorBuffer, QRhiVertexInputAttribute::Float3, 3 * sizeof( float ), colorStride, 0 } );
  geometry.setVertexCount( 6 );

  QgsMetalRoughMaterial3D material( QgsMetalRoughMaterial3D::Mode::DataDefinedColors );
  QCOMPARE( material.mode(), QgsMetalRoughMaterial3D::Mode::DataDefinedColors );
  material.setCullMode( QRhiGraphicsPipeline::None );

  // both uniform colours must be ignored entirely by this variant
  material.setBaseColor( QColor( 0, 0, 255 ) );
  material.setEmissionColor( QColor( 0, 0, 255 ) );

  QgsRenderPass pass( u"metal rough data defined"_s );
  setTopDownCamera( pass );
  pass.setClearColor( QColor( 0, 0, 255 ) );

  QgsRenderCommand command;
  command.geometry = &geometry;
  command.material = &material;
  pass.addCommand( command );

  // unlit, so only the per-vertex emission shows and the expected colour is exact
  engine->setReadbackEnabled( true );
  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();
  QVERIFY2( colorsClose( centerPixel( engine->readbackImage() ), QColor( 255, 0, 0 ) ), centerPixel( engine->readbackImage() ).name().toUtf8() );

  // lit, the per-vertex base colour adds its own green on top of that emission
  setOverheadLight( pass );
  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();
  const QColor lit = centerPixel( engine->readbackImage() );
  QVERIFY2( lit.green() > 40, lit.name().toUtf8() );
  QVERIFY2( lit.red() > 200, lit.name().toUtf8() );

  pass.releaseGpuResources();
  material.releaseGpuResources();
  cache.clear();
}

void TestQgsMaterials3D::testMetalRoughFlatShading()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  const std::unique_ptr<QgsRhiEngine> engine = makeEngine();
  QVERIFY( engine );
  QgsGraphicsPipelineCache cache;

  // the square lies flat, but every vertex claims its normal points sideways, so the
  // interpolated normal is wrong and the derived one is right
  QgsGpuBuffer vertexBuffer( QgsGpuBuffer::Role::Vertex );
  makeQuad( vertexBuffer, QVector3D( 0.0f, 1.0f, 0.0f ) );

  const quint32 stride = 12 * sizeof( float );
  QgsGeometry3D geometry;
  geometry.addAttribute( { u"vertexPosition"_s, &vertexBuffer, QRhiVertexInputAttribute::Float3, 0, stride, 0 } );
  geometry.addAttribute( { u"vertexNormal"_s, &vertexBuffer, QRhiVertexInputAttribute::Float3, 3 * sizeof( float ), stride, 0 } );
  geometry.setVertexCount( 6 );

  QgsMetalRoughMaterial3D material;
  material.setCullMode( QRhiGraphicsPipeline::None );
  material.setBaseColor( QColor( 255, 255, 255 ) );
  material.setMetalness( 0.0f );
  material.setRoughness( 1.0f );

  QgsRenderPass pass( u"metal rough flat shading"_s );
  setTopDownCamera( pass );
  pass.setClearColor( QColor( 0, 0, 255 ) );
  setOverheadLight( pass );

  QgsRenderCommand command;
  command.geometry = &geometry;
  command.material = &material;
  pass.addCommand( command );

  engine->setReadbackEnabled( true );

  // the light grazes the claimed normal, so the surface picks up almost nothing
  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();
  const QColor interpolated = centerPixel( engine->readbackImage() );

  // with flat shading the normal comes from the geometry itself, which really does face
  // the light
  material.setFlatShadingEnabled( true );
  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();
  const QColor flat = centerPixel( engine->readbackImage() );

  QVERIFY2( flat.red() > interpolated.red() + 20, u"flat %1 should be brighter than interpolated %2"_s.arg( flat.name(), interpolated.name() ).toUtf8() );

  // and it points out of the surface rather than into it, which only holds because the
  // shader corrects its y derivative for the framebuffer's row order. Without that this
  // passes on OpenGL and comes out black on every other backend.
  QgsLightUniforms fromBelow;
  fromBelow.addDirectionalLight( QVector3D( 0.0f, 0.0f, 1.0f ), QColor( 255, 255, 255 ), 1.0f );
  pass.setLights( fromBelow );

  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();
  QVERIFY2( colorsClose( centerPixel( engine->readbackImage() ), QColor( 0, 0, 0 ) ), "a light behind the facet should not light it" );

  pass.releaseGpuResources();
  material.releaseGpuResources();
  cache.clear();
}

//
// QgsPhongTexturedMaterial3D
//

void TestQgsMaterials3D::testPhongTexturedVariantsExist()
{
  const QString fragment = u":/shaders/rhi/phongtextured.frag.qsb"_s;
  checkVariantsExist( { u"phongtextured"_s, u"phongtextured_datadefined"_s, u"phongtextured_instanced"_s, u"phongtextured_instanced_scale"_s, u"phongtextured_instanced_rotation"_s, u"phongtextured_instanced_scale_rotation"_s }, fragment );

  const std::shared_ptr<QgsRhiShader> plain = QgsRhiShader::load( u":/shaders/rhi/phongtextured.vert.qsb"_s, fragment );
  QCOMPARE( plain->inputLocation( u"vertexNormal"_s ), 1 );
  QCOMPARE( plain->inputLocation( u"vertexTexCoord"_s ), 2 );
  QCOMPARE( plain->inputLocation( u"ddTextureTransform"_s ), -1 );

  const std::shared_ptr<QgsRhiShader> dataDefined = QgsRhiShader::load( u":/shaders/rhi/phongtextured_datadefined.vert.qsb"_s, fragment );
  QCOMPARE( dataDefined->inputLocation( u"ddTextureTransform"_s ), 3 );

  const std::shared_ptr<QgsRhiShader> instanced = QgsRhiShader::load( u":/shaders/rhi/phongtextured_instanced_scale_rotation.vert.qsb"_s, fragment );
  QCOMPARE( instanced->inputLocation( u"instanceTranslation"_s ), 4 );
  QCOMPARE( instanced->inputLocation( u"instanceScale"_s ), 5 );
  QCOMPARE( instanced->inputLocation( u"instanceRotation"_s ), 6 );

  const QVector<QShaderDescription::InOutVariable> samplers = plain->samplers();
  QCOMPARE( samplers.size(), 1 );
  QCOMPARE( samplers.at( 0 ).binding, QgsPhongTexturedMaterial3D::DiffuseTextureBinding );
}

void TestQgsMaterials3D::testPhongTexturedUniformBlockLayout()
{
  const std::shared_ptr<QgsRhiShader> shader = QgsRhiShader::load( u":/shaders/rhi/phongtextured.vert.qsb"_s, u":/shaders/rhi/phongtextured.frag.qsb"_s );
  QVERIFY( shader );

  QCOMPARE( shader->uniformBlockSize( QgsRhiBinding::Material ), int( sizeof( QgsPhongTexturedUniforms ) ) );

  QString error;
  const QVector<QgsUniformMember> members {
    { u"ambientColor"_s, offsetof( QgsPhongTexturedUniforms, ambientColor ), 16 },
    { u"specularColor"_s, offsetof( QgsPhongTexturedUniforms, specularColor ), 16 },
    { u"shininess"_s, offsetof( QgsPhongTexturedUniforms, shininess ), 4 },
    { u"opacity"_s, offsetof( QgsPhongTexturedUniforms, opacity ), 4 },
    { u"texCoordScale"_s, offsetof( QgsPhongTexturedUniforms, texCoordScale ), 4 },
    { u"texCoordRotation"_s, offsetof( QgsPhongTexturedUniforms, texCoordRotation ), 4 },
    { u"texCoordOffset"_s, offsetof( QgsPhongTexturedUniforms, texCoordOffset ), 8 },
    { u"meshMatrix"_s, offsetof( QgsPhongTexturedUniforms, meshMatrix ), 64 },
    { u"meshNormalMatrix"_s, offsetof( QgsPhongTexturedUniforms, meshNormalMatrix ), 48 },
    { u"symbolRotation"_s, offsetof( QgsPhongTexturedUniforms, symbolRotation ), 16 },
    { u"symbolScale"_s, offsetof( QgsPhongTexturedUniforms, symbolScale ), 16 },
  };
  QVERIFY2( shader->checkUniformBlockLayout( QgsRhiBinding::Material, members, &error ), error.toUtf8() );
}

void TestQgsMaterials3D::testPhongTexturedQuad()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  const std::unique_ptr<QgsRhiEngine> engine = makeEngine();
  QVERIFY( engine );
  QgsGraphicsPipelineCache cache;

  // position, normal and texture coordinate interleaved
  const float vertices[] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 10.0f, 0.0f,  0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 10.0f, 10.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
                             0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 10.0f, 10.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f,  10.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f };
  QgsGpuBuffer vertexBuffer( QgsGpuBuffer::Role::Vertex );
  vertexBuffer.setData( QByteArray( reinterpret_cast<const char *>( vertices ), sizeof( vertices ) ) );

  const quint32 stride = 8 * sizeof( float );
  QgsGeometry3D geometry;
  geometry.addAttribute( { u"vertexPosition"_s, &vertexBuffer, QRhiVertexInputAttribute::Float3, 0, stride, 0 } );
  geometry.addAttribute( { u"vertexNormal"_s, &vertexBuffer, QRhiVertexInputAttribute::Float3, 3 * sizeof( float ), stride, 0 } );
  geometry.addAttribute( { u"vertexTexCoord"_s, &vertexBuffer, QRhiVertexInputAttribute::Float2, 6 * sizeof( float ), stride, 0 } );
  geometry.setVertexCount( 6 );

  QImage source( 4, 4, QImage::Format_RGBA8888 );
  source.fill( QColor( 20, 180, 90 ) );
  QgsGpuTexture texture;
  texture.setImage( source );

  QgsPhongTexturedMaterial3D material;
  QVERIFY( !material.isDataDefinedTextureTransform() );
  QVERIFY( !material.isInstanced() );
  material.setCullMode( QRhiGraphicsPipeline::None );
  material.setDiffuseTexture( &texture );
  QCOMPARE( material.diffuseTexture(), &texture );

  // no ambient and no specular, so a fully lit surface shows the texture untouched
  material.setAmbient( Qt::black );
  material.setSpecular( Qt::black );
  material.setShininess( 0.0f );

  QgsRenderPass pass( u"phong textured"_s );
  setTopDownCamera( pass );
  pass.setClearColor( QColor( 0, 0, 255 ) );
  setOverheadLight( pass );

  QgsRenderCommand command;
  command.geometry = &geometry;
  command.material = &material;
  pass.addCommand( command );

  engine->setReadbackEnabled( true );
  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();

  QVERIFY2( colorsClose( centerPixel( engine->readbackImage() ), QColor( 20, 180, 90 ) ), centerPixel( engine->readbackImage() ).name().toUtf8() );

  // and with no light at all it is only the ambient colour, which is what says the texture
  // really did go through the shading model rather than straight to the output
  QgsLightUniforms noLights;
  pass.setLights( noLights );

  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();
  QVERIFY2( colorsClose( centerPixel( engine->readbackImage() ), QColor( 0, 0, 0 ) ), centerPixel( engine->readbackImage() ).name().toUtf8() );

  pass.releaseGpuResources();
  material.releaseGpuResources();
  texture.releaseGpuResources();
  cache.clear();
}

void TestQgsMaterials3D::testPhongTexturedTextureTransform()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  const std::unique_ptr<QgsRhiEngine> engine = makeEngine();
  QVERIFY( engine );
  QgsGraphicsPipelineCache cache;

  const float vertices[] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 10.0f, 0.0f,  0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 10.0f, 10.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
                             0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 10.0f, 10.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f,  10.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f };
  QgsGpuBuffer vertexBuffer( QgsGpuBuffer::Role::Vertex );
  vertexBuffer.setData( QByteArray( reinterpret_cast<const char *>( vertices ), sizeof( vertices ) ) );

  const quint32 stride = 8 * sizeof( float );
  QgsGeometry3D geometry;
  geometry.addAttribute( { u"vertexPosition"_s, &vertexBuffer, QRhiVertexInputAttribute::Float3, 0, stride, 0 } );
  geometry.addAttribute( { u"vertexNormal"_s, &vertexBuffer, QRhiVertexInputAttribute::Float3, 3 * sizeof( float ), stride, 0 } );
  geometry.addAttribute( { u"vertexTexCoord"_s, &vertexBuffer, QRhiVertexInputAttribute::Float2, 6 * sizeof( float ), stride, 0 } );
  geometry.setVertexCount( 6 );

  // two texels side by side, so which one is sampled says where the texture coordinates ended up
  QImage source( 2, 1, QImage::Format_RGBA8888 );
  source.setPixelColor( 0, 0, QColor( 255, 0, 0 ) );
  source.setPixelColor( 1, 0, QColor( 0, 255, 0 ) );
  QgsGpuTexture texture;
  texture.setImage( source );

  QgsPhongTexturedMaterial3D material;
  material.setCullMode( QRhiGraphicsPipeline::None );
  material.setDiffuseTexture( &texture );
  material.setAmbient( Qt::black );
  material.setSpecular( Qt::black );
  material.setShininess( 0.0f );

  // a scale of zero collapses every texture coordinate onto the offset point, which makes
  // the whole quad sample one texel and the offset directly observable
  material.setDiffuseTextureScale( 0.0f );
  material.setDiffuseTextureOffset( 0.25f, 0.5f );

  QgsRenderPass pass( u"phong textured transform"_s );
  setTopDownCamera( pass );
  pass.setClearColor( QColor( 0, 0, 255 ) );
  setOverheadLight( pass );

  QgsRenderCommand command;
  command.geometry = &geometry;
  command.material = &material;
  pass.addCommand( command );

  engine->setReadbackEnabled( true );
  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();
  QVERIFY2( colorsClose( centerPixel( engine->readbackImage() ), QColor( 255, 0, 0 ) ), centerPixel( engine->readbackImage() ).name().toUtf8() );

  // the centre of the other texel
  material.setDiffuseTextureOffset( 0.75f, 0.5f );

  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();
  QVERIFY2( colorsClose( centerPixel( engine->readbackImage() ), QColor( 0, 255, 0 ) ), centerPixel( engine->readbackImage() ).name().toUtf8() );

  pass.releaseGpuResources();
  material.releaseGpuResources();
  texture.releaseGpuResources();
  cache.clear();
}

QGSTEST_MAIN( TestQgsMaterials3D )
#include "testqgsmaterials3d.moc"
