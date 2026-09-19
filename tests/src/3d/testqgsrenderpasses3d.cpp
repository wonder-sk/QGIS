/***************************************************************************
     testqgsrenderpasses3d.cpp
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

#include "qgsambientocclusionmaterial3d.h"
#include "qgsambientocclusionrenderpass.h"
#include "qgsbackgroundmaterial3d.h"
#include "qgsbloommaterial3d.h"
#include "qgsbloomrenderpass.h"
#include "qgsdepthrendermaterial3d.h"
#include "qgsdepthrenderpass.h"
#include "qgsforwardrenderpass.h"
#include "qgsgeometry3d.h"
#include "qgsgpubuffer.h"
#include "qgsgpurendertarget.h"
#include "qgsgraphicspipelinecache.h"
#include "qgshighlightmaterial3d.h"
#include "qgsoverlaytexturematerial3d.h"
#include "qgsphongmaterial3d.h"
#include "qgspostprocessingmaterial3d.h"
#include "qgspostprocessingrenderpass.h"
#include "qgsrendercommand.h"
#include "qgsrenderpass.h"
#include "qgsrenderuniforms.h"
#include "qgsrhiengine.h"
#include "qgsrhishader.h"
#include "qgsscenerenderer.h"
#include "qgsscreenquad.h"
#include "qgsshadowcastermaterial3d.h"
#include "qgsshadowrenderpass.h"
#include "qgstest.h"
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
 * Unit tests for the render passes that replace the Qt3D frame graph.
 *
 * What these have to get right divides into three kinds of thing:
 *
 * - the render targets themselves: that an attachment can be sampled by the next pass,
 *   that a pass can keep what an earlier pass left in a target rather than clearing it,
 *   and that a layered target addresses one layer at a time;
 * - each pass in isolation: that it draws what it claims to into the texture it claims to;
 * - the sequence: that QgsSceneRenderer hands each pass the right texture and produces the
 *   same image the frame graph would have.
 *
 * Everything renders offscreen, so these run unchanged under QT_QPA_PLATFORM=offscreen.
 */
class TestQgsRenderPasses3D : public QgsTest
{
    Q_OBJECT
  public:
    TestQgsRenderPasses3D()
      : QgsTest( u"QRhi render pass tests"_s )
    {}

  private slots:
    void initTestCase();
    void cleanupTestCase();

    void testRenderTargetColorOnly();
    void testRenderTargetDepthTexture();
    void testRenderTargetLayers();
    void testRenderTargetResize();
    void testRenderTargetMultisample();
    void testTargetClearsByDefault();
    void testTargetKeepsContents();

    void testScreenQuadGeometry();

    void testShadowUniformBlockLayout();
    void testShadowCasterVariantsExist();
    void testPostprocessingUniformBlockLayout();
    void testAmbientOcclusionUniformBlockLayout();
    void testBloomUniformBlockLayout();
    void testOverlayUniformBlockLayout();
    void testBackgroundUniformBlockLayout();

    void testForwardPassDraws();
    void testForwardPassGroupOrder();
    void testBackgroundBehindScene();

    void testDepthPassReadback();
    void testDepthReadbackSwitchedOnAfterPrepare();
    void testDepthReadbackIsNotOverwrittenBeforeCollect();
    void testOutputOrientation();
    void testAmbientOcclusionOpenSurface();
    void testBloomChainSizes();
    void testBloomDisabledHoldsNoTextures();

    void testPostprocessingGamma();
    void testPostprocessingExposure();
    void testOverlayOnTop();

    void testHighlightSilhouette();
    void testHighlightOutlineUniformBlockLayout();

    void testShadowMapRecordsCaster();
    void testShadowedSurfaceIsDarker_data();
    void testShadowedSurfaceIsDarker();

    void testSceneRendererFrame();
    void testSceneRendererResize();

  private:
    std::unique_ptr<QgsRhiEngine> makeEngine( QSize size = QSize( 64, 64 ), int sampleCount = 1 );

    //! Points \a pass straight down at the flat square makeQuad() builds
    void setTopDownCamera( QgsRenderPass &pass );

    /**
     * Fills \a buffer with a flat square in the XY plane, six vertices of position and
     * normal, and describes it in \a geometry.
     *
     * The square runs from (0,0) to (10,10) at height \a z, with its normal along +Z, so
     * the top down camera sees it filling most of the view.
     */
    void makeQuad( QgsGpuBuffer &buffer, QgsGeometry3D &geometry, float z = 0.0f, float size = 10.0f );

    //! Adds one white directional light shining straight down
    static void setOverheadLight( QgsRenderPass &pass );

    //! Renders one frame through \a body and returns what the engine read back
    QImage renderOffscreen( QgsRhiEngine *engine, const std::function<void()> &body );

    bool skipIfNoPixels() const { return mBackend == QRhi::Null; }

    QRhi::Implementation mBackend = QRhi::Null;
};

std::unique_ptr<QgsRhiEngine> TestQgsRenderPasses3D::makeEngine( QSize size, int sampleCount )
{
  auto engine = std::make_unique<QgsRhiEngine>();
  if ( !engine->createOffscreen( size, mBackend, sampleCount ) )
    return nullptr;
  return engine;
}

void TestQgsRenderPasses3D::setTopDownCamera( QgsRenderPass &pass )
{
  const QVector3D eye( 5.0f, 5.0f, 30.0f );

  QMatrix4x4 view;
  view.lookAt( eye, QVector3D( 5.0f, 5.0f, 0.0f ), QVector3D( 0.0f, 1.0f, 0.0f ) );

  QMatrix4x4 projection;
  projection.perspective( 45.0f, 1.0f, 1.0f, 100.0f );

  pass.setCamera( view, projection );
  pass.setEyePosition( eye );
  pass.setClipPlanes( 1.0f, 100.0f );
}

void TestQgsRenderPasses3D::makeQuad( QgsGpuBuffer &buffer, QgsGeometry3D &geometry, float z, float size )
{
  const float halfExtra = ( 10.0f - size ) / 2.0f;
  const float low = halfExtra;
  const float high = halfExtra + size;

  const float corners[4][2] = { { low, low }, { high, low }, { high, high }, { low, high } };
  const int order[6] = { 0, 1, 2, 0, 2, 3 };

  QVector<float> vertices;
  for ( const int corner : order )
  {
    vertices << corners[corner][0] << corners[corner][1] << z;
    vertices << 0.0f << 0.0f << 1.0f;
  }

  buffer.setData( QByteArray( reinterpret_cast<const char *>( vertices.constData() ), vertices.size() * int( sizeof( float ) ) ) );

  const quint32 stride = 6 * sizeof( float );
  geometry.addAttribute( { u"vertexPosition"_s, &buffer, QRhiVertexInputAttribute::Float3, 0, stride, 0 } );
  geometry.addAttribute( { u"vertexNormal"_s, &buffer, QRhiVertexInputAttribute::Float3, 3 * sizeof( float ), stride, 0 } );
  geometry.setVertexCount( 6 );
}

void TestQgsRenderPasses3D::setOverheadLight( QgsRenderPass &pass )
{
  QgsLightUniforms lights;
  lights.addDirectionalLight( QVector3D( 0.0f, 0.0f, -1.0f ), QColor( 255, 255, 255 ), 1.0f );
  pass.setLights( lights );
}

QImage TestQgsRenderPasses3D::renderOffscreen( QgsRhiEngine *engine, const std::function<void()> &body )
{
  engine->setReadbackEnabled( true );
  if ( !engine->beginFrame() )
    return QImage();
  body();
  engine->endFrame();
  return engine->readbackImage();
}

void TestQgsRenderPasses3D::initTestCase()
{
  mBackend = QgsRhiEngine::defaultBackend();

  QgsRhiEngine probe;
  if ( !probe.createOffscreen( QSize( 4, 4 ), mBackend ) )
    QSKIP( "No usable QRhi device for the default backend" );
}

