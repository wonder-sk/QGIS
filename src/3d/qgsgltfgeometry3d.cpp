/***************************************************************************
  qgsgltfgeometry3d.cpp
  --------------------------------------
  Date                 : September 2026
  Copyright            : (C) 2026 by Martin Dobias
  Email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsgltfgeometry3d.h"

#include <optional>

#include <QString>

#define TINYGLTF_NO_STB_IMAGE       // we use QImage-based reading of images
#define TINYGLTF_NO_STB_IMAGE_WRITE // we don't need writing of images
#include "tiny_gltf.h"

#include <QString>

using namespace Qt::StringLiterals;

///@cond PRIVATE

namespace
{
  //! Size in bytes of one component of a glTF accessor
  int componentSize( int componentType )
  {
    return tinygltf::GetComponentSizeInBytes( static_cast<uint32_t>( componentType ) );
  }

  /**
   * Returns the QRhi vertex format matching a glTF accessor, if there is an exact one.
   *
   * QRhi's format list is narrower than glTF's: it has no normalised shorts, no signed
   * normalised bytes and no three component byte format, all of which glTF allows. When
   * this returns nothing the accessor has to be unpacked to floats instead.
   */
  std::optional<QRhiVertexInputAttribute::Format> exactFormat( int componentType, int type, bool normalized )
  {
    const int components = tinygltf::GetNumComponentsInType( static_cast<uint32_t>( type ) );
    if ( components < 1 || components > 4 )
      return std::nullopt;

    switch ( componentType )
    {
      case TINYGLTF_COMPONENT_TYPE_FLOAT:
      {
        static constexpr QRhiVertexInputAttribute::Format formats[4]
          = { QRhiVertexInputAttribute::Float, QRhiVertexInputAttribute::Float2, QRhiVertexInputAttribute::Float3, QRhiVertexInputAttribute::Float4 };
        return formats[components - 1];
      }

      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        if ( !normalized )
          return std::nullopt; // QRhi has no plain unsigned byte vertex format
        switch ( components )
        {
          case 1:
            return QRhiVertexInputAttribute::UNormByte;
          case 2:
            return QRhiVertexInputAttribute::UNormByte2;
          case 4:
            return QRhiVertexInputAttribute::UNormByte4;
          default:
            return std::nullopt; // there is no UNormByte3
        }

      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        if ( normalized )
          return std::nullopt; // QRhi has no normalised short format
        switch ( components )
        {
          case 2:
            return QRhiVertexInputAttribute::UShort2;
          case 3:
            return QRhiVertexInputAttribute::UShort3;
          case 4:
            return QRhiVertexInputAttribute::UShort4;
          default:
            return QRhiVertexInputAttribute::UShort;
        }

      case TINYGLTF_COMPONENT_TYPE_SHORT:
        if ( normalized )
          return std::nullopt;
        switch ( components )
        {
          case 2:
            return QRhiVertexInputAttribute::SShort2;
          case 3:
            return QRhiVertexInputAttribute::SShort3;
          case 4:
            return QRhiVertexInputAttribute::SShort4;
          default:
            return QRhiVertexInputAttribute::SShort;
        }

      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
      {
        static constexpr QRhiVertexInputAttribute::Format formats[4] = { QRhiVertexInputAttribute::UInt, QRhiVertexInputAttribute::UInt2, QRhiVertexInputAttribute::UInt3, QRhiVertexInputAttribute::UInt4 };
        return formats[components - 1];
      }

      case TINYGLTF_COMPONENT_TYPE_INT:
      {
        static constexpr QRhiVertexInputAttribute::Format formats[4] = { QRhiVertexInputAttribute::SInt, QRhiVertexInputAttribute::SInt2, QRhiVertexInputAttribute::SInt3, QRhiVertexInputAttribute::SInt4 };
        return formats[components - 1];
      }

      default:
        return std::nullopt;
    }
  }

  //! Reads one component out of an accessor's data and returns it as a float
  float readComponent( const unsigned char *element, int index, int componentType, bool normalized )
  {
    const unsigned char *at = element + index * componentSize( componentType );
    switch ( componentType )
    {
      case TINYGLTF_COMPONENT_TYPE_FLOAT:
      {
        float value;
        memcpy( &value, at, sizeof( float ) );
        return value;
      }
      case TINYGLTF_COMPONENT_TYPE_DOUBLE:
      {
        double value;
        memcpy( &value, at, sizeof( double ) );
        return static_cast<float>( value );
      }
      case TINYGLTF_COMPONENT_TYPE_BYTE:
      {
        const qint8 value = static_cast<qint8>( *at );
        // the glTF spec's own dequantisation, which clamps -128 to -1 rather than to -1.008
        return normalized ? std::max( value / 127.0f, -1.0f ) : value;
      }
      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        return normalized ? *at / 255.0f : *at;
      case TINYGLTF_COMPONENT_TYPE_SHORT:
      {
        qint16 value;
        memcpy( &value, at, sizeof( qint16 ) );
        return normalized ? std::max( value / 32767.0f, -1.0f ) : value;
      }
      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
      {
        quint16 value;
        memcpy( &value, at, sizeof( quint16 ) );
        return normalized ? value / 65535.0f : value;
      }
      case TINYGLTF_COMPONENT_TYPE_INT:
      {
        qint32 value;
        memcpy( &value, at, sizeof( qint32 ) );
        return static_cast<float>( value );
      }
      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
      {
        quint32 value;
        memcpy( &value, at, sizeof( quint32 ) );
        return static_cast<float>( value );
      }
    }
    return 0.0f;
  }

  //! The float format of \a components floats
  QRhiVertexInputAttribute::Format floatFormat( int components )
  {
    static constexpr QRhiVertexInputAttribute::Format formats[4] = { QRhiVertexInputAttribute::Float, QRhiVertexInputAttribute::Float2, QRhiVertexInputAttribute::Float3, QRhiVertexInputAttribute::Float4 };
    return formats[components - 1];
  }
} // namespace


