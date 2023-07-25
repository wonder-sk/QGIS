
#include "3dtiles.h"

#include <Qt3DCore>
#include <Qt3DRender>
#include <Qt3DExtras>

#define TINYGLTF_IMPLEMENTATION       // should be just in one file
#define TINYGLTF_ENABLE_DRACO         // needs libdraco-dev
#define TINYGLTF_NO_STB_IMAGE         // we use QImage-based reading of images
#define TINYGLTF_NO_STB_IMAGE_WRITE   // we don't need writing of images

#include "tiny_gltf.h"

#include "qgscoordinatetransform.h"

static bool loadImageDataWithQImage(
  tinygltf::Image *image, const int image_idx, std::string *err,
  std::string *warn, int req_width, int req_height,
  const unsigned char *bytes, int size, void *user_data );


Qt3DRender::QAttribute::VertexBaseType parseVertexBaseType( int componentType )
{
  switch ( componentType )
  {
    case 5120: // BYTE
      return Qt3DRender::QAttribute::Byte;
    case 5121: // UNSIGNED_BYTE
      return Qt3DRender::QAttribute::UnsignedByte;
    case 5122: // SHORT
      return Qt3DRender::QAttribute::Short;
    case 5123: // UNSIGNED_SHORT
      return Qt3DRender::QAttribute::UnsignedShort;
    case 5125: // UNSIGNED_INT
      return Qt3DRender::QAttribute::UnsignedInt;
    case 5126: // FLOAT
      return Qt3DRender::QAttribute::Float;
  }
  Q_ASSERT( false );
  return Qt3DRender::QAttribute::UnsignedInt;
}

int parseVertexSize( int type )
{
  switch ( type )
  {
    case TINYGLTF_TYPE_SCALAR: return 1;
    case TINYGLTF_TYPE_VEC2: return 2;
    case TINYGLTF_TYPE_VEC3: return 3;
    case TINYGLTF_TYPE_VEC4: return 4;
    case TINYGLTF_TYPE_MAT2: return 4;
    case TINYGLTF_TYPE_MAT3: return 9;
    case TINYGLTF_TYPE_MAT4: return 16;
  }
  Q_ASSERT( false );
  return 1;
}

Qt3DRender::QAbstractTexture::Filter parseTextureFilter( int filter )
{
  switch ( filter )
  {
    case TINYGLTF_TEXTURE_FILTER_NEAREST: return Qt3DRender::QTexture2D::Nearest;
    case TINYGLTF_TEXTURE_FILTER_LINEAR: return Qt3DRender::QTexture2D::Linear;
    case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST: return Qt3DRender::QTexture2D::NearestMipMapNearest;
    case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST: return Qt3DRender::QTexture2D::LinearMipMapNearest;
    case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR: return Qt3DRender::QTexture2D::NearestMipMapLinear;
    case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR: return Qt3DRender::QTexture2D::LinearMipMapLinear;
  }
  Q_ASSERT( false );
  return Qt3DRender::QTexture2D::Nearest;
}

Qt3DRender::QTextureWrapMode::WrapMode parseTextureWrapMode( int wrapMode )
{
  switch ( wrapMode )
  {
    case TINYGLTF_TEXTURE_WRAP_REPEAT: return Qt3DRender::QTextureWrapMode::Repeat;
    case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE: return Qt3DRender::QTextureWrapMode::ClampToEdge;
    case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT: return Qt3DRender::QTextureWrapMode::MirroredRepeat;
  }
  Q_ASSERT( false );
  return Qt3DRender::QTextureWrapMode::Repeat;
}