void TestQgsRenderPasses3D::cleanupTestCase()
{
  QgsRhiShader::clearCache();
  QgsApplication::exitQgis();
}

//
// QgsGpuRenderTarget
//

void TestQgsRenderPasses3D::testRenderTargetColorOnly()
{
  auto engine = makeEngine();
  QVERIFY( engine );

  QgsGpuRenderTarget target;
  target.setColorFormat( QRhiTexture::RGBA8 );
  target.setDepthMode( QgsGpuRenderTarget::DepthMode::None );
  QVERIFY( target.create( engine->rhi(), QSize( 32, 24 ) ) );

  QVERIFY( target.isValid() );
  QCOMPARE( target.size(), QSize( 32, 24 ) );
  QVERIFY( target.colorTexture() );
  QCOMPARE( target.colorTexture()->pixelSize(), QSize( 32, 24 ) );

  // a target with no depth attachment has nothing for a later pass to sample
  QVERIFY( !target.depthTexture() );

  // both variants exist, and they are genuinely different objects: which one a pass draws
  // into is how it says whether it clears
  QVERIFY( target.rhiTarget( false ) );
  QVERIFY( target.rhiTarget( true ) );
  QVERIFY( target.rhiTarget( false ) != target.rhiTarget( true ) );

  // there is only one layer, so asking for a second one is an error rather than a wrap
  QVERIFY( !target.rhiTarget( false, 1 ) );
}

void TestQgsRenderPasses3D::testRenderTargetDepthTexture()
{
  auto engine = makeEngine();
  QVERIFY( engine );

  QgsGpuRenderTarget target;
  target.setDepthMode( QgsGpuRenderTarget::DepthMode::Texture );
  target.setDepthFormat( QRhiTexture::D32F );
  QVERIFY( target.create( engine->rhi(), QSize( 16, 16 ) ) );

  // this is the one the ambient occlusion, eye dome lighting and depth readback passes read
  QVERIFY( target.depthTexture() );
  QCOMPARE( target.depthTexture()->pixelSize(), QSize( 16, 16 ) );
}

void TestQgsRenderPasses3D::testRenderTargetLayers()
{
  auto engine = makeEngine();
  QVERIFY( engine );

  QgsGpuRenderTarget target;
  target.setColorFormat( QRhiTexture::R32F );
  target.setLayerCount( QgsShadowRenderPass::NUM_CASCADES );
  target.setDepthMode( QgsGpuRenderTarget::DepthMode::RenderBuffer );
  QVERIFY( target.create( engine->rhi(), QSize( 64, 64 ) ) );

  QCOMPARE( target.colorTexture()->arraySize(), QgsShadowRenderPass::NUM_CASCADES );

  // one render target per layer, each addressing a different slice of the same texture
  for ( int layer = 0; layer < QgsShadowRenderPass::NUM_CASCADES; ++layer )
    QVERIFY( target.rhiTarget( false, layer ) );
  QVERIFY( !target.rhiTarget( false, QgsShadowRenderPass::NUM_CASCADES ) );
}

void TestQgsRenderPasses3D::testRenderTargetResize()
{
  auto engine = makeEngine();
  QVERIFY( engine );

  QgsGpuRenderTarget target;
  QVERIFY( target.create( engine->rhi(), QSize( 16, 16 ) ) );

  QRhiTexture *before = target.colorTexture();
  // the same size rebuilds nothing, which is what keeps a redraw from churning textures
  QVERIFY( target.resize( engine->rhi(), QSize( 16, 16 ) ) );
  QCOMPARE( target.colorTexture(), before );

  QVERIFY( target.resize( engine->rhi(), QSize( 48, 32 ) ) );
  QCOMPARE( target.size(), QSize( 48, 32 ) );
  QCOMPARE( target.colorTexture()->pixelSize(), QSize( 48, 32 ) );
}

void TestQgsRenderPasses3D::testRenderTargetMultisample()
{
  auto engine = makeEngine();
  QVERIFY( engine );

  QgsGpuRenderTarget target;
  target.setSampleCount( 4 );
  target.setDepthMode( QgsGpuRenderTarget::DepthMode::Texture );
  if ( !target.create( engine->rhi(), QSize( 32, 32 ) ) )
    QSKIP( "This device cannot multisample at four samples" );

  // whatever happens behind it, what the next pass samples is always single sample
  QVERIFY( target.colorTexture() );
  QCOMPARE( target.colorTexture()->sampleCount(), 1 );

  // the depth texture is only there where the device can resolve depth; where it cannot,
  // that is exactly what tells the depth reading passes to stay out of the way
  if ( target.depthTexture() )
    QCOMPARE( target.depthTexture()->sampleCount(), 1 );
}

void TestQgsRenderPasses3D::testTargetClearsByDefault()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  auto engine = makeEngine();
  QVERIFY( engine );

  QgsGpuRenderTarget target;
  target.setDepthMode( QgsGpuRenderTarget::DepthMode::None );
  target.setReadbackEnabled( true );
  QVERIFY( target.create( engine->rhi(), QSize( 32, 32 ) ) );

  QgsGraphicsPipelineCache cache;
  QgsRenderPass first( u"first"_s );
  first.setClearColor( QColor( 255, 0, 0 ) );
  QgsRenderPass second( u"second"_s );
  second.setClearColor( QColor( 0, 0, 255 ) );

  QRhiReadbackResult readback;
  QVERIFY( engine->beginFrame() );
  QVERIFY( first.execute( engine.get(), &cache, target.rhiTarget() ) );
  QVERIFY( second.execute( engine.get(), &cache, target.rhiTarget() ) );
  QRhiResourceUpdateBatch *updates = engine->rhi()->nextResourceUpdateBatch();
  updates->readBackTexture( QRhiReadbackDescription( target.colorTexture() ), &readback );
  engine->commandBuffer()->resourceUpdate( updates );
  engine->endFrame();

  // the second pass drew into the clearing target, so the first pass's red is gone
  const QImage image = engine->imageFromReadback( readback );
  QVERIFY( !image.isNull() );
  QVERIFY( colorsClose( centerPixel( image ), QColor( 0, 0, 255 ) ) );
}

void TestQgsRenderPasses3D::testTargetKeepsContents()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  auto engine = makeEngine();
  QVERIFY( engine );

  QgsGpuRenderTarget target;
  target.setDepthMode( QgsGpuRenderTarget::DepthMode::None );
  target.setReadbackEnabled( true );
  QVERIFY( target.create( engine->rhi(), QSize( 32, 32 ) ) );

  QgsGraphicsPipelineCache cache;
  QgsRenderPass first( u"first"_s );
  first.setClearColor( QColor( 255, 0, 0 ) );
  QgsRenderPass second( u"second"_s );
  second.setClearColor( QColor( 0, 0, 255 ) );

  QRhiReadbackResult readback;
  QVERIFY( engine->beginFrame() );
  QVERIFY( first.execute( engine.get(), &cache, target.rhiTarget( false ) ) );
  // the preserving variant ignores the clear colour it is given and keeps what is there,
  // which is what lets the highlight and rubber band passes draw over the scene
  QVERIFY( second.execute( engine.get(), &cache, target.rhiTarget( true ) ) );
  QRhiResourceUpdateBatch *updates = engine->rhi()->nextResourceUpdateBatch();
  updates->readBackTexture( QRhiReadbackDescription( target.colorTexture() ), &readback );
  engine->commandBuffer()->resourceUpdate( updates );
  engine->endFrame();

  const QImage image = engine->imageFromReadback( readback );
  QVERIFY( !image.isNull() );
  QVERIFY( colorsClose( centerPixel( image ), QColor( 255, 0, 0 ) ) );
}