QgsGltfGeometry3D::QgsGltfGeometry3D() = default;

QgsGltfGeometry3D::~QgsGltfGeometry3D() = default;

QgsGpuBuffer *QgsGltfGeometry3D::bufferFor( const tinygltf::Model &model, int index )
{
  const auto it = mModelBuffers.find( index );
  if ( it != mModelBuffers.end() )
    return it->second.get();

  const tinygltf::Buffer &buffer = model.buffers[index];
  auto gpuBuffer = std::make_unique<QgsGpuBuffer>( QgsGpuBuffer::Role::Vertex );
  gpuBuffer->setData( QByteArray( reinterpret_cast<const char *>( buffer.data.data() ), static_cast<qsizetype>( buffer.data.size() ) ) );

  QgsGpuBuffer *result = gpuBuffer.get();
  mModelBuffers.emplace( index, std::move( gpuBuffer ) );
  return result;
}

QgsGpuBuffer *QgsGltfGeometry3D::ownBuffer( const QByteArray &data, QgsGpuBuffer::Role role )
{
  auto buffer = std::make_unique<QgsGpuBuffer>( role );
  buffer->setData( data );

  QgsGpuBuffer *result = buffer.get();
  mOwnBuffers.push_back( std::move( buffer ) );
  return result;
}

void QgsGltfGeometry3D::setPositions( const QByteArray &positions, int count )
{
  QgsVertexAttribute attribute;
  attribute.name = u"vertexPosition"_s;
  attribute.buffer = ownBuffer( positions, QgsGpuBuffer::Role::Vertex );
  attribute.format = QRhiVertexInputAttribute::Float3;
  attribute.byteStride = 3 * sizeof( float );
  mGeometry.addAttribute( attribute );
  mGeometry.setVertexCount( count );
}

