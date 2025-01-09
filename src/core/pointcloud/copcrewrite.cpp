
#include <QDebug>

#include "copcrewrite.h"

#include "qgslazdecoder.h"

#include <fstream>
#include <iostream>
#include <deque>

#include <lazperf/header.hpp>
#include <lazperf/vlr.hpp>
#include <lazperf/Extractor.hpp>
#include <lazperf/filestream.hpp>

#include <lazperf/readers.hpp>
#include <lazperf/writers.hpp>


//__attribute__((visibility("default"))) void copc_rewrite();



Entry Entry::create( std::istream &in )
{
  Entry e;
  std::vector<char> buf( Entry::Size );
  in.read( buf.data(), buf.size() );
  lazperf::LeExtractor s( buf.data(), buf.size() );
  s >> e.key.depth >> e.key.x >> e.key.y >> e.key.z;
  s >> e.offset >> e.byteSize >> e.pointCount;
  return e;
}

using Entries = std::deque<Entry>;


// copied from copc verifier
void dumpHeader( const lazperf::header14 &h )
{
  std::cout << "LAS Header:\n";
  std::cout << "\tFile source ID: " << h.file_source_id << "\n";
  std::cout << "\tGlobal encoding: " << h.global_encoding << "\n";
  std::cout << "\t\tTime representation: " <<
            ( ( h.global_encoding & 0x01 ) ? "GPS Satellite Time" : "GPS Week Time" ) << "\n";
  std::cout << "\t\tSRS Type: " <<
            ( ( h.global_encoding & 0x10 ) ? "WKT" : "GeoTIFF" ) << "\n";
  std::cout << "\tVersion: " << ( int )h.version.major << "." << ( int )h.version.minor << "\n";
  std::cout << "\tSystem ID: " << h.system_identifier << "\n";
  std::cout << "\tSoftware ID: " << h.generating_software << "\n";
  std::cout << "\tCreation day/year: " << h.creation.day << " / " << h.creation.year << "\n";
  std::cout << "\tHeader Size: " << h.header_size << "\n";
  std::cout << "\tPoint Offset: " << h.point_offset << "\n";
  std::cout << "\tVLR Count: " << h.vlr_count << "\n";
  std::cout << "\tEVLR Count: " << h.evlr_count << "\n";
  std::cout << "\tEVLR Offset: " << h.evlr_offset << "\n";
  std::cout << "\tPoint Format: " << ( h.point_format_id & 0xF ) << "\n";
  std::cout << "\tPoint Length: " << h.point_record_length << "\n";
  std::cout << "\tNumber of Points old/1.4: " <<
            h.point_count << " / " << h.point_count_14 << "\n";
  std::cout << std::fixed;
  std::cout << "\tScale X Y Z: " << h.scale.x << " " << h.scale.y << " " << h.scale.z << "\n";
  std::cout << "\tOffset X Y Z: " << h.offset.x << " " << h.offset.y << " " << h.offset.z << "\n";
  std::cout << "\tMin X Y Z: " << h.minx << " " << h.miny << " " << h.minz << "\n";
  std::cout << "\tMax X Y Z: " << h.maxx << " " << h.maxy << " " << h.maxz << "\n";
  std::cout << std::defaultfloat;
  std::cout << "\tPoint Counts by Return:     ";
  for ( int i = 0; i < 5; ++i )
    std::cout << h.points_by_return[i] << " ";
  std::cout << "\n";
  std::cout << "\tExt Point Counts by Return: ";
  for ( int i = 0; i < 15; ++i )
    std::cout << h.points_by_return_14[i] << " ";
  std::cout << "\n\n";
}

void dumpCopcVlr( const lazperf::copc_info_vlr &v )
{
  std::cout << "COPC VLR:\n";
  std::cout << "\tCenter X Y Z: " << v.center_x << " " << v.center_y << " " << v.center_z << "\n";
  std::cout << "\tRoot node halfsize: " << v.halfsize << "\n";
  std::cout << "\tRoot node point spacing: " << v.spacing << "\n";
  std::cout << "\tGPS time min/max = " << v.gpstime_minimum << "/" << v.gpstime_maximum << "\n";
  std::cout << "\n";
}

Entries getHierarchyPage( std::ifstream &copcFile, uint64_t offset, uint64_t size )
{
  Entries page;

  int numEntries = size / Entry::Size;
  copcFile.seekg( offset );
  while ( numEntries-- )
  {
    page.push_back( Entry::create( copcFile ) );
    //Entry& e = page.back();
  }
  return page;
}