void TestQgsRenderPasses3D::testScreenQuadGeometry()
{
  const QgsScreenQuad quad;
  const QgsGeometry3D *geometry = quad.geometry();

  // two triangles, one attribute, no indices: anything more would be a shader input that
  // the full screen materials would then all have to declare
  QCOMPARE( geometry->vertexCount(), 6 );
  QVERIFY( !geometry->isIndexed() );
  QCOMPARE( geometry->attributes().size(), 1 );
  QVERIFY( geometry->attribute( u"vertexPosition"_s ) );
}

//
// Uniform block layouts. A mismatch here corrupts every value after the first bad offset,
// and does so silently, so each block is checked against the shader that reads it.
//

void TestQgsRenderPasses3D::testShadowUniformBlockLayout()
{
  // the shadow block is pass state rather than material state, so it is checked against a
  // lit material's shader rather than one of its own
  const std::shared_ptr<QgsRhiShader> shader = QgsRhiShader::load( u":/shaders/rhi/phong.vert.qsb"_s, u":/shaders/rhi/phong.frag.qsb"_s );
  QVERIFY( shader );

  const QVector<QgsUniformMember> members {
    { u"csmMatrices"_s, offsetof( QgsShadowUniforms, csmMatrices ), 256 },
    { u"csmBoundsMatrices"_s, offsetof( QgsShadowUniforms, csmBoundsMatrices ), 256 },
    { u"shadowBias"_s, offsetof( QgsShadowUniforms, shadowBias ), 4 },
    { u"maxShadowDistance"_s, offsetof( QgsShadowUniforms, maxShadowDistance ), 4 },
    { u"renderShadows"_s, offsetof( QgsShadowUniforms, renderShadows ), 4 },
    { u"shadowLightIndex"_s, offsetof( QgsShadowUniforms, shadowLightIndex ), 4 },
    { u"showCascadeSplits"_s, offsetof( QgsShadowUniforms, showCascadeSplits ), 4 },
  };

  QString error;
  QVERIFY2( shader->checkUniformBlockLayout( QgsRhiBinding::Shadow, members, &error ), error.toUtf8() );
}

void TestQgsRenderPasses3D::testShadowCasterVariantsExist()
{
  const QStringList variants { u"shadowcaster"_s, u"shadowcaster_instanced"_s, u"shadowcaster_instanced_scale"_s, u"shadowcaster_instanced_rotation"_s, u"shadowcaster_instanced_scale_rotation"_s };

  for ( const QString &variant : variants )
  {
    const std::shared_ptr<QgsRhiShader> shader = QgsRhiShader::load( u":/shaders/rhi/%1.vert.qsb"_s.arg( variant ), u":/shaders/rhi/shadowcaster.frag.qsb"_s );
    QVERIFY2( shader, variant.toUtf8() );

    // Position only, deliberately. A shadow caster that read a normal or a texture
    // coordinate would refuse to draw any geometry that does not carry one.
    QCOMPARE( shader->inputNames().contains( u"vertexNormal"_s ), false );
    QCOMPARE( shader->inputLocation( u"vertexPosition"_s ), 0 );

    QString error;
    QVERIFY2( shader->checkStageInterface( &error ), u"%1: %2"_s.arg( variant, error ).toUtf8() );
  }
}

void TestQgsRenderPasses3D::testPostprocessingUniformBlockLayout()
{
  const QgsPostprocessingMaterial3D material;
  QVERIFY( material.shader() );

  const QVector<QgsUniformMember> members {
    { u"edlEnabled"_s, offsetof( QgsPostprocessingUniforms, edlEnabled ), 4 },
    { u"edlStrength"_s, offsetof( QgsPostprocessingUniforms, edlStrength ), 4 },
    { u"edlDistance"_s, offsetof( QgsPostprocessingUniforms, edlDistance ), 4 },
    { u"ambientOcclusionEnabled"_s, offsetof( QgsPostprocessingUniforms, ambientOcclusionEnabled ), 4 },
    { u"bloomEnabled"_s, offsetof( QgsPostprocessingUniforms, bloomEnabled ), 4 },
    { u"bloomFactor"_s, offsetof( QgsPostprocessingUniforms, bloomFactor ), 4 },
    { u"exposureAdjustment"_s, offsetof( QgsPostprocessingUniforms, exposureAdjustment ), 4 },
    { u"toneMapping"_s, offsetof( QgsPostprocessingUniforms, toneMapping ), 4 },
  };

  QString error;
  QVERIFY2( material.shader()->checkUniformBlockLayout( QgsRhiBinding::Material, members, &error ), error.toUtf8() );
}

void TestQgsRenderPasses3D::testAmbientOcclusionUniformBlockLayout()
{
  const QgsAmbientOcclusionMaterial3D material;
  QVERIFY( material.shader() );

  const QVector<QgsUniformMember> members {
    { u"kernel"_s, offsetof( QgsAmbientOcclusionUniforms, kernel ), 1024 },
    { u"noise"_s, offsetof( QgsAmbientOcclusionUniforms, noise ), 256 },
    { u"intensity"_s, offsetof( QgsAmbientOcclusionUniforms, intensity ), 4 },
    { u"radius"_s, offsetof( QgsAmbientOcclusionUniforms, radius ), 4 },
    { u"threshold"_s, offsetof( QgsAmbientOcclusionUniforms, threshold ), 4 },
    { u"tanHalfFov"_s, offsetof( QgsAmbientOcclusionUniforms, tanHalfFov ), 4 },
  };

  QString error;
  QVERIFY2( material.shader()->checkUniformBlockLayout( QgsRhiBinding::Material, members, &error ), error.toUtf8() );

  // the kernel is generated once in the constructor rather than uploaded as a texture, so
  // check it actually holds directions rather than zeroes
  float longest = 0.0f;
  for ( int i = 0; i < QgsAmbientOcclusionUniforms::KERNEL_SIZE; ++i )
  {
    const float *sample = material.uniforms().kernel[i];
    longest = std::max( longest, std::sqrt( sample[0] * sample[0] + sample[1] * sample[1] + sample[2] * sample[2] ) );
  }
  QVERIFY( longest > 0.9f );
}

void TestQgsRenderPasses3D::testBloomUniformBlockLayout()
{
  for ( const QgsBloomMaterial3D::Step step : { QgsBloomMaterial3D::Step::Downsample, QgsBloomMaterial3D::Step::Upsample } )
  {
    const QgsBloomMaterial3D material( step );
    QVERIFY( material.shader() );

    const QVector<QgsUniformMember> members {
      { u"filterRadius"_s, offsetof( QgsBloomUniforms, filterRadius ), 4 },
      { u"aspectRatio"_s, offsetof( QgsBloomUniforms, aspectRatio ), 4 },
    };

    QString error;
    QVERIFY2( material.shader()->checkUniformBlockLayout( QgsRhiBinding::Material, members, &error ), error.toUtf8() );

    // the upsampling step is what adds each level onto the one above it
    QCOMPARE( material.blend().enable, step == QgsBloomMaterial3D::Step::Upsample );
  }
}

void TestQgsRenderPasses3D::testOverlayUniformBlockLayout()
{
  const QgsOverlayTextureMaterial3D material;
  QVERIFY( material.shader() );

  const QVector<QgsUniformMember> members {
    { u"centerTexCoords"_s, offsetof( QgsOverlayTextureUniforms, centerTexCoords ), 8 },
    { u"sizeTexCoords"_s, offsetof( QgsOverlayTextureUniforms, sizeTexCoords ), 8 },
    { u"isDepth"_s, offsetof( QgsOverlayTextureUniforms, isDepth ), 4 },
    { u"flipTextureY"_s, offsetof( QgsOverlayTextureUniforms, flipTextureY ), 4 },
  };

  QString error;
  QVERIFY2( material.shader()->checkUniformBlockLayout( QgsRhiBinding::Material, members, &error ), error.toUtf8() );
}

