
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


#if 0
// 3D tiles when in ECEF often come with rotation on the globe.
// As we're using XZ plane with Y going up, we need to transform the coordinates
// using a rotation matrix.
QMatrix4x4 rotationMatrixEcefToYUp( QVector3D ecef )
{
  QVector3D normal = ecef.normalized();
  qDebug() << "normal" << normal;

  QVector3D up( 0, 1, 0 );
  QVector3D zAxis = normal;  // "forward"
  QVector3D xAxis = QVector3D::crossProduct( up, zAxis ).normalized(); // "right"
  QVector3D yAxis = QVector3D::crossProduct( zAxis, xAxis ).normalized(); // "up"
  qDebug() << "x" << xAxis;
  qDebug() << "y" << yAxis;
  qDebug() << "z" << zAxis;
  QMatrix3x3 m;
  float *ptr = m.data();
  // 1st column
  ptr[0] = xAxis.x();  ptr[1] = xAxis.y();  ptr[2] = xAxis.z();
  // 2nd column
  ptr[3] = yAxis.x();  ptr[4] = yAxis.y();  ptr[5] = yAxis.z();
  // 3rd column
  ptr[6] = zAxis.x();  ptr[7] = zAxis.y();  ptr[8] = zAxis.z();

  qDebug() << m;

  QMatrix4x4 m4;
  float *ptr4 = m4.data();
  // column 1
  ptr4[0] = ptr[0];  ptr4[1] = ptr[1];  ptr4[2] = ptr[2];  ptr4[3] = 0;
  // column 2
  ptr4[4] = ptr[3];  ptr4[5] = ptr[4];  ptr4[6] = ptr[5];  ptr4[7] = 0;
  // column 3
  ptr4[8] = ptr[6];  ptr4[9] = ptr[7];  ptr4[10] = ptr[8];  ptr4[11] = 0;
  // column 4
  ptr4[12] = 0;  ptr4[13] = 0;  ptr4[14] = 0;  ptr4[15] = 1;

  return m4.transposed();
}
#endif


Qt3DRender::QAttribute::VertexBaseType accessorComponentTypeToVertexBaseType( int componentType )
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

int accessorTypeToVertexSize( int type )
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

Qt3DRender::QAbstractTexture::Filter decodeTextureFilter( int filter )
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

Qt3DRender::QTextureWrapMode::WrapMode decodeTextureWrapMode( int wrapMode )
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


Qt3DRender::QAttribute *accessorToAttribute( tinygltf::Model &model, int accessorIndex )
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
  attribute->setVertexBaseType( accessorComponentTypeToVertexBaseType( accessor.componentType ) );
  attribute->setVertexSize( accessorTypeToVertexSize( accessor.type ) );

  return attribute;
}
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


