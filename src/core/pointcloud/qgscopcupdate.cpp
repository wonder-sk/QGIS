
#include <QDebug>

#include "qgscopcupdate.h"

#include "qgslazdecoder.h"

#include <fstream>
#include <iostream>

#include <lazperf/header.hpp>
#include <lazperf/vlr.hpp>
#include <lazperf/Extractor.hpp>
#include <lazperf/filestream.hpp>

#include <lazperf/readers.hpp>
#include <lazperf/writers.hpp>


//! Keeps one entry of COPC hierarchy
struct HierarchyEntry
{
  //! Key of the data to which this entry corresponds
  QgsPointCloudNodeId key;

  /**
   * Absolute offset to the data chunk if the pointCount > 0.
   * Absolute offset to a child hierarchy page if the pointCount is -1.
   * 0 if the pointCount is 0.
   */
  uint64_t offset;

  /**
   * Size of the data chunk in bytes (compressed size) if the pointCount > 0.
   * Size of the hierarchy page if the pointCount is -1.
   * 0 if the pointCount is 0.
   */
  int32_t byteSize;

  /**
   * If > 0, represents the number of points in the data chunk.
   * If -1, indicates the information for this octree node is found in another hierarchy page.
   * If 0, no point data exists for this key, though may exist for child entries.
   */
  int32_t pointCount;
};

typedef QVector<HierarchyEntry> HierarchyEntries;


HierarchyEntries getHierarchyPage( std::ifstream &file, uint64_t offset, uint64_t size )
{
  HierarchyEntries page;
  std::vector<char> buf( 32 );
  int numEntries = size / 32;
  file.seekg( offset );
  while ( numEntries-- )
  {
    file.read( buf.data(), buf.size() );
    lazperf::LeExtractor s( buf.data(), buf.size() );

    HierarchyEntry e;
    int d, x, y, z;
    s >> d >> x >> y >> z;
    s >> e.offset >> e.byteSize >> e.pointCount;
    e.key = QgsPointCloudNodeId( d, x, y, z );

    page.push_back( e );
  }
  return page;
}