Qt3DRender::QAttribute *parseAttribute( tinygltf::Model &model, int accessorIndex )
{
  tinygltf::Accessor &accessor = model.accessors[accessorIndex];
  tinygltf::BufferView &bv = model.bufferViews[accessor.bufferView];
  tinygltf::Buffer &b = model.buffers[bv.buffer];

  // TODO: only ever create one QBuffer for a buffer even if it is used multiple times
  QByteArray byteArray( ( const char * )b.data.data(), ( int )b.data.size() ); // makes a deep copy
  Qt3DRender::QBuffer *buffer = new Qt3DRender::QBuffer();
  buffer->setData( byteArray );

  // TODO: check that bv.byteSize is related to "count" of accessor?

  Qt3DRender::QAttribute *attribute = new Qt3DRender::QAttribute();

  // "target" is optional, can be zero
  if ( bv.target == TINYGLTF_TARGET_ARRAY_BUFFER )
    attribute->setAttributeType( Qt3DRender::QAttribute::VertexAttribute );
  else if ( bv.target == TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER )
    attribute->setAttributeType( Qt3DRender::QAttribute::IndexAttribute );

  attribute->setBuffer( buffer );
  attribute->setByteOffset( bv.byteOffset + accessor.byteOffset );
  attribute->setByteStride( bv.byteStride );  // could be zero, it seems that's fine (assuming packed)
  attribute->setCount( accessor.count );
  attribute->setVertexBaseType( parseVertexBaseType( accessor.componentType ) );
  attribute->setVertexSize( parseVertexSize( accessor.type ) );

  return attribute;
}

#if 0
#include <QFile>
void dumpAttributeData( tinygltf::Model &model, int accessorIndex, double offsetX = 0, double offsetY = 0, double offsetZ = 0 )
{
  tinygltf::Accessor &accessor = model.accessors[accessorIndex];
  tinygltf::BufferView &bv = model.bufferViews[accessor.bufferView];
  tinygltf::Buffer &b = model.buffers[bv.buffer];

  // TODO: assuming float VEC3

  QFile f( "/tmp/vertices.csv" );
  f.open( QIODevice::WriteOnly );
  f.write( "x,y,z\n" );

  void *ptr = b.data.data() + bv.byteOffset + accessor.byteOffset;
  float *fptr = ( float * )ptr;
  for ( int i = 0; i < accessor.count; ++i )
  {
    qDebug() << i << " == " << fptr[i * 3 + 0] << fptr[i * 3 + 1] << fptr[i * 3 + 2];
    // flip coords
    QString s = QString( "%1,%2,%3\n" ).arg( offsetX + fptr[i * 3 + 0], 0, 'f' ).arg( offsetY - fptr[i * 3 + 2], 0, 'f' ).arg( offsetZ + fptr[i * 3 + 1], 0, 'f' );
    f.write( s.toUtf8() );
  }
}
#endif

