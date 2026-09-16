/***************************************************************************
     testqgsrhiengine.cpp
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
#include "qgsgputexture.h"
#include "qgsgraphicspipelinecache.h"
#include "qgsmaterial3d.h"
#include "qgsrendercommand.h"
#include "qgsrenderpass.h"
#include "qgsrenderuniforms.h"
#include "qgsrhiengine.h"
#include "qgsrhishader.h"
#include "qgstest.h"
#include "qgstexturematerial3d.h"
#include "qgsuniformring.h"
#include "qgsunlitmaterial3d.h"

#include <QColor>
#include <QImage>
#include <QString>

using namespace Qt::StringLiterals;

namespace
{
  //! Colour of the pixel at the centre of \a image, for checking what was drawn
  QColor centerPixel( const QImage &image )
  {
    return image.pixelColor( image.width() / 2, image.height() / 2 );
  }

  /**
   * TRUE if two colours are within \a tolerance on every channel
   * The unlit material's uniform block, filled with a solid colour and nothing else
   */
  QgsUnlitUniforms unlitColor( float red, float green, float blue )
  {
    QgsUnlitUniforms uniforms;
    uniforms.materialColor[0] = red;
    uniforms.materialColor[1] = green;
    uniforms.materialColor[2] = blue;
    uniforms.materialColor[3] = 1.0f;
    return uniforms;
  }

  bool colorsClose( const QColor &a, const QColor &b, int tolerance = 4 )
  {
    return std::abs( a.red() - b.red() ) <= tolerance && std::abs( a.green() - b.green() ) <= tolerance && std::abs( a.blue() - b.blue() ) <= tolerance && std::abs( a.alpha() - b.alpha() ) <= tolerance;
  }
} // namespace

/**
 * \ingroup UnitTests
 * Unit tests for the QRhi based 3D engine.
 *
 * Everything here renders offscreen, which under QRhi needs no window and no particular
 * platform plugin, so these run unchanged under QT_QPA_PLATFORM=offscreen.
 */
class TestQgsRhiEngine : public QgsTest
{
    Q_OBJECT
  public:
    TestQgsRhiEngine()
      : QgsTest( u"QgsRhiEngine tests"_s )
    {}

  private slots:
    void initTestCase();
    void cleanupTestCase();

    void testBackendNames();
    void testSurfaceTypes();
    void testCreateOffscreen();
    void testClearOnly();
    void testUniformBlockLayouts();
    void testUniformRing();
    void testGeometryLayoutKey();
    void testDrawTriangle();
    void testVertexColors();
    void testTexturedQuad();
    void testInstancedDraw();
    void testDepthTest();
    void testPipelineSharing();
    void testMultisampleResolve();
    void testResize();

  private:
    //! Builds an engine rendering offscreen, skipping the test if no device can be created
    std::unique_ptr<QgsRhiEngine> makeEngine( QSize size = QSize( 64, 64 ), int sampleCount = 1 );

    //! A single triangle covering the middle of the viewport, in the XY plane at z = 0
    std::unique_ptr<QgsGpuBuffer> makeTriangleBuffer();

    //! Sets an identity camera, so that clip space coordinates can be fed in directly
    static void setIdentityCamera( QgsRenderPass &pass );

    /**
     * Skips the calling test if the backend does not actually rasterize anything.
     *
     * The Null backend creates every resource and accepts every command but draws no
     * pixels, which is exactly what makes it useful for testing resource handling. Tests
     * that look at the resulting image have nothing to look at there.
     */
    bool skipIfNoPixels() const;

    QRhi::Implementation mBackend = QRhi::Null;
};

std::unique_ptr<QgsRhiEngine> TestQgsRhiEngine::makeEngine( QSize size, int sampleCount )
{
  auto engine = std::make_unique<QgsRhiEngine>();
  if ( !engine->createOffscreen( size, mBackend, sampleCount ) )
    return nullptr;
  return engine;
}