// do a small rewrite of classification in the chunk so that we have updated data
std::vector<unsigned char> rewrite_chunk( const lazperf::header14 &header, const char *chunkData, int chunkPointCount )
{
  lazperf::reader::chunk_decompressor decompressor( header.pointFormat(), header.ebCount(), chunkData );
  lazperf::writer::chunk_compressor compressor( header.pointFormat(), header.ebCount() );

  std::unique_ptr<char []> decodedData( new char[ header.point_record_length ] );

  qDebug() << "pf" << header.pointFormat();
  Q_ASSERT( header.pointFormat() == 6 || header.pointFormat() == 7 );   // like PF 6 but also with RGB

  for ( int i = 0 ; i < chunkPointCount; ++i )
  {
    decompressor.decompress( decodedData.get() );
    char *buf = decodedData.get();
    //int cls = buf[16];
    //if (cls !=2)
    //  qDebug() << "pt" << cls;

    // rewrite some of them
    if ( i % 3 != 0 )
      buf[16] = 1;

    compressor.compress( decodedData.get() );
  }

  return compressor.done();
}


#if 0
// just modify the first entry of the root page
Entries rootPage = getHierarchyPage( oldCopc.file, oldCopc.copcVlr.root_hier_offset, oldCopc.copcVlr.root_hier_size );
Entry e = *rootPage.begin();

qDebug() << "using chunk " << e.key.depth << "-" << e.key.x << "-" << e.key.y << "-" << e.key.z << " at " << e.offset << " size " << e.byteSize << " points " << e.pointCount;
std::vector<char> chunkData( e.byteSize );
oldCopc.file.seekg( e.offset );
oldCopc.file.read( chunkData.data(), e.byteSize );
updatedChunks[e.key].pointCount = e.pointCount;  // unchanged (for now)
updatedChunks[e.key].chunkData = rewrite_chunk( oldCopc.header, chunkData.data(), e.pointCount );
#endif