Qt3DRender::QAttribute *reprojectPositions( tinygltf::Model &model, int accessorIndex, CoordsContext &coordsCtx, const QgsVector3D &tileTranslationEcef, QMatrix4x4 *matrix )
{
  tinygltf::Accessor &accessor = model.accessors[accessorIndex];
  tinygltf::BufferView &bv = model.bufferViews[accessor.bufferView];
  tinygltf::Buffer &b = model.buffers[bv.buffer];

  // TODO: only ever create one QBuffer for a buffer even if it is used multiple times

  // TODO: expecting only float VEC3 here
  Q_ASSERT( accessor.componentType == 5126 && accessor.type == TINYGLTF_TYPE_VEC3 );

  char *ptr = ( char * ) b.data.data() + bv.byteOffset + accessor.byteOffset;

  QVector<double> vx( accessor.count ), vy( accessor.count ), vz( accessor.count );
  for ( int i = 0; i < ( int )accessor.count; ++i )
  {
    float *fptr = ( float * )ptr;
    QVector3D vOrig( fptr[0], fptr[1], fptr[2] );

    if ( matrix )
      vOrig = matrix->map( vOrig );

    // go from y-up to z-up according to 3D Tiles spec
    QVector3D vFlip( vOrig.x(), -vOrig.z(), vOrig.y() );

    // apply also transform of the node
    QgsVector3D v = coordsCtx.nodeTransform.map( QgsVector3D( vFlip ) + tileTranslationEcef );
    vx[i] = v.x(); vy[i] = v.y(); vz[i] = v.z();

    if ( bv.byteStride )
      ptr += bv.byteStride;
    else
      ptr += 3 * sizeof( float );
  }

  // TODO: handle exceptions
  coordsCtx.ecefToTargetCrs->transformCoords( accessor.count, vx.data(), vy.data(), vz.data() );

  QByteArray byteArray;
  byteArray.resize( accessor.count * 4 * 3 );
  float *out = ( float * )byteArray.data();

  QgsVector3D sceneOrigin = coordsCtx.sceneOriginTargetCrs;
  for ( int i = 0; i < ( int )accessor.count; ++i )
  {
    double x = vx[i] - sceneOrigin.x();
    double y = vy[i] - sceneOrigin.y();
    double z = vz[i] - sceneOrigin.z();

    // QGIS 3D uses base plane (X,-Z) with Y up - so flip the coordinates
    out[i * 3 + 0] = x;
    out[i * 3 + 1] = z;
    out[i * 3 + 2] = -y;
  }

  Qt3DRender::QBuffer *buffer = new Qt3DRender::QBuffer();
  buffer->setData( byteArray );

  // TODO: check that bv.byteSize is related to "count" of accessor?

  Qt3DRender::QAttribute *attribute = new Qt3DRender::QAttribute();

  // "target" is optional, can be zero
  if ( bv.target == TINYGLTF_TARGET_ARRAY_BUFFER )
    attribute->setAttributeType( Qt3DRender::QAttribute::VertexAttribute );
  else if ( bv.target == TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER )
    attribute->setAttributeType( Qt3DRender::QAttribute::IndexAttribute );

  attribute->setBuffer( buffer );
  attribute->setByteOffset( 0 );
  attribute->setByteStride( 12 );
  attribute->setCount( accessor.count );
  attribute->setVertexBaseType( Qt3DRender::QAttribute::Float );
  attribute->setVertexSize( 3 );

  return attribute;
}

// QAbstractFunctor marked as deprecated in 5.15, but undeprecated for Qt 6.0. TODO -- remove when we require 6.0
Q_NOWARN_DEPRECATED_PUSH

class TinyGltfTextureImageDataGenerator : public Qt3DRender::QTextureImageDataGenerator
{
  public:
    TinyGltfTextureImageDataGenerator( Qt3DRender::QTextureImageDataPtr imagePtr )
      : mImagePtr( imagePtr ) {}

    QT3D_FUNCTOR( TinyGltfTextureImageDataGenerator )

    Qt3DRender::QTextureImageDataPtr operator()() override
    {
      return mImagePtr;
    }

    bool operator ==( const QTextureImageDataGenerator &other ) const override
    {
      const TinyGltfTextureImageDataGenerator *otherFunctor = functor_cast<TinyGltfTextureImageDataGenerator>( &other );
      return mImagePtr.get() == otherFunctor->mImagePtr.get();
    }

    Qt3DRender::QTextureImageDataPtr mImagePtr;
};

Q_NOWARN_DEPRECATED_POP

class TinyGltfTextureImage : public Qt3DRender::QAbstractTextureImage
{
  public:
    TinyGltfTextureImage( tinygltf::Image &image )
    {
      Q_ASSERT( image.bits == 8 );
      Q_ASSERT( image.component == 4 );
      Q_ASSERT( image.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE );

#if 0
      QImage i( img.width, image.height, QImage::Format_RGBA8888 );
      memcpy( i.bits(), image.image.data(), image.width * image.height * 4 );
      i.save( "/tmp/aaa.png" );
#endif

      imgDataPtr.reset( new Qt3DRender::QTextureImageData );
      imgDataPtr->setWidth( image.width );
      imgDataPtr->setHeight( image.height );
      imgDataPtr->setDepth( 1 ); // not sure what this is
      imgDataPtr->setFaces( 1 );
      imgDataPtr->setLayers( 1 );
      imgDataPtr->setMipLevels( 1 );
      QByteArray imageBytes( ( const char * )image.image.data(), image.image.size() );
      imgDataPtr->setData( imageBytes, 4 );
      imgDataPtr->setFormat( QOpenGLTexture::RGBA8_UNorm );
      imgDataPtr->setPixelFormat( QOpenGLTexture::BGRA ); // when using tinygltf with STB_image, pixel format is QOpenGLTexture::RGBA
      imgDataPtr->setPixelType( QOpenGLTexture::UInt8 );
      imgDataPtr->setTarget( QOpenGLTexture::Target2D );
    }