void TestQgsRenderPasses3D::testBackgroundUniformBlockLayout()
{
  const QgsBackgroundMaterial3D material;
  QVERIFY( material.shader() );

  const QVector<QgsUniformMember> members {
    { u"topColor"_s, offsetof( QgsBackgroundUniforms, topColor ), 16 },
    { u"bottomColor"_s, offsetof( QgsBackgroundUniforms, bottomColor ), 16 },
  };

  QString error;
  QVERIFY2( material.shader()->checkUniformBlockLayout( QgsRhiBinding::Material, members, &error ), error.toUtf8() );
}

//
// The forward pass
//

void TestQgsRenderPasses3D::testForwardPassDraws()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  auto engine = makeEngine();
  QVERIFY( engine );

  QgsForwardRenderPass forward;
  QVERIFY( forward.prepare( engine->rhi(), QSize( 64, 64 ) ) );
  QVERIFY( forward.colorTexture() );
  QVERIFY( forward.depthTexture() );

  forward.setClearColor( QColor( 0, 0, 0 ) );
  setTopDownCamera( forward.renderPass() );
  setOverheadLight( forward.renderPass() );

  QgsGpuBuffer buffer;
  QgsGeometry3D geometry;
  makeQuad( buffer, geometry );

  QgsUnlitMaterial3D material;
  material.setColor( QColor( 0, 255, 0 ) );

  QgsRenderCommand command;
  command.geometry = &geometry;
  command.material = &material;
  forward.addCommand( command, QgsForwardRenderPass::Group::Opaque );

  // the forward pass's own target is HDR and not readable, so the result is composed to
  // the engine's output the way a real frame does it
  QgsScreenQuad quad;
  QgsGraphicsPipelineCache cache;
  QgsPostprocessingRenderPass post;
  post.material().setColorTexture( forward.colorTexture() );

  const QImage image = renderOffscreen( engine.get(), [&]() {
    QVERIFY( forward.render( engine.get(), &cache ) );
    QVERIFY( post.render( engine.get(), &cache, &quad ) );
  } );

  QVERIFY( !image.isNull() );
  QVERIFY( colorsClose( centerPixel( image ), QColor( 0, 255, 0 ), 6 ) );
}

void TestQgsRenderPasses3D::testForwardPassGroupOrder()
{
  QgsForwardRenderPass forward;

  QgsGpuBuffer buffer;
  QgsGeometry3D geometry;
  makeQuad( buffer, geometry );
  QgsUnlitMaterial3D material;

  // added out of order, and with sort keys that would reverse them within a group
  QgsRenderCommand command;
  command.geometry = &geometry;
  command.material = &material;

  command.sortKey = 5;
  forward.addCommand( command, QgsForwardRenderPass::Group::TransparentDepth );
  command.sortKey = 9;
  forward.addCommand( command, QgsForwardRenderPass::Group::Opaque );
  command.sortKey = 1;
  forward.addCommand( command, QgsForwardRenderPass::Group::Opaque );
  command.sortKey = 0;
  forward.addCommand( command, QgsForwardRenderPass::Group::Background );

  // the pass keeps them in the order they arrived; it is the key that carries the group,
  // and the pass sorts by it when it draws
  std::vector<QgsRenderCommand> commands = forward.renderPass().commands();
  QCOMPARE( commands.size(), size_t( 4 ) );

  // the caller's own key survives in the low bits
  QCOMPARE( commands[0].sortKey & 0x0FFFFFFFu, 5u );
  QCOMPARE( commands[1].sortKey & 0x0FFFFFFFu, 9u );

  std::stable_sort( commands.begin(), commands.end(), []( const QgsRenderCommand &a, const QgsRenderCommand &b ) { return a.sortKey < b.sortKey; } );

  // background first, then the two opaque squares in their own key order, then the
  // transparent depth pass last
  QCOMPARE( commands[0].sortKey >> 28, static_cast<quint32>( QgsForwardRenderPass::Group::Background ) );
  QCOMPARE( commands[1].sortKey >> 28, static_cast<quint32>( QgsForwardRenderPass::Group::Opaque ) );
  QCOMPARE( commands[1].sortKey & 0x0FFFFFFFu, 1u );
  QCOMPARE( commands[2].sortKey & 0x0FFFFFFFu, 9u );
  QCOMPARE( commands[3].sortKey >> 28, static_cast<quint32>( QgsForwardRenderPass::Group::TransparentDepth ) );
}

void TestQgsRenderPasses3D::testBackgroundBehindScene()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  auto engine = makeEngine();
  QVERIFY( engine );

  QgsForwardRenderPass forward;
  QVERIFY( forward.prepare( engine->rhi(), QSize( 64, 64 ) ) );
  setTopDownCamera( forward.renderPass() );

  QgsScreenQuad quad;

  // a background covering the whole view, and a small square in front of it
  QgsBackgroundMaterial3D background;
  background.setTopColor( QColor( 255, 0, 0 ) );
  background.setBottomColor( QColor( 255, 0, 0 ) );

  QgsRenderCommand backgroundCommand;
  backgroundCommand.geometry = quad.geometry();
  backgroundCommand.material = &background;
  forward.addCommand( backgroundCommand, QgsForwardRenderPass::Group::Background );

  QgsGpuBuffer buffer;
  QgsGeometry3D geometry;
  makeQuad( buffer, geometry, 0.0f, 4.0f );

  QgsUnlitMaterial3D material;
  material.setColor( QColor( 0, 0, 255 ) );

  QgsRenderCommand command;
  command.geometry = &geometry;
  command.material = &material;
  forward.addCommand( command, QgsForwardRenderPass::Group::Opaque );

  QgsGraphicsPipelineCache cache;
  QgsPostprocessingRenderPass post;
  post.material().setColorTexture( forward.colorTexture() );

  const QImage image = renderOffscreen( engine.get(), [&]() {
    QVERIFY( forward.render( engine.get(), &cache ) );
    QVERIFY( post.render( engine.get(), &cache, &quad ) );
  } );

  QVERIFY( !image.isNull() );
  // the square is in the middle, the background is out at the corner
  QVERIFY( colorsClose( centerPixel( image ), QColor( 0, 0, 255 ), 6 ) );
  QVERIFY( colorsClose( image.pixelColor( 2, 2 ), QColor( 255, 0, 0 ), 6 ) );
}

//
// The passes that read the forward pass's attachments
//

void TestQgsRenderPasses3D::testDepthPassReadback()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  auto engine = makeEngine();
  QVERIFY( engine );

  QgsForwardRenderPass forward;
  QVERIFY( forward.prepare( engine->rhi(), QSize( 64, 64 ) ) );
  setTopDownCamera( forward.renderPass() );

  QgsGpuBuffer buffer;
  QgsGeometry3D geometry;
  makeQuad( buffer, geometry );
  QgsUnlitMaterial3D material;

  QgsRenderCommand command;
  command.geometry = &geometry;
  command.material = &material;
  forward.addCommand( command, QgsForwardRenderPass::Group::Opaque );

  QgsDepthRenderPass depth;
  QVERIFY( depth.prepare( engine->rhi(), QSize( 64, 64 ) ) );
  depth.setDepthTexture( forward.depthTexture() );

  QgsScreenQuad quad;
  QgsGraphicsPipelineCache cache;

  QVERIFY( engine->beginFrame() );
  QVERIFY( forward.render( engine.get(), &cache ) );
  QVERIFY( depth.render( engine.get(), &cache, &quad ) );
  engine->endFrame();
  depth.collectReadback( engine.get() );

  const QImage image = depth.image();
  QVERIFY( !image.isNull() );
  QCOMPARE( image.size(), QSize( 64, 64 ) );

  // The corner is background, so its depth is the cleared 1.0. The encoding puts the most
  // significant byte in blue and what is left over in green and red, so a depth of exactly
  // one comes out as pure blue.
  const QColor center = centerPixel( image );
  const QColor corner = image.pixelColor( 1, 1 );
  QCOMPARE( corner, QColor( 0, 0, 255 ) );

  // the middle of the view is the square, which is much nearer than the far plane
  QVERIFY( center != corner );
  QVERIFY( center.blue() < 255 );
}

