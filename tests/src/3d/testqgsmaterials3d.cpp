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

#include "qgsgeometry3d.h"
#include "qgsgoochmaterial3d.h"
#include "qgsgpubuffer.h"
#include "qgsgputexture.h"
#include "qgsgraphicspipelinecache.h"
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
