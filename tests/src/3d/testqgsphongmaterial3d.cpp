/***************************************************************************
     testqgsphongmaterial3d.cpp
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
#include "qgsgpubuffer.h"
#include "qgsgraphicspipelinecache.h"
#include "qgsmaterial3dutils.h"
#include "qgsphongmaterial3d.h"
#include "qgspolygon.h"
#include "qgsrendercommand.h"
#include "qgsrenderpass.h"
#include "qgsrenderuniforms.h"
#include "qgsrhiengine.h"
#include "qgsrhishader.h"
#include "qgstessellatedpolygongeometry3d.h"
#include "qgstessellator.h"
#include "qgstest.h"

#include <QColor>
#include <QImage>
#include <QQuaternion>
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
 * Unit tests for the QRhi Phong material and the tessellated polygon geometry that feeds it.
 *
 * This is the first real QGIS material ported to the new engine, so as well as checking
 * that it draws, these tests pin down the things a material port has to get right: that
 * the uniform struct matches the GLSL block, that each shader variant exists and reads the
 * attributes it should, and that the lighting the scene supplies actually reaches the shader.
 */
class TestQgsPhongMaterial3D : public QgsTest
{
    Q_OBJECT
  public:
    TestQgsPhongMaterial3D()
      : QgsTest( u"QgsPhongMaterial3D tests"_s )
    {}

  private slots:
    void initTestCase();
    void cleanupTestCase();

    void testAllVariantsExist();
    void testUniformBlockLayout();
    void testLightBlockLayout();
    void testLightUniforms();
    void testTessellatedGeometryLayout();
    void testTessellatedGeometryFeatureIds();
    void testLitPolygon();
    void testLightDirectionMatters();
    void testAmbientOnlyWithoutLights();
    void testOpacity();
    void testDataDefinedVariant();
    void testInstancedVariant();

  private:
    std::unique_ptr<QgsRhiEngine> makeEngine( QSize size = QSize( 64, 64 ) );

    //! Tessellates a 10x10 square lying flat, as a polygon layer would produce
    void tessellateSquare( QgsTessellatedPolygonGeometry3D &geometry );

    /**
     * Points the pass's camera straight down at the flat square.
     *
     * QgsTessellator::vertexBuffer() keeps map coordinates as they are, so a flat polygon
     * comes out lying in the XY plane with its normal along +Z. Note this is not what the
     * "(x, z, -y)" note in qgstessellator.h says - that applies to the deprecated data()
     * accessor, not to vertexBuffer().
     */
    void setTopDownCamera( QgsRenderPass &pass );

    bool skipIfNoPixels() const { return mBackend == QRhi::Null; }

    QRhi::Implementation mBackend = QRhi::Null;
};

std::unique_ptr<QgsRhiEngine> TestQgsPhongMaterial3D::makeEngine( QSize size )
{
  auto engine = std::make_unique<QgsRhiEngine>();
  if ( !engine->createOffscreen( size, mBackend ) )
    return nullptr;
  return engine;
}

void TestQgsPhongMaterial3D::tessellateSquare( QgsTessellatedPolygonGeometry3D &geometry )
{
  QgsTessellator tessellator;
  tessellator.setAddNormals( true );

  QgsPolygon polygon;
  polygon.fromWkt( u"POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))"_s );
  tessellator.addPolygon( polygon, 0.0f );

  QCOMPARE( tessellator.stride(), geometry.stride() );

  geometry.setVertexBufferData( tessellator.vertexBuffer(), tessellator.dataVerticesCount() );
  const QByteArray indexData = tessellator.indexBuffer();
  geometry.setIndexBufferData( indexData, static_cast<int>( indexData.size() / tessellator.indexStride() ) );
}

void TestQgsPhongMaterial3D::setTopDownCamera( QgsRenderPass &pass )
{
  const QVector3D eye( 5.0f, 5.0f, 30.0f );

  QMatrix4x4 view;
  view.lookAt( eye, QVector3D( 5.0f, 5.0f, 0.0f ), QVector3D( 0.0f, 1.0f, 0.0f ) );

  QMatrix4x4 projection;
  projection.perspective( 45.0f, 1.0f, 1.0f, 100.0f );

  pass.setCamera( view, projection );
  pass.setEyePosition( eye );
}

void TestQgsPhongMaterial3D::initTestCase()
{
  mBackend = QgsRhiEngine::defaultBackend();

  QgsRhiEngine probe;
  if ( !probe.createOffscreen( QSize( 4, 4 ), mBackend ) )
    QSKIP( "No usable QRhi device for the default backend" );
}