std::unique_ptr<QgsGpuBuffer> TestQgsRhiEngine::makeTriangleBuffer()
{
  const float vertices[] = { -0.8f, -0.8f, 0.0f, 0.8f, -0.8f, 0.0f, 0.0f, 0.8f, 0.0f };

  auto buffer = std::make_unique<QgsGpuBuffer>( QgsGpuBuffer::Role::Vertex );
  buffer->setData( QByteArray( reinterpret_cast<const char *>( vertices ), sizeof( vertices ) ) );
  return buffer;
}

void TestQgsRhiEngine::setIdentityCamera( QgsRenderPass &pass )
{
  pass.setCamera( QMatrix4x4(), QMatrix4x4() );
}

bool TestQgsRhiEngine::skipIfNoPixels() const
{
  return mBackend == QRhi::Null;
}

void TestQgsRhiEngine::initTestCase()
{
  mBackend = QgsRhiEngine::defaultBackend();

  // if no device can be created at all there is no point running any of this, which is
  // the situation on a machine with no usable GPU or driver
  QgsRhiEngine probe;
  if ( !probe.createOffscreen( QSize( 4, 4 ), mBackend ) )
    QSKIP( "No usable QRhi device for the default backend" );
}

void TestQgsRhiEngine::cleanupTestCase()
{
  QgsRhiShader::clearCache();
  QgsApplication::exitQgis();
}

void TestQgsRhiEngine::testBackendNames()
{
  bool ok = false;
  QCOMPARE( QgsRhiEngine::backendFromName( u"opengl"_s, &ok ), QRhi::OpenGLES2 );
  QVERIFY( ok );
  QCOMPARE( QgsRhiEngine::backendFromName( u"Metal"_s, &ok ), QRhi::Metal );
  QVERIFY( ok );
  QCOMPARE( QgsRhiEngine::backendFromName( u" vulkan "_s, &ok ), QRhi::Vulkan );
  QVERIFY( ok );
  QCOMPARE( QgsRhiEngine::backendFromName( u"d3d11"_s, &ok ), QRhi::D3D11 );
  QVERIFY( ok );

  QgsRhiEngine::backendFromName( u"nonsense"_s, &ok );
  QVERIFY( !ok );
}

void TestQgsRhiEngine::testSurfaceTypes()
{
  // a window whose surface type does not match the backend cannot carry a swapchain,
  // so this mapping is what makes the on screen path work on each platform
  QCOMPARE( QgsRhiEngine::surfaceTypeForBackend( QRhi::OpenGLES2 ), QSurface::OpenGLSurface );
  QCOMPARE( QgsRhiEngine::surfaceTypeForBackend( QRhi::Vulkan ), QSurface::VulkanSurface );
  QCOMPARE( QgsRhiEngine::surfaceTypeForBackend( QRhi::Metal ), QSurface::MetalSurface );
  QCOMPARE( QgsRhiEngine::surfaceTypeForBackend( QRhi::D3D11 ), QSurface::Direct3DSurface );
  QCOMPARE( QgsRhiEngine::surfaceTypeForBackend( QRhi::D3D12 ), QSurface::Direct3DSurface );
}

void TestQgsRhiEngine::testCreateOffscreen()
{
  const std::unique_ptr<QgsRhiEngine> engine = makeEngine();
  QVERIFY( engine );
  QVERIFY( engine->isValid() );
  QVERIFY( !engine->backendName().isEmpty() );
  QCOMPARE( engine->outputSize(), QSize( 64, 64 ) );
  QVERIFY( engine->colorTexture() );
  QVERIFY( engine->renderTarget() );
  QVERIFY( engine->renderPassDescriptor() );

  // without multisampling there is nothing to resolve, so both point at one texture
  QCOMPARE( engine->resolvedColorTexture(), engine->colorTexture() );
}

void TestQgsRhiEngine::testClearOnly()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  const std::unique_ptr<QgsRhiEngine> engine = makeEngine();
  QVERIFY( engine );

  engine->setReadbackEnabled( true );
  QVERIFY( engine->beginFrame() );
  engine->commandBuffer()->beginPass( engine->renderTarget(), QColor( 12, 34, 56 ), { 1.0f, 0 } );
  engine->commandBuffer()->endPass();
  engine->endFrame();

  const QImage image = engine->readbackImage();
  QCOMPARE( image.size(), QSize( 64, 64 ) );
  QVERIFY( colorsClose( centerPixel( image ), QColor( 12, 34, 56 ) ) );
}