    Qt3DRender::QTextureImageDataGeneratorPtr dataGenerator() const override
    {
      return Qt3DRender::QTextureImageDataGeneratorPtr( new TinyGltfTextureImageDataGenerator( imgDataPtr ) );
    }

    Qt3DRender::QTextureImageDataPtr imgDataPtr;
};


static Qt3DRender::QMaterial *parseMaterial( tinygltf::Model &model, int materialIndex, QString baseUri )
{
  tinygltf::Material &material = model.materials[materialIndex];
  tinygltf::PbrMetallicRoughness &pbr = material.pbrMetallicRoughness;

  if ( pbr.baseColorTexture.index >= 0 )
  {
    tinygltf::Texture &tex = model.textures[pbr.baseColorTexture.index];

    tinygltf::Image &img = model.images[tex.source];

    if ( !img.uri.empty() )
    {
      // TODO: if using a remote URI, we may need to do a network request
      QString imgUri = QString::fromStdString( img.uri );
      if ( imgUri.startsWith( "./" ) )
        imgUri = QFileInfo( baseUri ).absolutePath() + "/" + imgUri;

      qDebug() << "loading texture image " << imgUri;
      QFile f( imgUri );
      if ( f.open( QIODevice::ReadOnly ) )
      {
        QByteArray ba = f.readAll();
        if ( !loadImageDataWithQImage( &img, -1, nullptr, nullptr, 0, 0, ( const unsigned char * ) ba.constData(), ba.size(), nullptr ) )
        {
          qDebug() << "failed to load image " << imgUri;
        }
      }
      else
      {
        qDebug() << "can't open image " << imgUri;
      }
    }

    if ( img.image.empty() )
    {
      // TODO: what else to do?
      Qt3DExtras::QMetalRoughMaterial *pbrMaterial = new Qt3DExtras::QMetalRoughMaterial;
      pbrMaterial->setMetalness( pbr.metallicFactor ); // [0..1] or texture
      pbrMaterial->setRoughness( pbr.roughnessFactor );
      pbrMaterial->setBaseColor( QColor::fromRgbF( pbr.baseColorFactor[0], pbr.baseColorFactor[1], pbr.baseColorFactor[2], pbr.baseColorFactor[3] ) );
      return pbrMaterial;
    }

    TinyGltfTextureImage *textureImage = new TinyGltfTextureImage( img );

    Qt3DRender::QTexture2D *texture = new Qt3DRender::QTexture2D;
    texture->addTextureImage( textureImage );//texture take the ownership of textureImage if has no parant

    // let's use linear (rather than nearest) filtering by default to avoid blocky look of textures
    texture->setMinificationFilter( Qt3DRender::QTexture2D::Linear );
    texture->setMagnificationFilter( Qt3DRender::QTexture2D::Linear );

    if ( tex.sampler >= 0 )
    {
      tinygltf::Sampler &sampler = model.samplers[tex.sampler];
      if ( sampler.minFilter >= 0 )
        texture->setMinificationFilter( parseTextureFilter( sampler.minFilter ) );
      if ( sampler.magFilter >= 0 )
        texture->setMagnificationFilter( parseTextureFilter( sampler.magFilter ) );
      Qt3DRender::QTextureWrapMode wrapMode;
      wrapMode.setX( parseTextureWrapMode( sampler.wrapS ) );
      wrapMode.setY( parseTextureWrapMode( sampler.wrapT ) );
      texture->setWrapMode( wrapMode );
    }
    else
    {
      // TODO: "When undefined, a sampler with repeat wrapping and auto filtering SHOULD be used."
    }

// TODO: we should be using PBR material unless unlit material is requested,
// but then we need to deal with the lights somehow...
#if 1
    Qt3DExtras::QTextureMaterial *mat = new Qt3DExtras::QTextureMaterial;
    mat->setTexture( texture );
    return mat;
#else
    Qt3DExtras::QMetalRoughMaterial *pbrMaterial = new Qt3DExtras::QMetalRoughMaterial;
    pbrMaterial->setMetalness( pbr.metallicFactor ); // [0..1] or texture
    pbrMaterial->setRoughness( pbr.roughnessFactor );
    pbrMaterial->setBaseColor( QVariant::fromValue( texture ) );
    return pbrMaterial;
#endif
  }

  Qt3DExtras::QMetalRoughMaterial *pbrMaterial = new Qt3DExtras::QMetalRoughMaterial;
  pbrMaterial->setMetalness( pbr.metallicFactor ); // [0..1] or texture
  pbrMaterial->setRoughness( pbr.roughnessFactor );
  pbrMaterial->setBaseColor( QColor::fromRgbF( pbr.baseColorFactor[0], pbr.baseColorFactor[1], pbr.baseColorFactor[2], pbr.baseColorFactor[3] ) );
  return pbrMaterial;
}