void TestQgsPhongMaterial3D::cleanupTestCase()
{
  QgsRhiShader::clearCache();
  QgsApplication::exitQgis();
}

void TestQgsPhongMaterial3D::testAllVariantsExist()
{
  // a missing variant would otherwise show up much later as a material that draws nothing
  const QStringList vertexVariants {
    u"phong"_s,
    u"phong_datadefined"_s,
    u"phong_instanced"_s,
    u"phong_instanced_scale"_s,
    u"phong_instanced_rotation"_s,
    u"phong_instanced_scale_rotation"_s,
  };

  for ( const QString &variant : vertexVariants )
  {
    const std::shared_ptr<QgsRhiShader> shader = QgsRhiShader::load( u":/shaders/rhi/%1.vert.qsb"_s.arg( variant ), u":/shaders/rhi/phong.frag.qsb"_s );
    QVERIFY2( shader, variant.toUtf8() );

    // every variant must still read the two attributes the geometry always supplies
    QCOMPARE( shader->inputLocation( u"vertexPosition"_s ), 0 );
    QCOMPARE( shader->inputLocation( u"vertexNormal"_s ), 1 );
  }

  const std::shared_ptr<QgsRhiShader> dataDefined = QgsRhiShader::load( u":/shaders/rhi/phong_datadefined.vert.qsb"_s, u":/shaders/rhi/phong_datadefined.frag.qsb"_s );
  QVERIFY( dataDefined );
  QCOMPARE( dataDefined->inputLocation( u"dataDefinedAmbientColor"_s ), 2 );
  QCOMPARE( dataDefined->inputLocation( u"dataDefinedDiffuseColor"_s ), 3 );
  QCOMPARE( dataDefined->inputLocation( u"dataDefinedSpecularColor"_s ), 4 );

  // the plain variant must not read them, otherwise the geometry would have to supply them
  const std::shared_ptr<QgsRhiShader> plain = QgsRhiShader::load( u":/shaders/rhi/phong.vert.qsb"_s, u":/shaders/rhi/phong.frag.qsb"_s );
  QCOMPARE( plain->inputLocation( u"dataDefinedDiffuseColor"_s ), -1 );
  QCOMPARE( plain->inputLocation( u"instanceTranslation"_s ), -1 );

  const std::shared_ptr<QgsRhiShader> instanced = QgsRhiShader::load( u":/shaders/rhi/phong_instanced_scale_rotation.vert.qsb"_s, u":/shaders/rhi/phong.frag.qsb"_s );
  QCOMPARE( instanced->inputLocation( u"instanceTranslation"_s ), 5 );
  QCOMPARE( instanced->inputLocation( u"instanceScale"_s ), 6 );
  QCOMPARE( instanced->inputLocation( u"instanceRotation"_s ), 7 );
}

void TestQgsPhongMaterial3D::testUniformBlockLayout()
{
  const std::shared_ptr<QgsRhiShader> shader = QgsRhiShader::load( u":/shaders/rhi/phong.vert.qsb"_s, u":/shaders/rhi/phong.frag.qsb"_s );
  QVERIFY( shader );

  QCOMPARE( shader->uniformBlockSize( QgsRhiBinding::Material ), int( sizeof( QgsPhongUniforms ) ) );

  QString error;
  const QVector<QgsUniformMember> members {
    { u"ambientColor"_s, offsetof( QgsPhongUniforms, ambientColor ), 16 },
    { u"diffuseColor"_s, offsetof( QgsPhongUniforms, diffuseColor ), 16 },
    { u"specularColor"_s, offsetof( QgsPhongUniforms, specularColor ), 16 },
    { u"shininess"_s, offsetof( QgsPhongUniforms, shininess ), 4 },
    { u"opacity"_s, offsetof( QgsPhongUniforms, opacity ), 4 },
    { u"meshMatrix"_s, offsetof( QgsPhongUniforms, meshMatrix ), 64 },
    { u"meshNormalMatrix"_s, offsetof( QgsPhongUniforms, meshNormalMatrix ), 48 },
    { u"symbolRotation"_s, offsetof( QgsPhongUniforms, symbolRotation ), 16 },
    { u"symbolScale"_s, offsetof( QgsPhongUniforms, symbolScale ), 16 },
  };
  QVERIFY2( shader->checkUniformBlockLayout( QgsRhiBinding::Material, members, &error ), error.toUtf8() );
}