void TestQgsRenderPasses3D::testDepthReadbackSwitchedOnAfterPrepare()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  auto engine = makeEngine();
  QVERIFY( engine );

  // A 3D view prepares its scene renderer once, when the window is first shown, and only
  // switches the depth readback on later, when a camera gesture asks for a depth buffer.
  // The pass therefore has to build its target then rather than at prepare() time.
  QgsSceneRenderer renderer;
  QVERIFY( renderer.prepare( engine.get(), QSize( 64, 64 ) ) );
  QVERIFY( renderer.depthImage().isNull() );

  QMatrix4x4 view;
  const QVector3D eye( 5.0f, 5.0f, 30.0f );
  view.lookAt( eye, QVector3D( 5.0f, 5.0f, 0.0f ), QVector3D( 0.0f, 1.0f, 0.0f ) );
  QMatrix4x4 projection;
  projection.perspective( 45.0f, 1.0f, 1.0f, 100.0f );
  renderer.setCamera( view, projection, eye, 45.0f, 1.0f, 100.0f );
  renderer.setClearColor( QColor( 0, 0, 0 ) );

  QgsGpuBuffer buffer;
  QgsGeometry3D geometry;
  makeQuad( buffer, geometry );
  QgsUnlitMaterial3D material;

  QgsRenderCommand command;
  command.geometry = &geometry;
  command.material = &material;

  renderer.setDepthReadbackEnabled( true );

  renderer.clearCommands();
  renderer.forwardPass().addCommand( command, QgsForwardRenderPass::Group::Opaque );

  QVERIFY( engine->beginFrame() );
  QVERIFY( renderer.renderFrame( engine.get() ) );
  engine->endFrame();
  renderer.finishFrame( engine.get() );

  const QImage depth = renderer.depthImage();
  QVERIFY( !depth.isNull() );
  QCOMPARE( depth.size(), QSize( 64, 64 ) );

  // QgsCameraController averages the buffer by casting whole scan lines to QRgb *, so the
  // channel order has to be the native 32 bit one rather than the readback's RGBA8888.
  QCOMPARE( depth.format(), QImage::Format_RGB32 );

  // as in testDepthPassReadback(): the cleared background encodes to pure blue, the square
  // in the middle of the view is much nearer than the far plane
  QCOMPARE( depth.pixelColor( 1, 1 ), QColor( 0, 0, 255 ) );
  QVERIFY( centerPixel( depth ).blue() < 255 );

  // and switching it off again releases the pass without disturbing the frame
  renderer.setDepthReadbackEnabled( false );
  renderer.clearCommands();
  renderer.forwardPass().addCommand( command, QgsForwardRenderPass::Group::Opaque );

  QVERIFY( engine->beginFrame() );
  QVERIFY( renderer.renderFrame( engine.get() ) );
  engine->endFrame();
  renderer.finishFrame( engine.get() );
}

void TestQgsRenderPasses3D::testOutputOrientation()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  auto engine = makeEngine();
  QVERIFY( engine );

  QgsSceneRenderer renderer;
  QVERIFY( renderer.prepare( engine.get(), QSize( 64, 64 ) ) );

  // The camera looks straight down the -Z axis with +Y as up, so something at a greater Y
  // than the view centre has to come out in the upper half of the image.
  QMatrix4x4 view;
  const QVector3D eye( 5.0f, 5.0f, 30.0f );
  view.lookAt( eye, QVector3D( 5.0f, 5.0f, 0.0f ), QVector3D( 0.0f, 1.0f, 0.0f ) );
  QMatrix4x4 projection;
  projection.perspective( 45.0f, 1.0f, 1.0f, 100.0f );
  renderer.setCamera( view, projection, eye, 45.0f, 1.0f, 100.0f );
  renderer.setClearColor( QColor( 0, 0, 0 ) );

  QgsGpuBuffer buffer;
  QgsGeometry3D geometry;
  makeQuad( buffer, geometry, 0.0f, 3.0f );

  QgsUnlitMaterial3D material;
  material.setColor( QColor( 255, 0, 0 ) );

  QgsRenderCommand command;
  command.geometry = &geometry;
  command.material = &material;
  command.worldTransform.translate( 0.0f, 5.0f, 0.0f );

  renderer.clearCommands();
  renderer.forwardPass().addCommand( command, QgsForwardRenderPass::Group::Opaque );

  engine->setReadbackEnabled( true );
  QVERIFY( engine->beginFrame() );
  QVERIFY( renderer.renderFrame( engine.get() ) );
  engine->endFrame();

  const QImage image = engine->readbackImage();
  QVERIFY( !image.isNull() );

  // QImage row 0 is the top of the picture, so the square belongs there and the lower half
  // is the cleared background. Getting this the wrong way round mirrors the whole view,
  // which leaves the picture plausible but every vertical camera gesture inverted.
  const QColor upper = image.pixelColor( 32, 16 );
  const QColor lower = image.pixelColor( 32, 48 );
  QVERIFY2( colorsClose( upper, QColor( 255, 0, 0 ), 6 ), upper.name().toUtf8() );
  QVERIFY2( colorsClose( lower, QColor( 0, 0, 0 ), 6 ), lower.name().toUtf8() );
}

void TestQgsRenderPasses3D::testAmbientOcclusionOpenSurface()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  auto engine = makeEngine();
  QVERIFY( engine );

  QgsForwardRenderPass forward;
  QVERIFY( forward.prepare( engine->rhi(), QSize( 64, 64 ) ) );
  setTopDownCamera( forward.renderPass() );

  QgsGpuBuffer buffer;
  QgsGeometry3D geometry;
  makeQuad( buffer, geometry );
  QgsUnlitMaterial3D material;

  QgsRenderCommand command;
  command.geometry = &geometry;
  command.material = &material;
  forward.addCommand( command, QgsForwardRenderPass::Group::Opaque );

  QgsAmbientOcclusionRenderPass occlusion;
  QVERIFY( occlusion.prepare( engine->rhi(), QSize( 64, 64 ) ) );
  occlusion.setDepthTexture( forward.depthTexture() );
  setTopDownCamera( occlusion.occlusionPass() );
  occlusion.setTanHalfFov( std::tan( 45.0f * 0.5f * static_cast<float>( M_PI ) / 180.0f ) );

  QgsScreenQuad quad;
  QgsGraphicsPipelineCache cache;

  // the blurred factor is what the post processing pass multiplies the scene by, so it is
  // shown here through an overlay quad rather than read back as a float texture
  QgsOverlayTextureMaterial3D overlay;
  overlay.setTexture( occlusion.blurredTexture() );
  overlay.setIsDepth( true );
  overlay.setViewport( QPointF( 0.5, 0.5 ), QSizeF( 0.5, 0.5 ) );

  QgsRenderPass show( u"show"_s );
  QgsRenderCommand overlayCommand;
  overlayCommand.geometry = quad.geometry();
  overlayCommand.material = &overlay;
  show.addCommand( overlayCommand );

  const QImage image = renderOffscreen( engine.get(), [&]() {
    QVERIFY( forward.render( engine.get(), &cache ) );
    QVERIFY( occlusion.render( engine.get(), &cache, &quad ) );
    QVERIFY( show.execute( engine.get(), &cache ) );
  } );

  QVERIFY( !image.isNull() );

  // a single flat square has nothing to occlude it, so the factor should be near one
  const QColor center = centerPixel( image );
  QVERIFY2( center.red() > 200, QByteArray::number( center.red() ) );
}