bool QgsCopcUpdate::write( QString outputFilename, const QHash<QgsPointCloudNodeId, UpdatedChunk> &updatedChunks )
{

  std::ofstream m_f;
  m_f.open( QgsLazDecoder::toNativePath( outputFilename ), std::ios::out | std::ios::binary );

  // write header and all VLRs all the way to point offset
  // (then we patch what we need)
  mFile.seekg( 0 );
  std::vector<char> allHeaderData;
  allHeaderData.resize( mHeader.point_offset );
  mFile.read( allHeaderData.data(), allHeaderData.size() );
  m_f.write( allHeaderData.data(), allHeaderData.size() );

  m_f.write( "XXXXXXXX", 8 ); // placeholder for chunk table offset

  uint64_t currentChunkOffset = mHeader.point_offset + 8;
  mFile.seekg( currentChunkOffset ); // this is where first chunk starts

  // now, let's write chunks:
  // - iterate through original chunk table, write out chunks
  //   - if chunk is updated, use that instead
  //   - keep updating hierarchy as we go
  //   - keep updating chunk table as we go

  QHash<QgsPointCloudNodeId, uint64_t> voxelToNewOffset;

  int chIndex = 0;
  for ( lazperf::chunk ch : mChunks )
  {
    //qDebug() << "offset " << currentChunkOffset;
    Q_ASSERT( mOffsetToVoxel.contains( currentChunkOffset ) );
    QgsPointCloudNodeId k = mOffsetToVoxel[currentChunkOffset];
    //qDebug() << "      " << chIndex << k.depth << k.x << k.y << k.z << " | " << ch.offset  << ch.count;    // "offset" means size in bytes

    uint64_t newOffset = m_f.tellp();
    voxelToNewOffset[k] = newOffset;

    // check whether the chunk is modified
    if ( updatedChunks.contains( k ) )
    {
      const UpdatedChunk &updatedChunk = updatedChunks[k];

      // use updated one and skip in the original file
      mFile.seekg( ( uint64_t )mFile.tellg() + ch.offset );

      m_f.write( updatedChunk.chunkData.constData(), updatedChunk.chunkData.size() );

      // update sizes
      mChunks[chIndex].offset = updatedChunk.chunkData.size();
    }
    else
    {
      // use as is
      std::vector<char> chunkDataX;
      chunkDataX.resize( ch.offset );
      mFile.read( chunkDataX.data(), chunkDataX.size() );
      m_f.write( chunkDataX.data(), chunkDataX.size() );
    }

    currentChunkOffset += ch.offset;
    ++chIndex;
  }

  // write chunk table: size in bytes + point count of each chunk

  uint64_t newChunkTableOffset = m_f.tellp();

  m_f.write( "\0\0\0\0", 4 ); // chunk table version
  m_f.write( ( char * )&mChunkCount, sizeof( mChunkCount ) );

  lazperf::OutFileStream outStream( m_f );
  lazperf::compress_chunk_table( outStream.cb(), mChunks, true );

  // update hierarchy

  // NOTE: one big assumption we're doing here is that existing hierarchy pages
  // are packed one after another, with no gaps. if that's not the case, things
  // will break apart

  int hierPositionShift = ( uint64_t )m_f.tellp() + 60 - mHierarchyOffset;
  qDebug() << "hierarchy shift:" << hierPositionShift;

  HierarchyEntry *oldCopcHierarchyBlobEntries = ( HierarchyEntry * ) mHierarchyBlob.data();
  int nEntries = mHierarchyBlob.size() / 32;
  for ( int i = 0; i < nEntries; ++i )
  {
    HierarchyEntry &e = oldCopcHierarchyBlobEntries[i];
    QgsPointCloudNodeId k = e.key;
    //std::cout << k.depth << k.x << k.y << k.z << " " << e.offset << std::endl;
    if ( e.pointCount > 0 )
    {
      // update entry to new offset
      Q_ASSERT( voxelToNewOffset.contains( e.key ) );
      e.offset = voxelToNewOffset[e.key];
      //std::cout << "-- updating offset to " << e.offset << std::endl;

      if ( updatedChunks.contains( e.key ) )
      {
        uint64_t newByteSize = updatedChunks[e.key].chunkData.size();
        std::cout << "-- updating byte size from " << e.byteSize << " to " << newByteSize << std::endl;
        e.byteSize = newByteSize;
      }
    }
    else if ( e.pointCount < 0 )
    {
      // move hierarchy pages to new offset
      e.offset += hierPositionShift;
      std::cout << "-- updating offset to page " << k.toString().toStdString();
    }
    else  // pointCount == 0
    {
      // nothing to do - byte size and offset should be zero
    }

  }

  // write hierarchy eVLR

  uint64_t newEvlrOffset = m_f.tellp();

  lazperf::evlr_header outCopcHierEvlr;
  outCopcHierEvlr.reserved = 0;
  outCopcHierEvlr.user_id = "copc";
  outCopcHierEvlr.record_id = 1000;
  outCopcHierEvlr.data_length = mHierarchyBlob.size();
  outCopcHierEvlr.description = "EPT Hierarchy";

  outCopcHierEvlr.write( m_f );
  m_f.write( mHierarchyBlob.data(), mHierarchyBlob.size() );

  // write other eVLRs

  for ( size_t i = 0; i < mEvlrHeaders.size(); ++i )
  {
    lazperf::evlr_header evlrHeader = mEvlrHeaders[i];
    std::vector<char> evlrBody = mEvlrData[i];

    std::cout << "writing evlr " << evlrHeader.user_id << " " << evlrHeader.record_id << std::endl;
    evlrHeader.write( m_f );
    m_f.write( evlrBody.data(), evlrBody.size() );
  }

  // patch header

  qDebug() << "patching new evlr offset";
  m_f.seekp( 235 );
  m_f.write( ( const char * )&newEvlrOffset, 8 );

  uint64_t newRootHierOffset = mCopcVlr.root_hier_offset + hierPositionShift;
  qDebug() << "patching new root hier offset " << newRootHierOffset;
  m_f.seekp( 469 );
  m_f.write( ( const char * )&newRootHierOffset, 8 );

  qDebug() << "patching new chunk table offset " << newChunkTableOffset;
  m_f.seekp( mHeader.point_offset );
  m_f.write( ( const char * )&newChunkTableOffset, 8 );

  return true;
}



bool QgsCopcUpdate::read( QString inputFilename )
{
  mInputFilename = inputFilename;

  mFile.open( QgsLazDecoder::toNativePath( inputFilename ), std::ios::binary | std::ios::in );
  if ( mFile.fail() )
  {
    mErrorMessage = "Could not open file for reading: " + inputFilename;
    return false;
  }

  if ( !readHeader() )
    return false;

  readChunkTable();
  readHierarchy();

  return true;
}


bool QgsCopcUpdate::readHeader()
{
  // read header and COPC VLR
  mHeader = lazperf::header14::create( mFile );
  if ( !mFile )
  {
    mErrorMessage = "Error reading COPC header";
    return false;
  }

  //dumpHeader( mHeader );

  // TODO: various checks: magic header bytes, version, header size, pt format id

  lazperf::vlr_header vh = lazperf::vlr_header::create( mFile );
  mCopcVlr = lazperf::copc_info_vlr::create( mFile );

  //dumpCopcVlr( mCopcVlr );

  int baseCount = lazperf::baseCount( mHeader.point_format_id );
  if ( baseCount == 0 )
  {
    mErrorMessage = QString( "Bad point record format: %1" ).arg( mHeader.point_format_id );
    return false;
  }

  return true;
}