static std::unique_ptr<QMatrix4x4> parseNodeTransform( tinygltf::Node &node )
{
  // read node's transform: either specified with 4x4 "matrix" element
  // -OR- given by "translation", "rotation" and "scale" elements (to be combined as T * R * S)
  std::unique_ptr<QMatrix4x4> matrix;
  if ( !node.matrix.empty() )
  {
    matrix.reset( new QMatrix4x4 );
    float *mdata = matrix->data();
    for ( int i = 0; i < 16; ++i )
      mdata[i] = node.matrix[i];
  }
  else if ( node.translation.size() || node.rotation.size() || node.scale.size() )
  {
    matrix.reset( new QMatrix4x4 );
    if ( node.scale.size() )
    {
      matrix->scale( node.scale[0], node.scale[1], node.scale[2] );
    }
    if ( node.rotation.size() )
    {
      matrix->rotate( QQuaternion( node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2] ) );
    }
    if ( node.translation.size() )
    {
      matrix->translate( node.translation[0], node.translation[1], node.translation[2] );
    }
  }
  return matrix;
}



static Qt3DCore::QEntity *parseNode( tinygltf::Model &model, int nodeIndex, CoordsContext &coordsCtx, QgsVector3D &tileTranslationEcef, QString baseUri, QMatrix4x4 parentTransform )
{
  tinygltf::Node &node = model.nodes[nodeIndex];

  Qt3DCore::QEntity *e = new Qt3DCore::QEntity;

  // transform
  std::unique_ptr<QMatrix4x4> matrix = parseNodeTransform( node );
  if ( !parentTransform.isIdentity() )
  {
    if ( matrix )
      *matrix = parentTransform * *matrix;
    else
    {
      matrix.reset( new QMatrix4x4( parentTransform ) );
    }
  }

  // mesh
  if ( node.mesh >= 0 )
  {
    tinygltf::Mesh &mesh = model.meshes[node.mesh];

    for ( const tinygltf::Primitive &primitive : mesh.primitives )
    {
      if ( primitive.mode != 4 )
      {
        qDebug() << "unsupported primitive " << primitive.mode;
        continue;
      }
      // TODO: support other primitive types
      // see https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#_mesh_primitive_mode
      Q_ASSERT( primitive.mode == 4 ); // triangles

      Qt3DRender::QGeometry *geom = new Qt3DRender::QGeometry;

      auto posIt = primitive.attributes.find( "POSITION" );
      Q_ASSERT( posIt != primitive.attributes.end() );
      int positionAccessorIndex = posIt->second;

      Qt3DRender::QAttribute *positionAttribute = reprojectPositions( model, positionAccessorIndex, coordsCtx, tileTranslationEcef, matrix.get() );
      positionAttribute->setName( Qt3DRender::QAttribute::defaultPositionAttributeName() );
      geom->addAttribute( positionAttribute );

      //dumpAttributeData(model, positionAccessorIndex, tileTranslationEcef.x(), tileTranslationEcef.y(), tileTranslationEcef.z() );

      auto normalIt = primitive.attributes.find( "NORMAL" );
      if ( normalIt != primitive.attributes.end() )
      {
        int normalAccessorIndex = normalIt->second;
        Qt3DRender::QAttribute *normalAttribute = parseAttribute( model, normalAccessorIndex );
        normalAttribute->setName( Qt3DRender::QAttribute::defaultNormalAttributeName() );
        geom->addAttribute( normalAttribute );
      }

      auto texIt = primitive.attributes.find( "TEXCOORD_0" );
      if ( texIt != primitive.attributes.end() )
      {
        int texAccessorIndex = texIt->second;
        Qt3DRender::QAttribute *texAttribute = parseAttribute( model, texAccessorIndex );
        texAttribute->setName( Qt3DRender::QAttribute::defaultTextureCoordinateAttributeName() );
        geom->addAttribute( texAttribute );
      }

      // TODO: other attrs?

      Qt3DRender::QAttribute *indexAttribute = nullptr;
      if ( primitive.indices != -1 )
      {
        indexAttribute = parseAttribute( model, primitive.indices );
        geom->addAttribute( indexAttribute );
      }

      Qt3DRender::QGeometryRenderer *geomRenderer = new Qt3DRender::QGeometryRenderer;
      geomRenderer->setGeometry( geom );
      geomRenderer->setPrimitiveType( Qt3DRender::QGeometryRenderer::Triangles ); // looks like same values as "mode"
      geomRenderer->setVertexCount( indexAttribute ? indexAttribute->count() : model.accessors[positionAccessorIndex].count );

      Qt3DRender::QMaterial *material = parseMaterial( model, primitive.material, baseUri );

      Qt3DCore::QEntity *primitiveEntity = new Qt3DCore::QEntity( e );
      primitiveEntity->addComponent( geomRenderer );
      primitiveEntity->addComponent( material );
    }
  }

  // recursively add children
  if ( node.children.size() != 0 )
  {
    for ( int childNodeIndex : node.children )
    {
      Qt3DCore::QEntity *eChild = parseNode( model, childNodeIndex, coordsCtx, tileTranslationEcef, baseUri, matrix ? *matrix : QMatrix4x4() );
      eChild->setParent( e );
    }
  }

  return e;
}