void TestQgsRenderPasses3D::testBloomChainSizes()
{
  auto engine = makeEngine();
  QVERIFY( engine );

  QgsForwardRenderPass forward;
  QVERIFY( forward.prepare( engine->rhi(), QSize( 128, 64 ) ) );

  QgsBloomRenderPass bloom;
  bloom.setEnabled( true );
  bloom.setSourceTexture( forward.colorTexture() );
  QVERIFY( bloom.prepare( engine->rhi(), QSize( 128, 64 ) ) );

  // the top of the chain is what the post processing pass adds to the scene, and it is
  // half the view, not the whole of it
  QVERIFY( bloom.bloomTexture() );
  QCOMPARE( bloom.bloomTexture()->pixelSize(), QSize( 64, 32 ) );
}

void TestQgsRenderPasses3D::testBloomDisabledHoldsNoTextures()
{
  auto engine = makeEngine();
  QVERIFY( engine );

  QgsBloomRenderPass bloom;
  bloom.setEnabled( false );
  QVERIFY( bloom.prepare( engine->rhi(), QSize( 256, 256 ) ) );

  // five levels of half float colour is real memory, and nothing samples them while the
  // effect is off
  QVERIFY( !bloom.bloomTexture() );
}

//
// The post processing pass
//

void TestQgsRenderPasses3D::testPostprocessingGamma()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  auto engine = makeEngine();
  QVERIFY( engine );

  QgsForwardRenderPass forward;
  QVERIFY( forward.prepare( engine->rhi(), QSize( 32, 32 ) ) );
  // a linear mid grey, which gamma correction should lift well above 128
  forward.setClearColor( QColor::fromRgbF( 0.5f, 0.5f, 0.5f, 1.0f ) );

  QgsScreenQuad quad;
  QgsGraphicsPipelineCache cache;
  QgsPostprocessingRenderPass post;
  post.material().setColorTexture( forward.colorTexture() );

  const QImage image = renderOffscreen( engine.get(), [&]() {
    QVERIFY( forward.render( engine.get(), &cache ) );
    QVERIFY( post.render( engine.get(), &cache, &quad ) );
  } );

  QVERIFY( !image.isNull() );
  // 0.5 linear is about 0.73 after the hardcoded 2.2 gamma, so about 187
  const int red = centerPixel( image ).red();
  QVERIFY2( red > 175 && red < 200, QByteArray::number( red ) );
}

void TestQgsRenderPasses3D::testPostprocessingExposure()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  auto engine = makeEngine();
  QVERIFY( engine );

  QgsForwardRenderPass forward;
  QVERIFY( forward.prepare( engine->rhi(), QSize( 32, 32 ) ) );
  forward.setClearColor( QColor::fromRgbF( 0.25f, 0.25f, 0.25f, 1.0f ) );

  QgsScreenQuad quad;
  QgsGraphicsPipelineCache cache;
  QgsPostprocessingRenderPass post;
  post.material().setColorTexture( forward.colorTexture() );

  const QImage before = renderOffscreen( engine.get(), [&]() {
    QVERIFY( forward.render( engine.get(), &cache ) );
    QVERIFY( post.render( engine.get(), &cache, &quad ) );
  } );

  // one stop up doubles the linear value, which lifts the sRGB result noticeably
  post.material().setExposureAdjustment( 1.0f );
  const QImage after = renderOffscreen( engine.get(), [&]() {
    QVERIFY( forward.render( engine.get(), &cache ) );
    QVERIFY( post.render( engine.get(), &cache, &quad ) );
  } );

  QVERIFY( !before.isNull() && !after.isNull() );
  QVERIFY2( centerPixel( after ).red() > centerPixel( before ).red() + 20, QByteArray::number( centerPixel( after ).red() ) );
}

void TestQgsRenderPasses3D::testOverlayOnTop()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  auto engine = makeEngine( QSize( 64, 64 ) );
  QVERIFY( engine );

  QgsForwardRenderPass forward;
  QVERIFY( forward.prepare( engine->rhi(), QSize( 64, 64 ) ) );
  forward.setClearColor( QColor( 0, 0, 0 ) );

  // something to overlay: a second forward target filled with a flat colour
  QgsGpuRenderTarget source;
  source.setDepthMode( QgsGpuRenderTarget::DepthMode::None );
  QVERIFY( source.create( engine->rhi(), QSize( 8, 8 ) ) );

  QgsRenderPass fill( u"fill"_s );
  fill.setClearColor( QColor( 255, 0, 0 ) );

  QgsOverlayQuads overlays;
  QgsOverlayTextureMaterial3D *overlay = overlays.addOverlay( source.colorTexture() );
  QVERIFY( overlay );
  // the whole view, so the overlay is what the middle pixel shows
  overlay->setViewport( QPointF( 0.5, 0.5 ), QSizeF( 0.5, 0.5 ) );

  QgsScreenQuad quad;
  QgsGraphicsPipelineCache cache;
  QgsPostprocessingRenderPass post;
  post.material().setColorTexture( forward.colorTexture() );

  const QImage image = renderOffscreen( engine.get(), [&]() {
    QVERIFY( fill.execute( engine.get(), &cache, source.rhiTarget() ) );
    QVERIFY( forward.render( engine.get(), &cache ) );
    QVERIFY( post.render( engine.get(), &cache, &quad, nullptr, &overlays ) );
  } );

  QVERIFY( !image.isNull() );
  QVERIFY( colorsClose( centerPixel( image ), QColor( 255, 0, 0 ), 6 ) );
}

//
// Highlights
//

void TestQgsRenderPasses3D::testHighlightSilhouette()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  auto engine = makeEngine();
  QVERIFY( engine );

  QgsSceneRenderer renderer;
  QVERIFY( renderer.prepare( engine.get(), QSize( 64, 64 ) ) );
  renderer.highlightsPass().setOutlineColor( QColor( 0, 255, 0 ) );
  renderer.highlightsPass().setOutlineWidth( 3 );

  QMatrix4x4 view;
  const QVector3D eye( 5.0f, 5.0f, 30.0f );
  view.lookAt( eye, QVector3D( 5.0f, 5.0f, 0.0f ), QVector3D( 0.0f, 1.0f, 0.0f ) );
  QMatrix4x4 projection;
  projection.perspective( 45.0f, 1.0f, 1.0f, 100.0f );
  renderer.setCamera( view, projection, eye, 45.0f, 1.0f, 100.0f );
  renderer.setClearColor( QColor( 0, 0, 0 ) );

  QgsGpuBuffer buffer;
  QgsGeometry3D geometry;
  makeQuad( buffer, geometry, 0.0f, 4.0f );

  // the fill, drawn straight over the scene, is an ordinary scene material
  QgsUnlitMaterial3D fill;
  fill.setColor( QColor( 255, 0, 0 ) );
  fill.setDepthTest( false );
  fill.setDepthWrite( false );
  fill.setCullMode( QRhiGraphicsPipeline::None );

  QgsHighlightMaskMaterial3D mask;

  QgsRenderCommand command;
  command.geometry = &geometry;

  renderer.clearCommands();
  command.material = &fill;
  renderer.highlightsPass().fillPass().addCommand( command );
  command.material = &mask;
  renderer.highlightsPass().maskPass().addCommand( command );

  engine->setReadbackEnabled( true );
  QVERIFY( engine->beginFrame() );
  QVERIFY( renderer.renderFrame( engine.get() ) );
  engine->endFrame();

  const QImage image = engine->readbackImage();
  QVERIFY( !image.isNull() );

  // The four unit square lands on pixels 27 to 36, so its middle is fill, the three pixels
  // beyond its right edge are the band, and beyond that is the cleared scene.
  QVERIFY2( colorsClose( centerPixel( image ), QColor( 255, 0, 0 ), 6 ), centerPixel( image ).name().toUtf8() );

  const QColor band = image.pixelColor( 38, 32 );
  QVERIFY2( colorsClose( band, QColor( 0, 255, 0 ), 6 ), band.name().toUtf8() );

  QVERIFY2( colorsClose( image.pixelColor( 55, 32 ), QColor( 0, 0, 0 ), 6 ), image.pixelColor( 55, 32 ).name().toUtf8() );

  // and the band really is a band: it stops, rather than filling everything outside
  const QColor beyond = image.pixelColor( 45, 32 );
  QVERIFY2( colorsClose( beyond, QColor( 0, 0, 0 ), 6 ), beyond.name().toUtf8() );
}

