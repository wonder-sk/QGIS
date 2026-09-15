/***************************************************************************
  qgsrhishader.cpp
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
 ***************************************************************************/

#include "qgsrhishader.h"

#include <rhi/qshaderdescription.h>

#include "qgslogger.h"

#include <QFile>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QString>

using namespace Qt::StringLiterals;

namespace
{
  QMutex sShaderCacheMutex;
  QHash<QString, std::shared_ptr<QgsRhiShader>> sShaderCache;
} // namespace

QShader QgsRhiShader::loadStage( const QString &path )
{
  QFile file( path );
  if ( !file.open( QIODevice::ReadOnly ) )
  {
    QgsDebugError( u"Could not open baked shader %1"_s.arg( path ) );
    return QShader();
  }

  const QShader shader = QShader::fromSerialized( file.readAll() );
  if ( !shader.isValid() )
    QgsDebugError( u"Baked shader %1 is not a valid .qsb file"_s.arg( path ) );

  return shader;
}

std::shared_ptr<QgsRhiShader> QgsRhiShader::load( const QString &vertexPath, const QString &fragmentPath )
{
  const QString cacheKey = vertexPath + '\n' + fragmentPath;

  QMutexLocker locker( &sShaderCacheMutex );
  const auto it = sShaderCache.constFind( cacheKey );
  if ( it != sShaderCache.constEnd() )
    return it.value();

  const QShader vertexShader = loadStage( vertexPath );
  const QShader fragmentShader = loadStage( fragmentPath );
  if ( !vertexShader.isValid() || !fragmentShader.isValid() )
    return nullptr;

  // not make_shared, the constructor is private
  std::shared_ptr<QgsRhiShader> shader( new QgsRhiShader );
  shader->mVertexShader = vertexShader;
  shader->mFragmentShader = fragmentShader;
  shader->mVertexPath = vertexPath;
  shader->mFragmentPath = fragmentPath;

  sShaderCache.insert( cacheKey, shader );
  return shader;
}

void QgsRhiShader::clearCache()
{
  QMutexLocker locker( &sShaderCacheMutex );
  sShaderCache.clear();
}

int QgsRhiShader::inputLocation( const QString &name ) const
{
  const QByteArray target = name.toUtf8();
  const QList<QShaderDescription::InOutVariable> inputs = mVertexShader.description().inputVariables();
  for ( const QShaderDescription::InOutVariable &input : inputs )
  {
    if ( input.name == target )
      return input.location;
  }
  return -1;
}

QVector<QShaderDescription::InOutVariable> QgsRhiShader::samplers() const
{
  QList<QShaderDescription::InOutVariable> samplers = mFragmentShader.description().combinedImageSamplers();
  std::sort( samplers.begin(), samplers.end(), []( const QShaderDescription::InOutVariable &a, const QShaderDescription::InOutVariable &b ) { return a.binding < b.binding; } );
  return QVector<QShaderDescription::InOutVariable>( samplers.cbegin(), samplers.cend() );
}

int QgsRhiShader::uniformBlockSize( int binding ) const
{
  for ( const QShader *stage : { &mVertexShader, &mFragmentShader } )
  {
    const QList<QShaderDescription::UniformBlock> blocks = stage->description().uniformBlocks();
    for ( const QShaderDescription::UniformBlock &block : blocks )
    {
      if ( block.binding == binding )
        return block.size;
    }
  }
  return -1;
}

bool QgsRhiShader::checkUniformBlockLayout( int binding, const QVector<QgsUniformMember> &expected, QString *error ) const
{
  QList<QShaderDescription::BlockVariable> members;
  bool found = false;
  for ( const QShader *stage : { &mVertexShader, &mFragmentShader } )
  {
    const QList<QShaderDescription::UniformBlock> blocks = stage->description().uniformBlocks();
    for ( const QShaderDescription::UniformBlock &block : blocks )
    {
      if ( block.binding != binding )
        continue;
      members = block.members;
      found = true;
      break;
    }
    if ( found )
      break;
  }

  if ( !found )
  {
    if ( error )
      *error = u"no uniform block at binding %1 in %2"_s.arg( binding ).arg( mVertexPath );
    return false;
  }

  for ( const QgsUniformMember &wanted : expected )
  {
    const QByteArray name = wanted.name.toUtf8();
    const auto it = std::find_if( members.cbegin(), members.cend(), [&name]( const QShaderDescription::BlockVariable &member ) { return member.name == name; } );
    if ( it == members.cend() )
    {
      if ( error )
        *error = u"uniform block %1 has no member '%2'"_s.arg( binding ).arg( wanted.name );
      return false;
    }

    if ( it->offset != wanted.offset || it->size != wanted.size )
    {
      if ( error )
      {
        *error = u"uniform '%1' is at offset %2 size %3 in the shader, but offset %4 size %5 in C++"_s.arg( wanted.name ).arg( it->offset ).arg( it->size ).arg( wanted.offset ).arg( wanted.size );
      }
      return false;
    }
  }

  return true;
}