void TestQgsRhiEngine::testUniformBlockLayouts()
{
  // the C++ structs and the GLSL blocks are written by hand in two different files, and a
  // disagreement between them corrupts every value after the first wrong offset without
  // any error, so the two are compared member by member here
  const std::shared_ptr<QgsRhiShader> shader = QgsRhiShader::load( u":/shaders/rhi/unlit.vert.qsb"_s, u":/shaders/rhi/unlit.frag.qsb"_s );
  QVERIFY( shader );

  QCOMPARE( shader->uniformBlockSize( QgsRhiBinding::View ), int( sizeof( QgsViewUniforms ) ) );
  QCOMPARE( shader->uniformBlockSize( QgsRhiBinding::Draw ), int( sizeof( QgsDrawUniforms ) ) );

  QString error;
  const QVector<QgsUniformMember> viewMembers {
    { u"viewMatrix"_s, offsetof( QgsViewUniforms, viewMatrix ), 64 },
    { u"projectionMatrix"_s, offsetof( QgsViewUniforms, projectionMatrix ), 64 },
    { u"viewProjectionMatrix"_s, offsetof( QgsViewUniforms, viewProjectionMatrix ), 64 },
    { u"inverseViewMatrix"_s, offsetof( QgsViewUniforms, inverseViewMatrix ), 64 },
    { u"inverseProjectionMatrix"_s, offsetof( QgsViewUniforms, inverseProjectionMatrix ), 64 },
    { u"eyePosition"_s, offsetof( QgsViewUniforms, eyePosition ), 12 },
    { u"nearPlane"_s, offsetof( QgsViewUniforms, nearPlane ), 4 },
    { u"farPlane"_s, offsetof( QgsViewUniforms, farPlane ), 4 },
    { u"aspectRatio"_s, offsetof( QgsViewUniforms, aspectRatio ), 4 },
    { u"viewportWidth"_s, offsetof( QgsViewUniforms, viewportWidth ), 4 },
    { u"viewportHeight"_s, offsetof( QgsViewUniforms, viewportHeight ), 4 },
  };
  QVERIFY2( shader->checkUniformBlockLayout( QgsRhiBinding::View, viewMembers, &error ), error.toUtf8() );

  const QVector<QgsUniformMember> drawMembers {
    { u"modelMatrix"_s, offsetof( QgsDrawUniforms, modelMatrix ), 64 },
    { u"modelViewProjectionMatrix"_s, offsetof( QgsDrawUniforms, modelViewProjectionMatrix ), 64 },
    { u"modelNormalMatrix"_s, offsetof( QgsDrawUniforms, modelNormalMatrix ), 48 },
  };
  QVERIFY2( shader->checkUniformBlockLayout( QgsRhiBinding::Draw, drawMembers, &error ), error.toUtf8() );

  // a member the shader does not have must be reported rather than silently accepted
  const QVector<QgsUniformMember> bogus { { u"noSuchUniform"_s, 0, 4 } };
  QVERIFY( !shader->checkUniformBlockLayout( QgsRhiBinding::View, bogus, &error ) );
  QVERIFY( !error.isEmpty() );

  // and so must a member at the wrong offset
  const QVector<QgsUniformMember> misplaced { { u"farPlane"_s, 0, 4 } };
  QVERIFY( !shader->checkUniformBlockLayout( QgsRhiBinding::View, misplaced, &error ) );
}