void CopcUpdater::write( QString outputFilename, const QHash<VoxelKey, UpdatedChunk> &updatedChunks )
{

  //
  // let's write the output file
  //

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

  QHash<VoxelKey, uint64_t> voxelToNewOffset;

  int chIndex = 0;
  for ( lazperf::chunk ch : mChunks )
  {
    //qDebug() << "offset " << currentChunkOffset;
    Q_ASSERT( mOffsetToVoxel.contains( currentChunkOffset ) );
    VoxelKey k = mOffsetToVoxel[currentChunkOffset];
    //qDebug() << "      " << chIndex << k.depth << k.x << k.y << k.z << " | " << ch.offset  << ch.count;    // "offset" means size in bytes

    uint64_t newOffset = m_f.tellp();
    voxelToNewOffset[k] = newOffset;

    // check whether the chunk is modified
    if ( updatedChunks.contains( k ) )
    {
      const UpdatedChunk &updatedChunk = updatedChunks[k];

      // use updated one and skip in the original file
      mFile.seekg( ( uint64_t )mFile.tellg() + ch.offset );

      m_f.write( ( const char * )updatedChunk.chunkData.data(), updatedChunk.chunkData.size() );

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

  Entry *oldCopcHierarchyBlobEntries = ( Entry * ) mHierarchyBlob.data();
  int nEntries = mHierarchyBlob.size() / 32;
  for ( int i = 0; i < nEntries; ++i )
  {
    Entry &e = oldCopcHierarchyBlobEntries[i];
    VoxelKey k = e.key;
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
      std::cout << "-- updating offset to page " << k.depth << k.x << k.y << k.z;
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

}



void CopcUpdater::read( QString inputFilename )
{

  mFile.open( QgsLazDecoder::toNativePath( inputFilename ), std::ios::binary | std::ios::in );
  if ( mFile.fail() )
  {
    qDebug() << "error opening";
    return;
  }

  //
  // read header and COPC VLR
  //

  mHeader = lazperf::header14::create( mFile );
  if ( !mFile )
  {
    qDebug() << "error reading header";
    return;
  }

  dumpHeader( mHeader );

  // TODO: various checks: magic header bytes, version, header size, pt format id

  lazperf::vlr_header vh = lazperf::vlr_header::create( mFile );
  mCopcVlr = lazperf::copc_info_vlr::create( mFile );

  dumpCopcVlr( mCopcVlr );

  int baseCount = lazperf::baseCount( mHeader.point_format_id );
  if ( baseCount == 0 )
  {
    qDebug() << "Bad point record format '" << mHeader.point_format_id << ".";
    return;
  }

  uint64_t chunkTableOffset;

  mFile.seekg( mHeader.point_offset );
  mFile.read( ( char * )&chunkTableOffset, sizeof( chunkTableOffset ) );
  mFile.seekg( chunkTableOffset + 4 ); // The first 4 bytes are the version, then the chunk count.
  mFile.read( ( char * )&mChunkCount, sizeof( mChunkCount ) );
  if ( mChunkCount > ( std::numeric_limits<int>::max )() )
  {
    std::cout << "Chunk count in chunk table exceeds maximum expected.";
    return;
  }

  qDebug() << "chunk table:";
  qDebug() << "offset  " << chunkTableOffset;
  qDebug() << "count   " << mChunkCount;

  //
  // read chunk table
  //

  bool variable = true;  // TODO: (m_vlr.chunk_size == lazperf::VariableChunkSize);

  // TODO: not sure why, but after decompress_chunk_table() the input stream seems to be dead, so we create a temporary one
  std::ifstream copcFileTmp;
  copcFileTmp.open( QgsLazDecoder::toNativePath( inputFilename ), std::ios::binary | std::ios::in );
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

  //
  // read hierarchy
  //

  qDebug() << "hierarchy";
  qDebug() << "  offset " << mCopcVlr.root_hier_offset;
  qDebug() << "  size   " << mCopcVlr.root_hier_size;

  // get all hierarchy pages

  Entries childEntriesToProcess;
  childEntriesToProcess.push_back( Entry{ VoxelKey(), mCopcVlr.root_hier_offset, ( int32_t )mCopcVlr.root_hier_size, -1 } );

  //std::cout << "Chunks:\n";
  //std::cout << "\tKey:     Offest / Count\n";

  while ( !childEntriesToProcess.empty() )
  {
    Entry childEntry = childEntriesToProcess.back();
    childEntriesToProcess.pop_back();

    std::cout << "getting page at " << childEntry.offset << " size " << childEntry.byteSize << std::endl;
    Entries page = getHierarchyPage( mFile, childEntry.offset, childEntry.byteSize );

    for ( const Entry &e : page )
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


Entry CopcUpdater::findVoxel( VoxelKey k )
{
  // TODO: this is not very efficient

  Entries childEntriesToProcess;
  childEntriesToProcess.push_back( Entry{ VoxelKey(), mCopcVlr.root_hier_offset, ( int32_t )mCopcVlr.root_hier_size, -1 } );

  while ( !childEntriesToProcess.empty() )
  {
    Entry childEntry = childEntriesToProcess.back();
    childEntriesToProcess.pop_back();

    Entries page = getHierarchyPage( mFile, childEntry.offset, childEntry.byteSize );
    for ( const Entry &e : page )
    {
      if ( e.pointCount > 0 ) // it's a non-empty node
      {
        if ( e.key == k )
          return e;
      }
      else if ( e.pointCount < 0 ) // referring to a child page
      {
        childEntriesToProcess.push_back( e );
      }
    }
  }
  Q_ASSERT( false );
  return Entry();
}


std::vector<unsigned char> CopcUpdater::updateChunkValues( int newClassValue, VoxelKey k, QSet<int> pointIndices )
{
  // set new classification value for the given points in voxel and return updated chunk data

  Entry entry = findVoxel( k );

  std::vector<char> chunkData( entry.byteSize );
  mFile.seekg( entry.offset );
  mFile.read( chunkData.data(), entry.byteSize );

  lazperf::reader::chunk_decompressor decompressor( mHeader.pointFormat(), mHeader.ebCount(), chunkData.data() );
  lazperf::writer::chunk_compressor compressor( mHeader.pointFormat(), mHeader.ebCount() );

  std::unique_ptr<char []> decodedData( new char[ mHeader.point_record_length ] );

  Q_ASSERT( mHeader.pointFormat() == 6 || mHeader.pointFormat() == 7 );

  for ( int i = 0 ; i < entry.pointCount; ++i )
  {
    decompressor.decompress( decodedData.get() );
    char *buf = decodedData.get();

    if ( pointIndices.contains( i ) )
    {
      // TODO: support update of any attribute
      buf[16] = ( char )newClassValue;
    }

    compressor.compress( decodedData.get() );
  }

  return compressor.done();
}