Qt3DRender::QAttribute *reprojectPositions( tinygltf::Model &model, int accessorIndex, CoordsContext &coordsCtx, const QgsVector3D &tileTranslationEcef )
{
  tinygltf::Accessor &accessor = model.accessors[accessorIndex];
  tinygltf::BufferView &bv = model.bufferViews[accessor.bufferView];
  tinygltf::Buffer &b = model.buffers[bv.buffer];

  // TODO: only ever create one QBuffer for a buffer even if it is used multiple times

  // TODO: expecting only float VEC3 here
  Q_ASSERT( accessor.componentType == 5126 && accessor.type == TINYGLTF_TYPE_VEC3 );

  void *ptr = b.data.data() + bv.byteOffset + accessor.byteOffset;
  float *fptr = ( float * )ptr;

  QVector<double> vx( accessor.count ), vy( accessor.count ), vz( accessor.count );
  for ( int i = 0; i < accessor.count; ++i )
  {
    // flip coordinates from GLTF y-up to ECEF z-up
    vx[i] = tileTranslationEcef.x() + fptr[i * 3 + 0];
    vy[i] = tileTranslationEcef.y() - fptr[i * 3 + 2];
    vz[i] = tileTranslationEcef.z() + fptr[i * 3 + 1];

    // apply also transform of the node
    QgsVector3D v = coordsCtx.nodeTransform.map( QgsVector3D( vx[i], vy[i], vz[i] ) );
    vx[i] = v.x(); vy[i] = v.y(); vz[i] = v.z();
  }

  // TODO: handle exceptions
  coordsCtx.ecefToTargetCrs->transformCoords( accessor.count, vx.data(), vy.data(), vz.data() );

  QByteArray byteArray;
  byteArray.resize( accessor.count * 4 * 3 );
  float *out = ( float * )byteArray.data();

  QgsVector3D sceneOrigin = coordsCtx.sceneOriginTargetCrs;
  for ( int i = 0; i < accessor.count; ++i )
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



class TinyGltfTextureImageDataGenerator : public Qt3DRender::QTextureImageDataGenerator
{
  public:
    TinyGltfTextureImageDataGenerator( Qt3DRender::QTextureImageDataPtr imagePtr )
      : mImagePtr( imagePtr ) {}

    //Q_NOWARN_DEPRECATED_PUSH
    QT3D_FUNCTOR( TinyGltfTextureImageDataGenerator )
    //Q_NOWARN_DEPRECATED_POP

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


Qt3DRender::QMaterial *materialToMaterial( tinygltf::Model &model, int materialIndex )
{
  tinygltf::Material &material = model.materials[materialIndex];
  tinygltf::PbrMetallicRoughness &pbr = material.pbrMetallicRoughness;

  if ( pbr.baseColorTexture.index >= 0 )
  {
    tinygltf::Texture &tex = model.textures[pbr.baseColorTexture.index];

    tinygltf::Image &img = model.images[tex.source];

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
        texture->setMinificationFilter( decodeTextureFilter( sampler.minFilter ) );
      if ( sampler.magFilter >= 0 )
        texture->setMagnificationFilter( decodeTextureFilter( sampler.magFilter ) );
      Qt3DRender::QTextureWrapMode wrapMode;
      wrapMode.setX( decodeTextureWrapMode( sampler.wrapS ) );
      wrapMode.setY( decodeTextureWrapMode( sampler.wrapT ) );
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

Qt3DCore::QEntity *entityForNode( tinygltf::Model &model, int nodeIndex, CoordsContext &coordsCtx, QgsVector3D &tileTranslationEcef )
{
  tinygltf::Node &node = model.nodes[nodeIndex];

  Qt3DCore::QEntity *e = new Qt3DCore::QEntity;

  if ( node.translation.size() )
  {
    tileTranslationEcef = QgsVector3D( node.translation[0], -node.translation[2], node.translation[1] );

    //rootTranslation = QVector3D(node.translation[0], node.translation[1], node.translation[2]);
    //qDebug()<< "TR" << rootTranslation;
  }

  if ( model.extensions.find( "CESIUM_RTC" ) != model.extensions.end() )
  {
    tinygltf::Value v = model.extensions["CESIUM_RTC"];
    Q_ASSERT( v.IsObject() && v.Has( "center" ) );
    tinygltf::Value center = v.Get( "center" );
    Q_ASSERT( center.IsArray() && center.Size() == 3 );
    tileTranslationEcef = QgsVector3D( center.Get( 0 ).GetNumberAsDouble(), center.Get( 1 ).GetNumberAsDouble(), center.Get( 2 ).GetNumberAsDouble() );
    //qDebug()<< "TR" << rootTranslation;
  }

  //        Qt3DCore::QTransform *tr = new Qt3DCore::QTransform;
  //        tr->setTranslation(QVector3D(node.translation[0], node.translation[1], node.translation[2]));
  //        e->addComponent(tr);

  // TODO: load transform (node: rotation, scale, translation  --or-- matrix)

  if ( node.mesh >= 0 )
  {
    tinygltf::Mesh &mesh = model.meshes[node.mesh];

    for ( const tinygltf::Primitive &primitive : mesh.primitives )
    {
      // TODO: support other primitive types
      // see https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#_mesh_primitive_mode
      Q_ASSERT( primitive.mode == 4 ); // triangles

      // TODO: assuming always indexed
      Q_ASSERT( primitive.indices != -1 );

      Qt3DRender::QGeometry *geom = new Qt3DRender::QGeometry;

      auto posIt = primitive.attributes.find( "POSITION" );
      Q_ASSERT( posIt != primitive.attributes.end() );
      int positionAccessorIndex = posIt->second;

      Qt3DRender::QAttribute *positionAttribute;
      if ( coordsCtx.ecefToTargetCrs )
        positionAttribute = reprojectPositions( model, positionAccessorIndex, coordsCtx, tileTranslationEcef );
      else
        positionAttribute = accessorToAttribute( model, positionAccessorIndex );

      positionAttribute->setName( Qt3DRender::QAttribute::defaultPositionAttributeName() );
      geom->addAttribute( positionAttribute );

      //dumpAttributeData(model, positionAccessorIndex, node.translation[0], -node.translation[2], node.translation[1]);

      auto normalIt = primitive.attributes.find( "NORMAL" );
      if ( normalIt != primitive.attributes.end() )
      {
        int normalAccessorIndex = normalIt->second;
        Qt3DRender::QAttribute *normalAttribute = accessorToAttribute( model, normalAccessorIndex );
        normalAttribute->setName( Qt3DRender::QAttribute::defaultNormalAttributeName() );
        geom->addAttribute( normalAttribute );
      }

      auto texIt = primitive.attributes.find( "TEXCOORD_0" );
      if ( texIt != primitive.attributes.end() )
      {
        int texAccessorIndex = texIt->second;
        Qt3DRender::QAttribute *texAttribute = accessorToAttribute( model, texAccessorIndex );
        texAttribute->setName( Qt3DRender::QAttribute::defaultTextureCoordinateAttributeName() );
        geom->addAttribute( texAttribute );
      }

      // TODO: other attrs?

      Qt3DRender::QAttribute *indexAttribute = accessorToAttribute( model, primitive.indices );
      geom->addAttribute( indexAttribute );

      Qt3DRender::QGeometryRenderer *geomRenderer = new Qt3DRender::QGeometryRenderer;
      geomRenderer->setGeometry( geom );
      geomRenderer->setPrimitiveType( Qt3DRender::QGeometryRenderer::Triangles ); // looks like same values as "mode"
      geomRenderer->setVertexCount( indexAttribute->count() );

      Qt3DRender::QMaterial *material = materialToMaterial( model, primitive.material );

      Qt3DCore::QEntity *primitiveEntity = new Qt3DCore::QEntity( e );
      primitiveEntity->addComponent( geomRenderer );
      primitiveEntity->addComponent( material );
    }
  }

  // TODO: recursively add children
  if ( node.children.size() != 0 )
  {
    qDebug() << "unsupported: root node has children: " << node.children.size();
  }

  return e;
}



QMatrix4x4 tileToSceneTransform( QVector3D sceneOriginECEF, QVector3D tileRootTranslation )
{

  QMatrix4x4 translateECEFtoSCENE;
  translateECEFtoSCENE.translate( -sceneOriginECEF );

  QMatrix4x4 rotateYUPtoZUP(
    1.0, 0.0,  0.0, 0.0,
    0.0, 0.0, -1.0, 0.0,
    0.0, 1.0,  0.0, 0.0,
    0.0, 0.0,  0.0, 1.0
  );

  QMatrix4x4 translateTILEtoECEF;
  translateTILEtoECEF.translate( tileRootTranslation ); //QVector3D(-2693952.395885576, 3854579.3104452426, 4297074.539132856));
  qDebug() << "tile to ecef";
  qDebug() << translateTILEtoECEF;
  qDebug() << translateTILEtoECEF.map( QVector3D( 500, 0, 0 ) );

  QMatrix4x4 combined = rotateYUPtoZUP * translateTILEtoECEF;
  qDebug() << combined.map( QVector3D( 500, 0, 0 ) );

  QMatrix4x4 combinedFull = translateECEFtoSCENE * combined;
  qDebug() << combinedFull.map( QVector3D( 500, 0, 0 ) );

  return combinedFull;
}


static Qt3DCore::QEntity *gltfModelToEntity( tinygltf::Model &model, CoordsContext &coordsCtx )
{
  using namespace tinygltf;

  // model - has possibly multiple scenes (one scene is the default)
  // scene has multiple root nodes
  //   - nodes may have child nodes

  // scene = root QEntity
  // each node = QEntity

  Scene &scene = model.scenes[model.defaultScene];

  QgsVector3D tileTranslationEcef;
  Qt3DCore::QEntity *gltfEntity = entityForNode( model, scene.nodes[0], coordsCtx, tileTranslationEcef );

  if ( !coordsCtx.ecefToTargetCrs )
  {
    // only apply offset + rotation when not projecting to a plane

    QMatrix4x4 rotateYUPtoZUP(
      1.0, 0.0,  0.0, 0.0,
      0.0, 0.0, -1.0, 0.0,
      0.0, 1.0,  0.0, 0.0,
      0.0, 0.0,  0.0, 1.0
    );
    QgsVector3D b = coordsCtx.sceneOriginTargetCrs, a = tileTranslationEcef;
    rotateYUPtoZUP.setColumn( 3, QVector4D( a.x() - b.x(), a.y() - b.y(), a.z() - b.z(), 1 ) );

    Qt3DCore::QTransform *tileTransform = new Qt3DCore::QTransform;
    tileTransform->setMatrix( rotateYUPtoZUP );
    gltfEntity->addComponent( tileTransform );
  }

  return gltfEntity;
}

// tinygltf can use STB_image to read images (e.g. JPG), but let's rather use
// QImage to avoid another dependency (although STB_image is just one big header file)
static bool loadImageDataWithQImage(
  tinygltf::Image *image, const int image_idx, std::string *err,
  std::string *warn, int req_width, int req_height,
  const unsigned char *bytes, int size, void *user_data )
{
  //qDebug() << "LoadImageData" << image << image_idx << req_width << req_height << bytes << size << user_data;

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

Qt3DCore::QEntity *gltfMemoryToEntity( const QByteArray &data, CoordsContext &coordsCtx )
{
  using namespace tinygltf;

  Model model;
  TinyGLTF loader;
  std::string err;
  std::string warn;

  loader.SetImageLoader( loadImageDataWithQImage, nullptr );

  bool res = loader.LoadBinaryFromMemory( &model, &err, &warn,
                                          ( const unsigned char * )data.constData(), data.size(), "", REQUIRE_VERSION );
  if ( !res )
  {
    qDebug() << "errors: " << err.data();
    return new Qt3DCore::QEntity;  // TODO
  }

  return gltfModelToEntity( model, coordsCtx );
}


Qt3DCore::QEntity *gltfToEntity( QString path, CoordsContext &coordsCtx )
{
  using namespace tinygltf;

  Model model;
  TinyGLTF loader;
  std::string err;
  std::string warn;

  loader.SetImageLoader( loadImageDataWithQImage, nullptr );

  QByteArray pathBA = path.toUtf8();
  const char *filename = pathBA.constData();

  bool res;
  if ( QString( filename ).endsWith( ".glb" ) )
  {
    //qDebug() << "GLB" << filename;
    res = loader.LoadBinaryFromFile( &model, &err, &warn, filename );
  }
  else
  {
    //qDebug() << "GLTF (ascii)" << filename;
    res = loader.LoadASCIIFromFile( &model, &err, &warn, filename );
  }
  if ( !res )
  {
    qDebug() << "errors: " << err.data();
    return new Qt3DCore::QEntity;  // TODO
  }

  return gltfModelToEntity( model, coordsCtx );
}