void QgsCopcUpdate::readChunkTable()
{
  uint64_t chunkTableOffset;

  mFile.seekg( mHeader.point_offset );
  mFile.read( ( char * )&chunkTableOffset, sizeof( chunkTableOffset ) );
  mFile.seekg( chunkTableOffset + 4 ); // The first 4 bytes are the version, then the chunk count.
  mFile.read( ( char * )&mChunkCount, sizeof( mChunkCount ) );

  qDebug() << "chunk table:";
  qDebug() << "offset  " << chunkTableOffset;
  qDebug() << "count   " << mChunkCount;

  //
  // read chunk table
  //

  bool variable = true;  // TODO: (m_vlr.chunk_size == lazperf::VariableChunkSize);


  // TODO: not sure why, but after decompress_chunk_table() the input stream seems to be dead, so we create a temporary one
  std::ifstream copcFileTmp;
  copcFileTmp.open( QgsLazDecoder::toNativePath( mInputFilename ), std::ios::binary | std::ios::in );
  copcFileTmp.seekg( mFile.tellg() );
  lazperf::InFileStream copcInFileStream( copcFileTmp );

  mChunks = lazperf::decompress_chunk_table( copcInFileStream.cb(), mChunkCount, variable );
  //qDebug() << "  index   bytes    count";
  //int chIndex = 0;
  std::vector<lazperf::chunk> chunksWithAbsoluteOffsets;
  uint64_t nextChunkOffset = mHeader.point_offset + 8;
  for ( lazperf::chunk ch : mChunks )
  {
    //qDebug() << "      " << chIndex++ << ch.offset  << ch.count;    // "offset" means size in bytes
    chunksWithAbsoluteOffsets.push_back( {nextChunkOffset, ch.count} );
    //qDebug() << "      " << chIndex++ << nextChunkOffset  << ch.count;    // "offset" means absolute offset
    nextChunkOffset += ch.offset;
  }
}


void QgsCopcUpdate::readHierarchy()
{

  qDebug() << "hierarchy";
  qDebug() << "  offset " << mCopcVlr.root_hier_offset;
  qDebug() << "  size   " << mCopcVlr.root_hier_size;

  // get all hierarchy pages

  HierarchyEntries childEntriesToProcess;
  childEntriesToProcess.push_back( HierarchyEntry{ QgsPointCloudNodeId( 0, 0, 0, 0 ), mCopcVlr.root_hier_offset, ( int32_t )mCopcVlr.root_hier_size, -1 } );

  //std::cout << "Chunks:\n";
  //std::cout << "\tKey:     Offest / Count\n";

  while ( !childEntriesToProcess.empty() )
  {
    HierarchyEntry childEntry = childEntriesToProcess.back();
    childEntriesToProcess.pop_back();

    std::cout << "getting page at " << childEntry.offset << " size " << childEntry.byteSize << std::endl;
    HierarchyEntries page = getHierarchyPage( mFile, childEntry.offset, childEntry.byteSize );

    for ( const HierarchyEntry &e : page )
    {
      //std::cout << "\t" << std::string(e.key) << ": " << e.offset << " / " << e.pointCount << "\n";
      if ( e.pointCount > 0 ) // it's a non-empty node
      {
        Q_ASSERT( !mOffsetToVoxel.contains( e.offset ) );
        mOffsetToVoxel[e.offset] = e.key;
      }
      else if ( e.pointCount < 0 ) // referring to a child page
      {
        childEntriesToProcess.push_back( e );
      }
    }
  }

  std::cout << "\n";

  std::cout << "EVLRs: " << mHeader.evlr_count << std::endl;
  std::cout << "  offset " << mHeader.evlr_offset << std::endl;

  lazperf::evlr_header evlr1;
  mFile.seekg( mHeader.evlr_offset );

  mHierarchyOffset = 0;  // where the hierarchy eVLR payload starts

  for ( uint32_t i = 0; i < mHeader.evlr_count; ++i )
  {
    evlr1.read( mFile );
    std::cout << " '" << i + 1 << "' " << evlr1.user_id << " - " << evlr1.description << " | len " << evlr1.data_length << std::endl;
    if ( evlr1.user_id == "copc" && evlr1.record_id == 1000 )
    {
      mHierarchyBlob.resize( evlr1.data_length );
      mHierarchyOffset = mFile.tellg();
      mFile.read( mHierarchyBlob.data(), evlr1.data_length );
    }
    else
    {
      // keep for later
      mEvlrHeaders.push_back( evlr1 );
      std::vector<char> evlrBlob;
      evlrBlob.resize( evlr1.data_length );
      mFile.read( evlrBlob.data(), evlrBlob.size() );
      mEvlrData.push_back( evlrBlob );
    }
  }

  Q_ASSERT( !mHierarchyBlob.empty() );

  std::cout << "should be at the end now: " << mFile.tellg() << std::endl;
}


bool QgsCopcUpdate::writeUpdatedFile( const QString &inputFilename,
                                      const QString &outputFilename,
                                      const QHash<QgsPointCloudNodeId, UpdatedChunk> &updatedChunks,
                                      QString *errorMessage )
{
  QgsCopcUpdate copcUpdate;
  if ( !copcUpdate.read( inputFilename ) )
  {
    if ( errorMessage )
      *errorMessage = copcUpdate.errorMessage();
    return false;
  }

  if ( !copcUpdate.write( outputFilename, updatedChunks ) )
  {
    if ( errorMessage )
      *errorMessage = copcUpdate.errorMessage();
    return false;
  }

  return true;
}
