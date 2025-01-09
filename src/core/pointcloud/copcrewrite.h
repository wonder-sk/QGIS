#ifndef COPCREWRITE_H
#define COPCREWRITE_H

#include "qgis_core.h"

#include <lazperf/header.hpp>
#include <lazperf/vlr.hpp>


struct VoxelKey
{
  int depth;
  int x;
  int y;
  int z;

  operator std::string() const
  {
    return std::to_string( depth ) + "-" + std::to_string( x ) + "-" + std::to_string( y ) + "-" +
           std::to_string( z );
  }

  VoxelKey parent() const
  {
    return VoxelKey {depth - 1, x >> 1, y >> 1, z >> 1};
  }

  bool operator==( const VoxelKey &other ) const
  {
    return depth == other.depth && x == other.x && y == other.y && z == other.z;
  }
};

uint qHash( const VoxelKey &k )
{
  return k.depth ^ k.x ^ k.y ^ k.z;
}



class Entry
{
  public:
    VoxelKey key;
    uint64_t offset;
    int32_t byteSize;
    int32_t pointCount;
    static const int Size = 32;

    bool isChildRef() const
    { return pointCount == -1; }

    static Entry create( std::istream &in );
};


class CORE_EXPORT CopcUpdater
{
  public:
    // read input COPC file and initialize all the members
    void read( QString inputFilename );

    Entry findVoxel( VoxelKey k );
    std::vector<unsigned char> updateChunkValues( int newClassValue, VoxelKey k, QSet<int> pointIndices );

    struct UpdatedChunk
    {
      int32_t pointCount;
      std::vector<unsigned char> chunkData;  // compressed already
    };

    // write updates to a new COPC file
    void write( QString outputFilename, const QHash<VoxelKey, UpdatedChunk> &updatedChunks );


    std::ifstream mFile;
    lazperf::header14 mHeader;
    lazperf::copc_info_vlr mCopcVlr;
    std::vector<lazperf::chunk> mChunks;
    uint32_t mChunkCount;
    uint64_t mHierarchyOffset = 0;
    std::vector<char> mHierarchyBlob;
    std::vector<lazperf::evlr_header> mEvlrHeaders;
    std::vector<std::vector<char>> mEvlrData;
    QHash<uint64_t, VoxelKey> mOffsetToVoxel;

};

#endif // COPCREWRITE_H