void TestQgsPhongMaterial3D::testLightBlockLayout()
{
  const std::shared_ptr<QgsRhiShader> shader = QgsRhiShader::load( u":/shaders/rhi/phong.vert.qsb"_s, u":/shaders/rhi/phong.frag.qsb"_s );
  QVERIFY( shader );

  QCOMPARE( shader->uniformBlockSize( QgsRhiBinding::Lights ), int( sizeof( QgsLightUniforms ) ) );

  QString error;
  const QVector<QgsUniformMember> members {
    { u"lightCount"_s, offsetof( QgsLightUniforms, lightCount ), 4 },
    { u"lights"_s, offsetof( QgsLightUniforms, lights ), int( sizeof( QgsLightUniforms::Light ) ) * QgsLightUniforms::MAX_LIGHTS },
  };
  QVERIFY2( shader->checkUniformBlockLayout( QgsRhiBinding::Lights, members, &error ), error.toUtf8() );
}

void TestQgsPhongMaterial3D::testLightUniforms()
{
  QgsLightUniforms lights;
  QCOMPARE( lights.lightCount, 0 );

  QVERIFY( lights.addDirectionalLight( QVector3D( 0, -2, 0 ), QColor( 255, 255, 255 ), 1.0f ) );
  QCOMPARE( lights.lightCount, 1 );

  // the direction is normalised on the way in, so shaders never have to
  QCOMPARE( lights.lights[0].directionAndCutOff[1], -1.0f );
  QCOMPARE( lights.lights[0].positionAndType[3], float( QgsLightUniforms::Type::Directional ) );
  QCOMPARE( lights.lights[0].colorAndIntensity[3], 1.0f );

  QVERIFY( lights.addPointLight( QVector3D( 1, 2, 3 ), QColor( 255, 0, 0 ), 2.0f, 1.0f, 0.0f, 0.0f ) );
  QCOMPARE( lights.lightCount, 2 );
  QCOMPARE( lights.lights[1].positionAndType[0], 1.0f );
  QCOMPARE( lights.lights[1].positionAndType[3], float( QgsLightUniforms::Type::Point ) );
  QCOMPARE( lights.lights[1].colorAndIntensity[0], 1.0f );
  QCOMPARE( lights.lights[1].attenuation[0], 1.0f );

  QVERIFY( lights.addSpotLight( QVector3D( 0, 5, 0 ), QVector3D( 0, -1, 0 ), 30.0f, QColor( 0, 255, 0 ), 1.0f, 1.0f, 0.0f, 0.0f ) );
  QCOMPARE( lights.lights[2].directionAndCutOff[3], 30.0f );

  // filling up must refuse rather than write past the end of the array
  while ( lights.lightCount < QgsLightUniforms::MAX_LIGHTS )
    QVERIFY( lights.addDirectionalLight( QVector3D( 0, -1, 0 ), Qt::white, 1.0f ) );

  QCOMPARE( lights.lightCount, QgsLightUniforms::MAX_LIGHTS );
  QVERIFY( !lights.addDirectionalLight( QVector3D( 0, -1, 0 ), Qt::white, 1.0f ) );
  QCOMPARE( lights.lightCount, QgsLightUniforms::MAX_LIGHTS );

  lights.clear();
  QCOMPARE( lights.lightCount, 0 );
}

void TestQgsPhongMaterial3D::testTessellatedGeometryLayout()
{
  // positions only
  {
    const QgsTessellatedPolygonGeometry3D geometry( false, false, false );
    QCOMPARE( geometry.stride(), 3 * int( sizeof( float ) ) );
    QCOMPARE( geometry.geometry()->attributes().size(), 1 );
    QVERIFY( geometry.geometry()->isIndexed() );
  }

  // positions and normals, which is what a polygon symbol normally asks for
  {
    const QgsTessellatedPolygonGeometry3D geometry( true, false, false );
    QCOMPARE( geometry.stride(), 6 * int( sizeof( float ) ) );

    const QgsVertexAttribute *position = geometry.geometry()->attribute( u"vertexPosition"_s );
    const QgsVertexAttribute *normal = geometry.geometry()->attribute( u"vertexNormal"_s );
    QVERIFY( position );
    QVERIFY( normal );
    QCOMPARE( position->byteOffset, 0u );
    QCOMPARE( normal->byteOffset, 3 * sizeof( float ) );
    QCOMPARE( position->byteStride, 6 * sizeof( float ) );
    QCOMPARE( normal->byteStride, 6 * sizeof( float ) );
  }

  // the full set, in the order QgsTessellator writes them
  {
    const QgsTessellatedPolygonGeometry3D geometry( true, true, true );
    QCOMPARE( geometry.stride(), 12 * int( sizeof( float ) ) );
    QCOMPARE( geometry.geometry()->attribute( u"vertexTangent"_s )->byteOffset, 6 * sizeof( float ) );
    QCOMPARE( geometry.geometry()->attribute( u"vertexTexCoord"_s )->byteOffset, 10 * sizeof( float ) );
  }
}