void TestQgsRhiEngine::testUniformRing()
{
  const std::unique_ptr<QgsRhiEngine> engine = makeEngine();
  QVERIFY( engine );

  QgsUniformRing ring;
  ring.reset( engine->rhi(), sizeof( QgsDrawUniforms ) );

  // slices are spaced at the device's uniform buffer alignment, which is at least as
  // large as the block itself
  QVERIFY( ring.elementSize() >= sizeof( QgsDrawUniforms ) );
  QCOMPARE( ring.elementSize() % engine->rhi()->ubufAlignment(), 0u );

  QgsDrawUniforms uniforms;
  QCOMPARE( ring.append( &uniforms ), 0u );
  QCOMPARE( ring.append( &uniforms ), ring.elementSize() );
  QCOMPARE( ring.append( &uniforms ), 2 * ring.elementSize() );
  QCOMPARE( ring.count(), 3 );

  ring.reset( engine->rhi(), sizeof( QgsDrawUniforms ) );
  QCOMPARE( ring.count(), 0 );
  QCOMPARE( ring.append( &uniforms ), 0u );

  ring.releaseGpuResources();
}

void TestQgsRhiEngine::testGeometryLayoutKey()
{
  QgsGpuBuffer buffer( QgsGpuBuffer::Role::Vertex );
  buffer.setData( QByteArray( 36, '\0' ) );

  QgsGeometry3D a;
  a.addAttribute( { u"vertexPosition"_s, &buffer, QRhiVertexInputAttribute::Float3, 0, 0, 0 } );

  QgsGeometry3D b;
  b.addAttribute( { u"vertexPosition"_s, &buffer, QRhiVertexInputAttribute::Float3, 0, 0, 0 } );

  // the same layout must share a pipeline, so the keys have to agree
  QCOMPARE( a.layoutKey(), b.layoutKey() );

  QgsGeometry3D c;
  c.addAttribute( { u"vertexPosition"_s, &buffer, QRhiVertexInputAttribute::Float2, 0, 0, 0 } );
  QVERIFY( a.layoutKey() != c.layoutKey() );

  // adding an attribute changes the layout, and must invalidate the cached key
  b.addAttribute( { u"vertexColor"_s, &buffer, QRhiVertexInputAttribute::Float4, 0, 0, 0 } );
  QVERIFY( a.layoutKey() != b.layoutKey() );

  QCOMPARE( a.attribute( u"vertexPosition"_s )->format, QRhiVertexInputAttribute::Float3 );
  QVERIFY( !a.attribute( u"missing"_s ) );
}

void TestQgsRhiEngine::testDrawTriangle()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  const std::unique_ptr<QgsRhiEngine> engine = makeEngine();
  QVERIFY( engine );
  QgsGraphicsPipelineCache cache;

  const std::unique_ptr<QgsGpuBuffer> buffer = makeTriangleBuffer();

  QgsGeometry3D geometry;
  geometry.addAttribute( { u"vertexPosition"_s, buffer.get(), QRhiVertexInputAttribute::Float3, 0, 0, 0 } );
  geometry.setVertexCount( 3 );

  const std::shared_ptr<QgsRhiShader> shader = QgsRhiShader::load( u":/shaders/rhi/unlit.vert.qsb"_s, u":/shaders/rhi/unlit.frag.qsb"_s );
  QVERIFY( shader );

  QgsMaterial3D material( shader );
  material.setCullMode( QRhiGraphicsPipeline::None );
  material.setUniforms( unlitColor( 1.0f, 0.0f, 0.0f ) );

  QgsRenderPass pass( u"triangle"_s );
  setIdentityCamera( pass );
  pass.setClearColor( QColor( 0, 0, 255 ) );

  QgsRenderCommand command;
  command.geometry = &geometry;
  command.material = &material;
  pass.addCommand( command );

  engine->setReadbackEnabled( true );
  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();

  const QImage image = engine->readbackImage();
  QCOMPARE( image.size(), QSize( 64, 64 ) );

  // red in the middle where the triangle is, blue in the top left corner where it is not
  QVERIFY2( colorsClose( centerPixel( image ), QColor( 255, 0, 0 ) ), centerPixel( image ).name().toUtf8() );
  QVERIFY2( colorsClose( image.pixelColor( 1, 1 ), QColor( 0, 0, 255 ) ), image.pixelColor( 1, 1 ).name().toUtf8() );

  pass.releaseGpuResources();
  material.releaseGpuResources();
  cache.clear();
}