static Qt3DCore::QEntity *parseModel( tinygltf::Model &model, CoordsContext &coordsCtx, QString baseUri )
{
  using namespace tinygltf;

  // model - has possibly multiple scenes (one scene is the default)
  // scene has multiple root nodes
  //   - nodes may have child nodes

  // scene = root QEntity
  // each node = QEntity

  Scene &scene = model.scenes[model.defaultScene];

  QgsVector3D tileTranslationEcef;
  if ( model.extensions.find( "CESIUM_RTC" ) != model.extensions.end() )
  {
    tinygltf::Value v = model.extensions["CESIUM_RTC"];
    Q_ASSERT( v.IsObject() && v.Has( "center" ) );
    tinygltf::Value center = v.Get( "center" );
    Q_ASSERT( center.IsArray() && center.Size() == 3 );
    tileTranslationEcef = QgsVector3D( center.Get( 0 ).GetNumberAsDouble(), center.Get( 1 ).GetNumberAsDouble(), center.Get( 2 ).GetNumberAsDouble() );
    //qDebug()<< "TR" << rootTranslation;
  }

  if ( scene.nodes.size() == 0 )
  {
    qDebug() << "No nodes present in the gltf data!";
    return nullptr;
  }

  if ( scene.nodes.size() > 1 )
  {
    // TODO: handle multiple root nodes
    qDebug() << "GLTF scene contains multiple nodes: only loading the first one!";
  }

  int rootNodeIndex = scene.nodes[0];
  tinygltf::Node &rootNode = model.nodes[rootNodeIndex];

  if ( tileTranslationEcef.isNull() && rootNode.translation.size() )
  {
    QgsVector3D rootTranslation( rootNode.translation[0], rootNode.translation[1], rootNode.translation[2] );

    // if root node of a GLTF contains translation by a large amount, let's handle it as the tile translation.
    // this will ensure that we keep double precision rather than loosing precision when dealing with floats
    if ( rootTranslation.length() > 1e6 )
    {
      // we flip Y/Z axes here because GLTF uses Y-up convention, while 3D Tiles use Z-up convention
      tileTranslationEcef = QgsVector3D( rootTranslation.x(), -rootTranslation.z(), rootTranslation.y() );
      rootNode.translation[0] = rootNode.translation[1] = rootNode.translation[2] = 0;
    }
  }

  Qt3DCore::QEntity *gltfEntity = parseNode( model, rootNodeIndex, coordsCtx, tileTranslationEcef, baseUri, QMatrix4x4() );
  return gltfEntity;
}