void TestQgsPhongMaterial3D::testTessellatedGeometryFeatureIds()
{
  QgsTessellatedPolygonGeometry3D geometry;
  geometry.setTriangleIndex( { 10, 20, 30 }, { 0u, 4u, 9u } );

  QCOMPARE( geometry.triangleIndexToFeatureId( 0 ), 10 );
  QCOMPARE( geometry.triangleIndexToFeatureId( 3 ), 10 );
  QCOMPARE( geometry.triangleIndexToFeatureId( 4 ), 20 );
  QCOMPARE( geometry.triangleIndexToFeatureId( 8 ), 20 );
  QCOMPARE( geometry.triangleIndexToFeatureId( 9 ), 30 );

  // anything past the last feature still belongs to it, matching the Qt3D implementation
  QCOMPARE( geometry.triangleIndexToFeatureId( 1000 ), 30 );

  QCOMPARE( geometry.featureIds().size(), 3 );
  QCOMPARE( geometry.triangleIndexStartingIndices().size(), 3 );
}

void TestQgsPhongMaterial3D::testLitPolygon()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  const std::unique_ptr<QgsRhiEngine> engine = makeEngine();
  QVERIFY( engine );
  QgsGraphicsPipelineCache cache;

  QgsTessellatedPolygonGeometry3D geometry;
  tessellateSquare( geometry );
  QVERIFY( geometry.geometry()->drawCount() > 0 );

  QgsPhongMaterial3D material;
  material.setCullMode( QRhiGraphicsPipeline::None );
  material.setAmbient( Qt::black );
  material.setDiffuse( QColor( 255, 0, 0 ) );
  material.setShininess( 0.0f ); // no specular, so the expected colour is exact

  QgsRenderPass pass( u"phong"_s );
  setTopDownCamera( pass );
  pass.setClearColor( QColor( 0, 0, 255 ) );

  // a white light shining straight down onto a surface whose normal points straight up
  QgsLightUniforms lights;
  lights.addDirectionalLight( QVector3D( 0.0f, 0.0f, -1.0f ), QColor( 255, 255, 255 ), 1.0f );
  pass.setLights( lights );

  QgsRenderCommand command;
  command.geometry = geometry.geometry();
  command.material = &material;
  pass.addCommand( command );

  engine->setReadbackEnabled( true );
  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();

  const QImage image = engine->readbackImage();

  // fully lit, so the surface shows its diffuse colour exactly
  QVERIFY2( colorsClose( centerPixel( image ), QColor( 255, 0, 0 ) ), centerPixel( image ).name().toUtf8() );

  // and the background is still there outside the square
  QVERIFY2( colorsClose( image.pixelColor( 1, 1 ), QColor( 0, 0, 255 ) ), image.pixelColor( 1, 1 ).name().toUtf8() );

  pass.releaseGpuResources();
  material.releaseGpuResources();
  geometry.releaseGpuResources();
  cache.clear();
}