void TestQgsRhiEngine::testVertexColors()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  const std::unique_ptr<QgsRhiEngine> engine = makeEngine();
  QVERIFY( engine );
  QgsGraphicsPipelineCache cache;

  const std::unique_ptr<QgsGpuBuffer> positions = makeTriangleBuffer();

  // every vertex green, so the interpolated centre is green too
  const float colors[] = { 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f };
  QgsGpuBuffer colorBuffer( QgsGpuBuffer::Role::Vertex );
  colorBuffer.setData( QByteArray( reinterpret_cast<const char *>( colors ), sizeof( colors ) ) );

  QgsGeometry3D geometry;
  geometry.addAttribute( { u"vertexPosition"_s, positions.get(), QRhiVertexInputAttribute::Float3, 0, 0, 0 } );
  geometry.addAttribute( { u"vertexColor"_s, &colorBuffer, QRhiVertexInputAttribute::Float4, 0, 0, 0 } );
  geometry.setVertexCount( 3 );

  const std::shared_ptr<QgsRhiShader> shader = QgsRhiShader::load( u":/shaders/rhi/unlit_vertexcolor.vert.qsb"_s, u":/shaders/rhi/unlit.frag.qsb"_s );
  QVERIFY2( shader, "the VERTEX_COLOR shader variant was not baked" );

  QgsMaterial3D material( shader );
  material.setCullMode( QRhiGraphicsPipeline::None );
  material.setUniforms( unlitColor( 1.0f, 1.0f, 1.0f ) );

  QgsRenderPass pass( u"vertex colors"_s );
  setIdentityCamera( pass );
  pass.setClearColor( Qt::black );

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

void TestQgsRhiEngine::testTexturedQuad()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  const std::unique_ptr<QgsRhiEngine> engine = makeEngine();
  QVERIFY( engine );
  QgsGraphicsPipelineCache cache;

  // a quad covering the whole viewport, as two triangles
  const float vertices[] = { -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 1.0f,  1.0f, 0.0f, 1.0f, 0.0f,
                             -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,  0.0f, 1.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f };
  QgsGpuBuffer buffer( QgsGpuBuffer::Role::Vertex );
  buffer.setData( QByteArray( reinterpret_cast<const char *>( vertices ), sizeof( vertices ) ) );

  // position and texture coordinate interleaved in one buffer, which is the layout most
  // of QGIS's geometry generators already produce
  QgsGeometry3D geometry;
  geometry.addAttribute( { u"vertexPosition"_s, &buffer, QRhiVertexInputAttribute::Float3, 0, 5 * sizeof( float ), 0 } );
  geometry.addAttribute( { u"vertexTexCoord"_s, &buffer, QRhiVertexInputAttribute::Float2, 3 * sizeof( float ), 5 * sizeof( float ), 0 } );
  geometry.setVertexCount( 6 );

  QImage source( 4, 4, QImage::Format_RGBA8888 );
  source.fill( QColor( 20, 180, 90 ) );
  QgsGpuTexture texture;
  texture.setImage( source );

  const std::shared_ptr<QgsRhiShader> shader = QgsRhiShader::load( u":/shaders/rhi/textured.vert.qsb"_s, u":/shaders/rhi/textured.frag.qsb"_s );
  QVERIFY( shader );

  const QVector<QShaderDescription::InOutVariable> samplers = shader->samplers();
  QCOMPARE( samplers.size(), 1 );
  QCOMPARE( samplers.at( 0 ).binding, QgsRhiBinding::FirstSampler );

  QgsMaterial3D material( shader );
  material.setCullMode( QRhiGraphicsPipeline::None );
  QgsTexturedUniforms texturedUniforms;
  texturedUniforms.opacity = 1.0f;
  material.setUniforms( texturedUniforms );
  material.setTexture( { QgsRhiBinding::FirstSampler, &texture } );

  QgsRenderPass pass( u"textured"_s );
  setIdentityCamera( pass );

  QgsRenderCommand command;
  command.geometry = &geometry;
  command.material = &material;
  pass.addCommand( command );

  engine->setReadbackEnabled( true );
  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();

  QVERIFY2( colorsClose( centerPixel( engine->readbackImage() ), QColor( 20, 180, 90 ) ), centerPixel( engine->readbackImage() ).name().toUtf8() );

  pass.releaseGpuResources();
  material.releaseGpuResources();
  texture.releaseGpuResources();
  cache.clear();
}