// tinygltf can use STB_image to read images (e.g. JPG), but let's rather use
// QImage to avoid another dependency (although STB_image is just one big header file)
static bool loadImageDataWithQImage(
  tinygltf::Image *image, const int image_idx, std::string *err,
  std::string *warn, int req_width, int req_height,
  const unsigned char *bytes, int size, void *user_data )
{
  qDebug() << "LoadImageData" << image << image_idx << req_width << req_height << bytes << size << user_data;

  Q_ASSERT( req_width == 0 && req_height == 0 );  // unsure why we would request a particular width/height

  ( void )warn;
  ( void )user_data;

  QImage img;
  if ( !img.loadFromData( bytes, size ) )
  {
    if ( err )
    {
      ( *err ) +=
        "Unknown image format. QImage cannot decode image data for image[" +
        std::to_string( image_idx ) + "] name = \"" + image->name + "\".\n";
    }
    return false;
  }

  Q_ASSERT( img.format() == QImage::Format_RGB32 || img.format() == QImage::Format_ARGB32 );

  image->width = img.width();
  image->height = img.height();
  image->component = 4;
  image->bits = 8;
  image->pixel_type = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;

  image->image.resize( static_cast<size_t>( image->width * image->height * image->component ) * size_t( image->bits / 8 ) );
  std::copy( img.constBits(), img.constBits() + image->width * image->height * image->component * ( image->bits / 8 ), image->image.begin() );

  return true;
}


Qt3DCore::QEntity *gltfToEntity( const QByteArray &data, CoordsContext &coordsCtx, QString baseUri )
{
  using namespace tinygltf;

  Model model;
  TinyGLTF loader;
  std::string err;
  std::string warn;

  loader.SetImageLoader( loadImageDataWithQImage, nullptr );

  std::string baseDir;  // TODO: may be useful to set it from baseUri

  bool res;
  if ( data.startsWith( "glTF" ) )   // 4-byte magic value in binary GLTF
  {
    res = loader.LoadBinaryFromMemory( &model, &err, &warn,
                                       ( const unsigned char * )data.constData(), data.size(), baseDir );
  }
  else
  {
    res = loader.LoadASCIIFromString( &model, &err, &warn,
                                      data.constData(), data.size(), baseDir );
  }
  if ( !res )
  {
    qDebug() << "errors: " << err.data();
    return new Qt3DCore::QEntity;
  }

  return parseModel( model, coordsCtx, baseUri );
}