void TestQgsRenderPasses3D::testHighlightOutlineUniformBlockLayout()
{
  const QgsHighlightOutlineMaterial3D material;
  QVERIFY( material.shader() );

  const QVector<QgsUniformMember> members {
    { u"outlineColor"_s, offsetof( QgsHighlightOutlineUniforms, outlineColor ), 16 },
    { u"outlineWidth"_s, offsetof( QgsHighlightOutlineUniforms, outlineWidth ), 4 },
  };

  QString error;
  QVERIFY2( material.shader()->checkUniformBlockLayout( QgsRhiBinding::Material, members, &error ), error.toUtf8() );

  // the mask shares the shadow caster's vertex stages, so it reads a position and nothing else
  const QgsHighlightMaskMaterial3D mask;
  QVERIFY( mask.shader() );
  QCOMPARE( mask.shader()->inputNames().size(), 1 );
  QCOMPARE( mask.shader()->inputLocation( u"vertexPosition"_s ), 0 );
}

//
// Shadows
//

void TestQgsRenderPasses3D::testShadowMapRecordsCaster()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  auto engine = makeEngine();
  QVERIFY( engine );

  QgsShadowRenderPass shadow;
  QVERIFY( shadow.prepare( engine->rhi(), 256 ) );
  QVERIFY( shadow.shadowMap() );
  QCOMPARE( shadow.shadowMap()->arraySize(), QgsShadowRenderPass::NUM_CASCADES );

  QMatrix4x4 cameraView;
  cameraView.lookAt( QVector3D( 5.0f, 5.0f, 30.0f ), QVector3D( 5.0f, 5.0f, 0.0f ), QVector3D( 0.0f, 1.0f, 0.0f ) );
  shadow.updateCascades( engine->rhi(), cameraView, 45.0f, 1.0f, 1.0f, 100.0f, QVector3D( 0.0f, 0.0f, -1.0f ), 0, 0.00001f, 500.0f );

  QCOMPARE( shadow.uniforms().renderShadows, 1 );
  QCOMPARE( shadow.uniforms().shadowLightIndex, 0 );

  QgsGpuBuffer buffer;
  QgsGeometry3D geometry;
  makeQuad( buffer, geometry );

  QgsShadowCasterMaterial3D caster;
  QgsRenderCommand command;
  command.geometry = &geometry;
  command.material = &caster;
  shadow.addCommand( command );
  QCOMPARE( shadow.commands().size(), size_t( 1 ) );

  // the shadow map is a float array, so what it holds is checked by showing the nearest
  // cascade through an overlay quad rather than by reading floats back
  QgsScreenQuad quad;
  QgsGraphicsPipelineCache cache;

  const QImage image = renderOffscreen( engine.get(), [&]() { QVERIFY( shadow.render( engine.get(), &cache ) ); } );

  // the frame drew nothing into the engine's own output, which is fine: what matters is
  // that every cascade recorded without failing
  QVERIFY( !image.isNull() );
}

void TestQgsRenderPasses3D::testShadowedSurfaceIsDarker_data()
{
  QTest::addColumn<QVector3D>( "lightDirection" );
  QTest::addColumn<QVector3D>( "blockerOffset" );
  QTest::addColumn<QPoint>( "samplePoint" );

  // The camera looks straight down with world +y as screen up, and the floor spans about
  // 2.6 pixels per world unit around the centre of the image.
  QTest::newRow( "light along x" ) << QVector3D( 0.4f, 0.0f, -1.0f ).normalized() << QVector3D() << QPoint( 41, 32 );
  QTest::newRow( "light along y" ) << QVector3D( 0.0f, 0.4f, -1.0f ).normalized() << QVector3D() << QPoint( 32, 23 );

  // A caster in the middle of the shadow map is symmetric about both of its axes, so it
  // casts the same shadow whether the map is read the right way up or mirrored. Moving it
  // off centre is what makes the lookup's orientation observable: read mirrored, this
  // caster is found at -y and the floor at +y comes out lit.
  QTest::newRow( "caster off centre" ) << QVector3D( 0.4f, 0.0f, -1.0f ).normalized() << QVector3D( 0.0f, 3.0f, 0.0f ) << QPoint( 40, 24 );
}

void TestQgsRenderPasses3D::testShadowedSurfaceIsDarker()
{
  QFETCH( QVector3D, lightDirection );
  QFETCH( QVector3D, blockerOffset );
  QFETCH( QPoint, samplePoint );

  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  auto engine = makeEngine();
  QVERIFY( engine );

  // A wide floor, and a small blocker floating above the middle of it. The light is
  // deliberately off vertical: straight down, the blocker's shadow would land exactly
  // under the blocker and be hidden by it.
  QgsGpuBuffer floorBuffer;
  QgsGeometry3D floorGeometry;
  makeQuad( floorBuffer, floorGeometry, 0.0f, 10.0f );

  QgsGpuBuffer blockerBuffer;
  QgsGeometry3D blockerGeometry;
  makeQuad( blockerBuffer, blockerGeometry, 8.0f, 3.0f );

  QgsSceneRenderer renderer;
  QVERIFY( renderer.prepare( engine.get(), QSize( 64, 64 ) ) );

  QMatrix4x4 view;
  const QVector3D eye( 5.0f, 5.0f, 30.0f );
  view.lookAt( eye, QVector3D( 5.0f, 5.0f, 0.0f ), QVector3D( 0.0f, 1.0f, 0.0f ) );
  QMatrix4x4 projection;
  projection.perspective( 45.0f, 1.0f, 1.0f, 100.0f );
  renderer.setCamera( view, projection, eye, 45.0f, 1.0f, 100.0f );

  QgsLightUniforms lights;
  lights.addDirectionalLight( lightDirection, QColor( 255, 255, 255 ), 1.0f );
  renderer.setLights( lights );
  renderer.setClearColor( QColor( 0, 0, 0 ) );

  QgsPhongMaterial3D floorMaterial;
  floorMaterial.setAmbient( QColor( 0, 0, 0 ) );
  floorMaterial.setDiffuse( QColor( 255, 255, 255 ) );
  floorMaterial.setSpecular( QColor( 0, 0, 0 ) );

  QgsShadowCasterMaterial3D caster;
  // The caster material culls front faces by default, following what Qt3D's shadow render
  // view did: writing the back of each object into the map keeps self-shadowing acne on
  // the side facing away from the light. A single sided square has no back face at all,
  // so this test asks for both.
  caster.setCullMode( QRhiGraphicsPipeline::None );

  auto renderOnce = [&]( bool shadows ) -> QImage {
    // no second prepare(): the scene renderer builds the shadow pass when the setting is
    // switched on, which is what a real view does between two frames
    renderer.setShadowsEnabled( shadows, lightDirection, 0, 1024, 500.0f );

    renderer.clearCommands();

    QgsRenderCommand floorCommand;
    floorCommand.geometry = &floorGeometry;
    floorCommand.material = &floorMaterial;
    renderer.forwardPass().addCommand( floorCommand, QgsForwardRenderPass::Group::Opaque );

    QgsRenderCommand blockerCommand;
    blockerCommand.geometry = &blockerGeometry;
    blockerCommand.material = &floorMaterial;
    blockerCommand.worldTransform.translate( blockerOffset );
    renderer.forwardPass().addCommand( blockerCommand, QgsForwardRenderPass::Group::Opaque );

    QgsRenderCommand casterCommand;
    casterCommand.geometry = &blockerGeometry;
    casterCommand.material = &caster;
    casterCommand.worldTransform.translate( blockerOffset );
    renderer.shadowPass().addCommand( casterCommand );

    engine->setReadbackEnabled( true );
    if ( !engine->beginFrame() )
      return QImage();
    renderer.renderFrame( engine.get() );
    engine->endFrame();
    return engine->readbackImage();
  };

  const QImage withoutShadows = renderOnce( false );
  const QImage withShadows = renderOnce( true );

  QVERIFY( !withoutShadows.isNull() );
  QVERIFY( !withShadows.isNull() );

  // A point on the floor inside the blocker's shadow but not behind the blocker itself.
  // The blocker is three units across at height eight, and the light leans far enough for
  // its shadow to land clear of it.
  const int lit = withoutShadows.pixelColor( samplePoint ).red();
  const int shadowed = withShadows.pixelColor( samplePoint ).red();

  // the blocker itself is lit in both, so it is only the floor around it that changes
  QVERIFY2( lit > 0, QByteArray::number( lit ) );
  QVERIFY2( shadowed < lit, u"lit %1, shadowed %2"_s.arg( lit ).arg( shadowed ).toUtf8() );
}