bool QgsGltfGeometry3D::addAttribute( const QString &name, const tinygltf::Model &model, int accessorIndex, QString *error )
{
  if ( accessorIndex < 0 || accessorIndex >= static_cast<int>( model.accessors.size() ) )
  {
    if ( error )
      *error = u"Accessor %1 is out of range"_s.arg( accessorIndex );
    return false;
  }

  const tinygltf::Accessor &accessor = model.accessors[accessorIndex];
  if ( accessor.bufferView < 0 || accessor.bufferView >= static_cast<int>( model.bufferViews.size() ) )
  {
    if ( error )
      *error = u"Accessor %1 has no buffer view"_s.arg( accessorIndex );
    return false;
  }

  const tinygltf::BufferView &bufferView = model.bufferViews[accessor.bufferView];
  const int components = tinygltf::GetNumComponentsInType( static_cast<uint32_t>( accessor.type ) );
  if ( components < 1 || components > 4 )
  {
    if ( error )
      *error = u"Unsupported accessor type: %1"_s.arg( accessor.type );
    return false;
  }

  const int elementSize = components * componentSize( accessor.componentType );
  const int stride = bufferView.byteStride ? static_cast<int>( bufferView.byteStride ) : elementSize;

  QgsVertexAttribute attribute;
  attribute.name = name;

  if ( const std::optional<QRhiVertexInputAttribute::Format> format = exactFormat( accessor.componentType, accessor.type, accessor.normalized ) )
  {
    // the accessor's layout is one QRhi understands, so the model's own buffer is used
    // as it stands, shared with every other accessor that reads from it
    attribute.buffer = bufferFor( model, bufferView.buffer );
    attribute.format = *format;
    attribute.byteOffset = static_cast<quint32>( bufferView.byteOffset + accessor.byteOffset );
    attribute.byteStride = static_cast<quint32>( stride );
  }
  else
  {
    // QRhi has no vertex format for this, so unpack it to tightly packed floats
    const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];
    const size_t start = bufferView.byteOffset + accessor.byteOffset;
    if ( start + static_cast<size_t>( accessor.count ? ( accessor.count - 1 ) * stride + elementSize : 0 ) > buffer.data.size() )
    {
      if ( error )
        *error = u"Accessor %1 reads past the end of its buffer"_s.arg( accessorIndex );
      return false;
    }

    QByteArray unpacked;
    unpacked.resize( static_cast<qsizetype>( accessor.count ) * components * static_cast<qsizetype>( sizeof( float ) ) );
    float *out = reinterpret_cast<float *>( unpacked.data() );
    const unsigned char *element = buffer.data.data() + start;
    for ( size_t i = 0; i < accessor.count; ++i, element += stride )
    {
      for ( int c = 0; c < components; ++c )
        *out++ = readComponent( element, c, accessor.componentType, accessor.normalized );
    }

    attribute.buffer = ownBuffer( unpacked, QgsGpuBuffer::Role::Vertex );
    attribute.format = floatFormat( components );
    attribute.byteStride = static_cast<quint32>( components * sizeof( float ) );
  }

  mGeometry.addAttribute( attribute );
  return true;
}