void TestQgsPhongMaterial3D::testLightDirectionMatters()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  const std::unique_ptr<QgsRhiEngine> engine = makeEngine();
  QVERIFY( engine );
  QgsGraphicsPipelineCache cache;

  QgsTessellatedPolygonGeometry3D geometry;
  tessellateSquare( geometry );

  QgsPhongMaterial3D material;
  material.setCullMode( QRhiGraphicsPipeline::None );
  material.setAmbient( Qt::black );
  material.setDiffuse( QColor( 255, 0, 0 ) );
  material.setShininess( 0.0f );

  QgsRenderPass pass( u"phong"_s );
  setTopDownCamera( pass );
  pass.setClearColor( QColor( 0, 0, 255 ) );

  QgsRenderCommand command;
  command.geometry = geometry.geometry();
  command.material = &material;
  pass.addCommand( command );

  // light shining away from the surface leaves it unlit
  QgsLightUniforms lights;
  lights.addDirectionalLight( QVector3D( 0.0f, 0.0f, 1.0f ), QColor( 255, 255, 255 ), 1.0f );
  pass.setLights( lights );

  engine->setReadbackEnabled( true );
  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();

  QVERIFY2( colorsClose( centerPixel( engine->readbackImage() ), QColor( 0, 0, 0 ) ), "a surface facing away from the light should only show ambient" );

  // half intensity halves the shaded colour
  lights.clear();
  lights.addDirectionalLight( QVector3D( 0.0f, 0.0f, -1.0f ), QColor( 255, 255, 255 ), 0.5f );
  pass.setLights( lights );

  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();

  QVERIFY2( colorsClose( centerPixel( engine->readbackImage() ), QColor( 128, 0, 0 ), 2 ), centerPixel( engine->readbackImage() ).name().toUtf8() );

  pass.releaseGpuResources();
  material.releaseGpuResources();
  geometry.releaseGpuResources();
  cache.clear();
}

void TestQgsPhongMaterial3D::testAmbientOnlyWithoutLights()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  const std::unique_ptr<QgsRhiEngine> engine = makeEngine();
  QVERIFY( engine );
  QgsGraphicsPipelineCache cache;

  QgsTessellatedPolygonGeometry3D geometry;
  tessellateSquare( geometry );

  QgsPhongMaterial3D material;
  material.setCullMode( QRhiGraphicsPipeline::None );
  material.setAmbient( QColor( 0, 255, 0 ) );
  material.setDiffuse( QColor( 255, 0, 0 ) );

  QgsRenderPass pass( u"phong"_s );
  setTopDownCamera( pass );
  pass.setClearColor( Qt::black );
  // no lights at all: only the ambient term survives

  QgsRenderCommand command;
  command.geometry = geometry.geometry();
  command.material = &material;
  pass.addCommand( command );

  engine->setReadbackEnabled( true );
  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();

  QVERIFY2( colorsClose( centerPixel( engine->readbackImage() ), QColor( 0, 255, 0 ) ), centerPixel( engine->readbackImage() ).name().toUtf8() );

  pass.releaseGpuResources();
  material.releaseGpuResources();
  geometry.releaseGpuResources();
  cache.clear();
}

void TestQgsPhongMaterial3D::testOpacity()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  const std::unique_ptr<QgsRhiEngine> engine = makeEngine();
  QVERIFY( engine );
  QgsGraphicsPipelineCache cache;

  QgsTessellatedPolygonGeometry3D geometry;
  tessellateSquare( geometry );

  QgsPhongMaterial3D material;
  material.setCullMode( QRhiGraphicsPipeline::None );
  material.setAmbient( QColor( 255, 0, 0 ) );
  material.setDiffuse( QColor( 255, 0, 0 ) );
  material.setOpacity( 0.5f );

  QRhiGraphicsPipeline::TargetBlend blend;
  blend.enable = true;
  blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
  blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
  material.setBlend( blend );
  material.setDepthWrite( false );

  QgsRenderPass pass( u"phong"_s );
  setTopDownCamera( pass );
  pass.setClearColor( QColor( 0, 0, 255 ) );

  QgsRenderCommand command;
  command.geometry = geometry.geometry();
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
  geometry.releaseGpuResources();
  cache.clear();
}