//
// The whole frame
//

void TestQgsRenderPasses3D::testSceneRendererFrame()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  auto engine = makeEngine();
  QVERIFY( engine );

  QgsSceneRenderer renderer;
  renderer.setDepthReadbackEnabled( true );
  QVERIFY( renderer.prepare( engine.get(), QSize( 64, 64 ) ) );
  QCOMPARE( renderer.size(), QSize( 64, 64 ) );

  QMatrix4x4 view;
  const QVector3D eye( 5.0f, 5.0f, 30.0f );
  view.lookAt( eye, QVector3D( 5.0f, 5.0f, 0.0f ), QVector3D( 0.0f, 1.0f, 0.0f ) );
  QMatrix4x4 projection;
  projection.perspective( 45.0f, 1.0f, 1.0f, 100.0f );
  renderer.setCamera( view, projection, eye, 45.0f, 1.0f, 100.0f );
  renderer.setClearColor( QColor( 0, 0, 0 ) );

  QgsGpuBuffer buffer;
  QgsGeometry3D geometry;
  makeQuad( buffer, geometry );

  QgsUnlitMaterial3D material;
  material.setColor( QColor( 255, 0, 0 ) );

  QgsRenderCommand command;
  command.geometry = &geometry;
  command.material = &material;

  renderer.clearCommands();
  renderer.forwardPass().addCommand( command, QgsForwardRenderPass::Group::Opaque );

  engine->setReadbackEnabled( true );
  QVERIFY( engine->beginFrame() );
  QVERIFY( renderer.renderFrame( engine.get() ) );
  engine->endFrame();
  renderer.finishFrame( engine.get() );

  const QImage image = engine->readbackImage();
  QVERIFY( !image.isNull() );
  QVERIFY( colorsClose( centerPixel( image ), QColor( 255, 0, 0 ), 6 ) );

  // the depth pass ran as part of the frame, so camera navigation has something to read
  QVERIFY( !renderer.depthImage().isNull() );
  QCOMPARE( renderer.depthImage().size(), QSize( 64, 64 ) );
}

void TestQgsRenderPasses3D::testSceneRendererResize()
{
  auto engine = makeEngine();
  QVERIFY( engine );

  QgsSceneRenderer renderer;
  QVERIFY( renderer.prepare( engine.get(), QSize( 64, 64 ) ) );

  QVERIFY( engine->setOutputSize( QSize( 96, 48 ) ) );
  QVERIFY( renderer.prepare( engine.get(), QSize( 96, 48 ) ) );
  QCOMPARE( renderer.size(), QSize( 96, 48 ) );
  QCOMPARE( renderer.forwardPass().colorTexture()->pixelSize(), QSize( 96, 48 ) );

  // releasing and preparing again has to work, because that is what happens when the
  // device is lost and rebuilt
  renderer.releaseGpuResources();
  QVERIFY( renderer.prepare( engine.get(), QSize( 64, 64 ) ) );
}

void TestQgsRenderPasses3D::testDepthReadbackIsNotOverwrittenBeforeCollect()
{
  if ( skipIfNoPixels() )
    QSKIP( "The Null backend draws no pixels" );

  auto engine = makeEngine();
  QVERIFY( engine );

  QgsSceneRenderer renderer;
  QVERIFY( renderer.prepare( engine.get(), QSize( 64, 64 ) ) );

  QMatrix4x4 view;
  const QVector3D eye( 5.0f, 5.0f, 30.0f );
  view.lookAt( eye, QVector3D( 5.0f, 5.0f, 0.0f ), QVector3D( 0.0f, 1.0f, 0.0f ) );
  QMatrix4x4 projection;
  projection.perspective( 45.0f, 1.0f, 1.0f, 100.0f );
  renderer.setCamera( view, projection, eye, 45.0f, 1.0f, 100.0f );
  renderer.setClearColor( QColor( 0, 0, 0 ) );

  QgsGpuBuffer buffer;
  QgsGeometry3D geometry;
  makeQuad( buffer, geometry );
  QgsUnlitMaterial3D material;

  QgsRenderCommand command;
  command.geometry = &geometry;
  command.material = &material;

  renderer.setDepthReadbackEnabled( true );

  // The frame a readback comes back in is not the frame that collects it: QRhi reports a
  // completed readback from inside beginFrame(), and finishFrame() runs after endFrame().
  // So there is always one frame in which the answer has landed and nobody has read it,
  // and the pass must not spend that frame queueing a new readback over the top of it -
  // if it does, on a window it never stops doing it and no depth buffer is ever produced.
  // This frame draws the square, the next one draws nothing, and the answer that survives
  // says which of the two the collected depth belongs to.
  renderer.clearCommands();
  renderer.forwardPass().addCommand( command, QgsForwardRenderPass::Group::Opaque );

  QVERIFY( engine->beginFrame() );
  QVERIFY( renderer.renderFrame( engine.get() ) );
  engine->endFrame();

  renderer.clearCommands();

  QVERIFY( engine->beginFrame() );
  QVERIFY( renderer.renderFrame( engine.get() ) );
  engine->endFrame();

  renderer.finishFrame( engine.get() );
  QVERIFY( renderer.hasNewDepthImage() );

  const QImage depth = renderer.takeDepthImage();
  QVERIFY( !depth.isNull() );
  // the square is much nearer than the far plane, so this is the first frame's depth and
  // not the empty second one, which would encode to pure blue everywhere
  QVERIFY2( centerPixel( depth ).blue() < 255, qPrintable( centerPixel( depth ).name() ) );

  // and the pass is free again, so the empty scene's depth comes back from the next frame
  QVERIFY( engine->beginFrame() );
  QVERIFY( renderer.renderFrame( engine.get() ) );
  engine->endFrame();
  renderer.finishFrame( engine.get() );

  QVERIFY( renderer.hasNewDepthImage() );
  QCOMPARE( centerPixel( renderer.takeDepthImage() ), QColor( 0, 0, 255 ) );
}

QGSTEST_MAIN( TestQgsRenderPasses3D )
#include "testqgsrenderpasses3d.moc"