void TestQgsRhiEngine::testInstancedDraw()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  const std::unique_ptr<QgsRhiEngine> engine = makeEngine();
  QVERIFY( engine );
  if ( !engine->rhi()->isFeatureSupported( QRhi::Instancing ) )
    QSKIP( "The device does not support instanced drawing" );
  QgsGraphicsPipelineCache cache;

  // a small triangle near the origin, moved into each quadrant by an instance attribute
  const float vertices[] = { -0.2f, -0.2f, 0.0f, 0.2f, -0.2f, 0.0f, 0.0f, 0.2f, 0.0f };
  QgsGpuBuffer positions( QgsGpuBuffer::Role::Vertex );
  positions.setData( QByteArray( reinterpret_cast<const char *>( vertices ), sizeof( vertices ) ) );

  const float translations[] = { -0.5f, 0.5f, 0.0f, 0.5f, 0.5f, 0.0f, -0.5f, -0.5f, 0.0f, 0.5f, -0.5f, 0.0f };
  QgsGpuBuffer instances( QgsGpuBuffer::Role::Vertex );
  instances.setData( QByteArray( reinterpret_cast<const char *>( translations ), sizeof( translations ) ) );

  QgsGeometry3D geometry;
  geometry.addAttribute( { u"vertexPosition"_s, &positions, QRhiVertexInputAttribute::Float3, 0, 0, 0 } );
  geometry.addAttribute( { u"instanceTranslation"_s, &instances, QRhiVertexInputAttribute::Float3, 0, 0, 1 } );
  geometry.setVertexCount( 3 );

  const std::shared_ptr<QgsRhiShader> shader = QgsRhiShader::load( u":/shaders/rhi/unlit_instanced.vert.qsb"_s, u":/shaders/rhi/unlit.frag.qsb"_s );
  QVERIFY2( shader, "the INSTANCING shader variant was not baked" );

  QgsMaterial3D material( shader );
  material.setCullMode( QRhiGraphicsPipeline::None );
  material.setUniforms( unlitColor( 1.0f, 1.0f, 0.0f ) );

  QgsRenderPass pass( u"instanced"_s );
  setIdentityCamera( pass );
  pass.setClearColor( Qt::black );

  QgsRenderCommand command;
  command.geometry = &geometry;
  command.material = &material;
  command.instanceCount = 4;
  pass.addCommand( command );

  engine->setReadbackEnabled( true );
  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();

  const QImage image = engine->readbackImage();

  // one triangle in each quadrant, and nothing in the middle where none of them reach
  QVERIFY2( colorsClose( image.pixelColor( 16, 16 ), QColor( 255, 255, 0 ) ), "top left instance missing" );
  QVERIFY2( colorsClose( image.pixelColor( 48, 16 ), QColor( 255, 255, 0 ) ), "top right instance missing" );
  QVERIFY2( colorsClose( image.pixelColor( 16, 48 ), QColor( 255, 255, 0 ) ), "bottom left instance missing" );
  QVERIFY2( colorsClose( image.pixelColor( 48, 48 ), QColor( 255, 255, 0 ) ), "bottom right instance missing" );
  QVERIFY2( colorsClose( centerPixel( image ), QColor( 0, 0, 0 ) ), "centre should be empty" );

  pass.releaseGpuResources();
  material.releaseGpuResources();
  cache.clear();
}