void TestQgsPhongMaterial3D::testDataDefinedVariant()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  const std::unique_ptr<QgsRhiEngine> engine = makeEngine();
  QVERIFY( engine );
  QgsGraphicsPipelineCache cache;

  // one flat quad, every vertex carrying the same per-vertex colours, so the interpolated
  // centre is exactly what was written
  const float vertices[] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 10.0f, 0.0f,  0.0f, 0.0f, 0.0f, 1.0f, 10.0f, 10.0f, 0.0f, 0.0f, 0.0f, 1.0f,
                             0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 10.0f, 10.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,  10.0f, 0.0f, 0.0f, 0.0f, 1.0f };
  QgsGpuBuffer vertexBuffer( QgsGpuBuffer::Role::Vertex );
  vertexBuffer.setData( QByteArray( reinterpret_cast<const char *>( vertices ), sizeof( vertices ) ) );

  // ambient black, diffuse green, specular black, repeated for all six vertices, packed
  // the way the geometry generators do it
  const QStringList colorNames { u"dataDefinedAmbientColor"_s, u"dataDefinedDiffuseColor"_s, u"dataDefinedSpecularColor"_s };
  const int colorStride = QgsMaterial3DUtils::dataDefinedColorStride( colorNames.size(), false );

  QByteArray colors;
  colors.resize( 6 * colorStride );
  for ( int i = 0; i < 6; ++i )
    QgsMaterial3DUtils::writeDataDefinedColors( colors.data() + i * colorStride, { Qt::black, QColor( 0, 255, 0 ), Qt::black }, false );

  QgsGpuBuffer colorBuffer( QgsGpuBuffer::Role::Vertex );
  colorBuffer.setData( colors );

  QgsGeometry3D geometry;
  geometry.addAttribute( { u"vertexPosition"_s, &vertexBuffer, QRhiVertexInputAttribute::Float3, 0, 6 * sizeof( float ), 0 } );
  geometry.addAttribute( { u"vertexNormal"_s, &vertexBuffer, QRhiVertexInputAttribute::Float3, 3 * sizeof( float ), 6 * sizeof( float ), 0 } );
  QgsMaterial3DUtils::addDataDefinedColorAttributes( &geometry, &colorBuffer, colorNames, false );
  geometry.setVertexCount( 6 );

  QgsPhongMaterial3D material( true );
  QVERIFY( material.isDataDefined() );
  material.setCullMode( QRhiGraphicsPipeline::None );
  material.setShininess( 0.0f );

  // the uniform colours must be ignored entirely by this variant
  material.setAmbient( QColor( 255, 0, 0 ) );
  material.setDiffuse( QColor( 255, 0, 0 ) );

  QgsRenderPass pass( u"phong data defined"_s );
  setTopDownCamera( pass );
  pass.setClearColor( QColor( 0, 0, 255 ) );

  QgsLightUniforms lights;
  lights.addDirectionalLight( QVector3D( 0.0f, 0.0f, -1.0f ), QColor( 255, 255, 255 ), 1.0f );
  pass.setLights( lights );

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

void TestQgsPhongMaterial3D::testInstancedVariant()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  const std::unique_ptr<QgsRhiEngine> engine = makeEngine();
  QVERIFY( engine );
  if ( !engine->rhi()->isFeatureSupported( QRhi::Instancing ) )
    QSKIP( "The device does not support instanced drawing" );

  QgsGraphicsPipelineCache cache;

  // a small flat quad in the XY plane, drawn once per instance
  const float vertices[] = { -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f,  1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
                             -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,  0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f };
  QgsGpuBuffer vertexBuffer( QgsGpuBuffer::Role::Vertex );
  vertexBuffer.setData( QByteArray( reinterpret_cast<const char *>( vertices ), sizeof( vertices ) ) );

  const float translations[] = { 2.0f, 2.0f, 0.0f, 8.0f, 8.0f, 0.0f };
  QgsGpuBuffer instanceBuffer( QgsGpuBuffer::Role::Vertex );
  instanceBuffer.setData( QByteArray( reinterpret_cast<const char *>( translations ), sizeof( translations ) ) );

  QgsGeometry3D geometry;
  geometry.addAttribute( { u"vertexPosition"_s, &vertexBuffer, QRhiVertexInputAttribute::Float3, 0, 6 * sizeof( float ), 0 } );
  geometry.addAttribute( { u"vertexNormal"_s, &vertexBuffer, QRhiVertexInputAttribute::Float3, 3 * sizeof( float ), 6 * sizeof( float ), 0 } );
  geometry.addAttribute( { u"instanceTranslation"_s, &instanceBuffer, QRhiVertexInputAttribute::Float3, 0, 0, 1 } );
  geometry.setVertexCount( 6 );

  QgsPhongMaterial3D material( false, true );
  QVERIFY( material.isInstanced() );
  material.setCullMode( QRhiGraphicsPipeline::None );
  material.setAmbient( QColor( 255, 0, 0 ) );
  material.setDiffuse( Qt::black );

  QgsRenderPass pass( u"phong instanced"_s );
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

  // both instances landed, on opposite sides of the centre, and the centre between them
  // stayed empty
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
  QVERIFY2( colorsClose( centerPixel( image ), QColor( 0, 0, 0 ) ), "nothing should sit at the centre" );

  pass.releaseGpuResources();
  material.releaseGpuResources();
  cache.clear();
}

QGSTEST_MAIN( TestQgsPhongMaterial3D )
#include "testqgsphongmaterial3d.moc"