bool QgsGltfGeometry3D::setIndices( const tinygltf::Model &model, int accessorIndex, QString *error )
{
  if ( accessorIndex < 0 || accessorIndex >= static_cast<int>( model.accessors.size() ) )
  {
    if ( error )
      *error = u"Accessor %1 is out of range"_s.arg( accessorIndex );
    return false;
  }

  const tinygltf::Accessor &accessor = model.accessors[accessorIndex];
  if ( accessor.bufferView < 0 || accessor.bufferView >= static_cast<int>( model.bufferViews.size() ) )
  {
    if ( error )
      *error = u"Index accessor %1 has no buffer view"_s.arg( accessorIndex );
    return false;
  }

  const tinygltf::BufferView &bufferView = model.bufferViews[accessor.bufferView];
  const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];
  const size_t start = bufferView.byteOffset + accessor.byteOffset;
  const int stride = bufferView.byteStride ? static_cast<int>( bufferView.byteStride ) : componentSize( accessor.componentType );

  switch ( accessor.componentType )
  {
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
      break;

    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
    {
      // QRhi draws 16 or 32 bit indices only, so byte indices are widened
      QByteArray widened;
      widened.resize( static_cast<qsizetype>( accessor.count ) * static_cast<qsizetype>( sizeof( quint16 ) ) );
      quint16 *out = reinterpret_cast<quint16 *>( widened.data() );
      const unsigned char *element = buffer.data.data() + start;
      for ( size_t i = 0; i < accessor.count; ++i, element += stride )
        *out++ = *element;

      mGeometry.setIndexBuffer( ownBuffer( widened, QgsGpuBuffer::Role::Index ), QRhiCommandBuffer::IndexUInt16 );
      mGeometry.setIndexCount( static_cast<int>( accessor.count ) );
      return true;
    }

    default:
      if ( error )
        *error = u"Unsupported index component type: %1"_s.arg( accessor.componentType );
      return false;
  }

  // an index buffer cannot carry a byte offset in QRhi the way a vertex attribute can,
  // so the indices are copied out of the model's buffer rather than sharing it
  const int indexSize = componentSize( accessor.componentType );
  QByteArray indices;
  indices.resize( static_cast<qsizetype>( accessor.count ) * indexSize );
  if ( stride == indexSize )
  {
    memcpy( indices.data(), buffer.data.data() + start, indices.size() );
  }
  else
  {
    char *out = indices.data();
    const unsigned char *element = buffer.data.data() + start;
    for ( size_t i = 0; i < accessor.count; ++i, element += stride, out += indexSize )
      memcpy( out, element, indexSize );
  }

  mGeometry.setIndexBuffer( ownBuffer( indices, QgsGpuBuffer::Role::Index ), accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT ? QRhiCommandBuffer::IndexUInt16 : QRhiCommandBuffer::IndexUInt32 );
  mGeometry.setIndexCount( static_cast<int>( accessor.count ) );
  return true;
}

void QgsGltfGeometry3D::setInstances( const QVector<InstanceTransform> &instances )
{
  constexpr int stride = 10 * sizeof( float ); // vec3 translation + vec4 rotation + vec3 scale

  QByteArray data;
  data.resize( instances.size() * stride );
  float *out = reinterpret_cast<float *>( data.data() );
  for ( const InstanceTransform &instance : instances )
  {
    *out++ = instance.translation.x();
    *out++ = instance.translation.y();
    *out++ = instance.translation.z();

    *out++ = instance.rotation.x();
    *out++ = instance.rotation.y();
    *out++ = instance.rotation.z();
    *out++ = instance.rotation.scalar();

    *out++ = instance.scale.x();
    *out++ = instance.scale.y();
    *out++ = instance.scale.z();
  }

  QgsGpuBuffer *buffer = ownBuffer( data, QgsGpuBuffer::Role::Vertex );

  const auto addInstanceAttribute = [this, buffer]( const QString &name, QRhiVertexInputAttribute::Format format, int offset ) {
    QgsVertexAttribute attribute;
    attribute.name = name;
    attribute.buffer = buffer;
    attribute.format = format;
    attribute.byteOffset = static_cast<quint32>( offset );
    attribute.byteStride = stride;
    attribute.instanceDivisor = 1;
    mGeometry.addAttribute( attribute );
  };

  addInstanceAttribute( u"instanceTranslation"_s, QRhiVertexInputAttribute::Float3, 0 );
  addInstanceAttribute( u"instanceRotation"_s, QRhiVertexInputAttribute::Float4, 3 * sizeof( float ) );
  addInstanceAttribute( u"instanceScale"_s, QRhiVertexInputAttribute::Float3, 7 * sizeof( float ) );

  mInstanceCount = static_cast<int>( instances.size() );
}

void QgsGltfGeometry3D::releaseGpuResources()
{
  for ( auto &buffer : mModelBuffers )
    buffer.second->releaseGpuResources();
  for ( auto &buffer : mOwnBuffers )
    buffer->releaseGpuResources();
}

/// @endcond
