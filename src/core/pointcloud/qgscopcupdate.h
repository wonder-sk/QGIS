#ifndef QGSCOPCUPDATE_H
#define QGSCOPCUPDATE_H

#include "qgis_core.h"

#include <lazperf/header.hpp>
#include <lazperf/vlr.hpp>

#include "qgspointcloudindex.h"


/**
 * This class takes an existing COPC file and a list of chunks that should be modified,
 * and outputs an updated COPC file where the modified chunks replace the original chunks.
 *
 * \since QGIS 3.42
 */
class CORE_EXPORT QgsCopcUpdate
{
  public:

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

    //! Keeps information how points of a single chunk has been modified
    struct UpdatedChunk
    {
      //! Number of points in the updated chunk
      int32_t pointCount;
      //! Data of the chunk (compressed already with LAZ compressor)
      std::vector<unsigned char> chunkData;
    };

    //! Reads input COPC file and initializes all the members
    void read( QString inputFilename );

    //! Finds hierarchy entry for a particular chunk
    HierarchyEntry findVoxel( QgsPointCloudNodeId k );

    /**
     * Modifies chunk
     * TODO: remove (just updateChunk())
     */
    std::vector<unsigned char> updateChunkValues( int newClassValue, QgsPointCloudNodeId k, QSet<int> pointIndices );

    //! Writes a COPC file with updated chunks
    void write( QString outputFilename, const QHash<QgsPointCloudNodeId, UpdatedChunk> &updatedChunks );

  private:
    HierarchyEntries getHierarchyPage( uint64_t offset, uint64_t size );
    void readHeader();
    void readChunkTable();
    void readHierarchy();

  private:
    QString mInputFilename;
    std::ifstream mFile;
    lazperf::header14 mHeader;
    lazperf::copc_info_vlr mCopcVlr;
    std::vector<lazperf::chunk> mChunks;
    uint32_t mChunkCount;
    uint64_t mHierarchyOffset = 0;
    std::vector<char> mHierarchyBlob;
    std::vector<lazperf::evlr_header> mEvlrHeaders;
    std::vector<std::vector<char>> mEvlrData;
    QHash<uint64_t, QgsPointCloudNodeId> mOffsetToVoxel;

};

#endif // QGSCOPCUPDATE_H