void TestQgsRhiEngine::testDepthTest()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  const std::unique_ptr<QgsRhiEngine> engine = makeEngine();
  QVERIFY( engine );
  QgsGraphicsPipelineCache cache;

  const std::unique_ptr<QgsGpuBuffer> buffer = makeTriangleBuffer();

  QgsGeometry3D geometry;
  geometry.addAttribute( { u"vertexPosition"_s, buffer.get(), QRhiVertexInputAttribute::Float3, 0, 0, 0 } );
  geometry.setVertexCount( 3 );

  const std::shared_ptr<QgsRhiShader> shader = QgsRhiShader::load( u":/shaders/rhi/unlit.vert.qsb"_s, u":/shaders/rhi/unlit.frag.qsb"_s );
  QVERIFY( shader );

  QgsMaterial3D nearMaterial( shader );
  nearMaterial.setCullMode( QRhiGraphicsPipeline::None );
  nearMaterial.setUniforms( unlitColor( 1.0f, 0.0f, 0.0f ) );

  QgsMaterial3D farMaterial( shader );
  farMaterial.setCullMode( QRhiGraphicsPipeline::None );
  farMaterial.setUniforms( unlitColor( 0.0f, 1.0f, 0.0f ) );

  QgsRenderPass pass( u"depth"_s );
  setIdentityCamera( pass );
  pass.setClearColor( Qt::black );
  pass.setSortEnabled( false );

  // the near triangle is drawn first, so the far one only shows up if depth testing is off
  QgsRenderCommand nearCommand;
  nearCommand.geometry = &geometry;
  nearCommand.material = &nearMaterial;
  nearCommand.worldTransform.translate( 0.0f, 0.0f, 0.2f );
  pass.addCommand( nearCommand );

  QgsRenderCommand farCommand;
  farCommand.geometry = &geometry;
  farCommand.material = &farMaterial;
  farCommand.worldTransform.translate( 0.0f, 0.0f, 0.8f );
  pass.addCommand( farCommand );

  engine->setReadbackEnabled( true );
  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();

  QVERIFY2( colorsClose( centerPixel( engine->readbackImage() ), QColor( 255, 0, 0 ) ), "the nearer triangle should have won the depth test" );

  pass.releaseGpuResources();
  nearMaterial.releaseGpuResources();
  farMaterial.releaseGpuResources();
  cache.clear();
}

void TestQgsRhiEngine::testPipelineSharing()
{
  const std::unique_ptr<QgsRhiEngine> engine = makeEngine();
  QVERIFY( engine );
  QgsGraphicsPipelineCache cache;

  const std::unique_ptr<QgsGpuBuffer> buffer = makeTriangleBuffer();

  QgsGeometry3D geometry;
  geometry.addAttribute( { u"vertexPosition"_s, buffer.get(), QRhiVertexInputAttribute::Float3, 0, 0, 0 } );
  geometry.setVertexCount( 3 );

  const std::shared_ptr<QgsRhiShader> shader = QgsRhiShader::load( u":/shaders/rhi/unlit.vert.qsb"_s, u":/shaders/rhi/unlit.frag.qsb"_s );
  QVERIFY( shader );

  // two materials differing only in colour: same shader, same state, so one pipeline
  QgsMaterial3D first( shader );
  first.setCullMode( QRhiGraphicsPipeline::None );
  first.setUniforms( unlitColor( 1.0f, 0.0f, 0.0f ) );

  QgsMaterial3D second( shader );
  second.setCullMode( QRhiGraphicsPipeline::None );
  second.setUniforms( unlitColor( 0.0f, 0.0f, 1.0f ) );

  QCOMPARE( first.stateKey(), second.stateKey() );

  // a third one differs in state, and must not share
  QgsMaterial3D third( shader );
  third.setCullMode( QRhiGraphicsPipeline::Back );
  third.setUniforms( unlitColor( 1.0f, 0.0f, 0.0f ) );
  QVERIFY( first.stateKey() != third.stateKey() );

  QgsRenderPass pass( u"sharing"_s );
  setIdentityCamera( pass );

  for ( QgsMaterial3D *material : { &first, &second, &third } )
  {
    QgsRenderCommand command;
    command.geometry = &geometry;
    command.material = material;
    pass.addCommand( command );
  }

  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();

  QCOMPARE( cache.pipelineCount(), 2 );

  // the binding sets differ per material, because each has its own uniform buffer
  QCOMPARE( cache.shaderResourceBindingsCount(), 3 );

  // running a second frame must reuse everything rather than build it again
  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();
  QCOMPARE( cache.pipelineCount(), 2 );
  QCOMPARE( cache.shaderResourceBindingsCount(), 3 );

  pass.releaseGpuResources();
  first.releaseGpuResources();
  second.releaseGpuResources();
  third.releaseGpuResources();
  cache.clear();
}

void TestQgsRhiEngine::testMultisampleResolve()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  // this is the case Qt3D's RHI renderer cannot do at all: it never sets a resolve texture
  // on an offscreen target, so multisampled offscreen rendering there needs either an
  // upstream patch, a manual resolve pass or supersampling. Here it is one API call.
  const std::unique_ptr<QgsRhiEngine> engine = makeEngine( QSize( 64, 64 ), 4 );
  if ( !engine || engine->sampleCount() < 2 )
    QSKIP( "The device does not support multisampled offscreen targets" );

  // the multisampled attachment is a renderbuffer, so the resolved texture is the only
  // one that can be sampled or read back
  QVERIFY( !engine->colorTexture() );
  QVERIFY( engine->resolvedColorTexture() );
  QgsGraphicsPipelineCache cache;

  const std::unique_ptr<QgsGpuBuffer> buffer = makeTriangleBuffer();

  QgsGeometry3D geometry;
  geometry.addAttribute( { u"vertexPosition"_s, buffer.get(), QRhiVertexInputAttribute::Float3, 0, 0, 0 } );
  geometry.setVertexCount( 3 );

  const std::shared_ptr<QgsRhiShader> shader = QgsRhiShader::load( u":/shaders/rhi/unlit.vert.qsb"_s, u":/shaders/rhi/unlit.frag.qsb"_s );
  QVERIFY( shader );

  QgsMaterial3D material( shader );
  material.setCullMode( QRhiGraphicsPipeline::None );
  material.setUniforms( unlitColor( 1.0f, 0.0f, 0.0f ) );

  QgsRenderPass pass( u"msaa"_s );
  setIdentityCamera( pass );
  pass.setClearColor( QColor( 0, 0, 255 ) );

  QgsRenderCommand command;
  command.geometry = &geometry;
  command.material = &material;
  pass.addCommand( command );

  engine->setReadbackEnabled( true );
  QVERIFY( engine->beginFrame() );
  QVERIFY( pass.execute( engine.get(), &cache ) );
  engine->endFrame();

  const QImage image = engine->readbackImage();
  QCOMPARE( image.size(), QSize( 64, 64 ) );
  QVERIFY( colorsClose( centerPixel( image ), QColor( 255, 0, 0 ) ) );

  // the point of multisampling: the triangle's sloped edge is no longer purely red or
  // purely blue, because the resolve averaged the samples that straddle it
  bool foundBlend = false;
  for ( int y = 0; y < image.height() && !foundBlend; ++y )
  {
    for ( int x = 0; x < image.width(); ++x )
    {
      const QColor pixel = image.pixelColor( x, y );
      if ( pixel.red() > 20 && pixel.red() < 235 && pixel.blue() > 20 && pixel.blue() < 235 )
      {
        foundBlend = true;
        break;
      }
    }
  }
  QVERIFY2( foundBlend, "no anti-aliased edge pixels found, was the MSAA resolve applied?" );

  pass.releaseGpuResources();
  material.releaseGpuResources();
  cache.clear();
}

void TestQgsRhiEngine::testResize()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  const std::unique_ptr<QgsRhiEngine> engine = makeEngine();
  QVERIFY( engine );

  QVERIFY( engine->setOutputSize( QSize( 32, 48 ) ) );
  QCOMPARE( engine->outputSize(), QSize( 32, 48 ) );
  QCOMPARE( engine->colorTexture()->pixelSize(), QSize( 32, 48 ) );

  engine->setReadbackEnabled( true );
  QVERIFY( engine->beginFrame() );
  engine->commandBuffer()->beginPass( engine->renderTarget(), QColor( 200, 100, 50 ), { 1.0f, 0 } );
  engine->commandBuffer()->endPass();
  engine->endFrame();

  const QImage image = engine->readbackImage();
  QCOMPARE( image.size(), QSize( 32, 48 ) );
  QVERIFY( colorsClose( centerPixel( image ), QColor( 200, 100, 50 ) ) );
}

QGSTEST_MAIN( TestQgsRhiEngine )
#include "testqgsrhiengine.moc"
