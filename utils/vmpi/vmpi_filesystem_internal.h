// Copyright Valve Corporation, All rights reserved.
//
//

#ifndef SRC_UTILS_VMPI_VMPI_FILESYSTEM_INTERNAL_H_
#define SRC_UTILS_VMPI_VMPI_FILESYSTEM_INTERNAL_H_

#include "vmpi_filesystem.h"
#include "filesystem.h"
#include "messbuf.h"
#include "iphelpers.h"
#include "vmpi.h"
#include "tier1/utlvector.h"
#include "tier1/utllinkedlist.h"
#include "filesystem_passthru.h"

// Sub packet IDs specific to the VMPI file system.
#define VMPI_FSPACKETID_FILE_REQUEST 1  // Sent by the worker to request a file.
#define VMPI_FSPACKETID_FILE_RESPONSE 2  // Master's response to a file request.
// Sent by workers to tell the master they received a chunk.
#define VMPI_FSPACKETID_CHUNK_RECEIVED 3
// Sent by workers to tell the master they received the whole file.
#define VMPI_FSPACKETID_FILE_RECEIVED 4
#define VMPI_FSPACKETID_MULTICAST_ADDR 5
#define VMPI_FSPACKETID_FILE_CHUNK 6  // Used to send file data when using TCP.

// In TCP mode, we send larger chunks in a sliding window.
#define TCP_CHUNK_QUEUE_LEN 16

#define TCP_CHUNK_PAYLOAD_SIZE (16 * 1024)
#define MULTICAST_CHUNK_PAYLOAD_SIZE (1024 * 1)
#define MAX_CHUNK_PAYLOAD_SIZE \
  MAX(TCP_CHUNK_PAYLOAD_SIZE, MULTICAST_CHUNK_PAYLOAD_SIZE)

struct CMulticastFileInfo {
  unsigned long m_CompressedSize;
  unsigned long m_UncompressedSize;
  int m_FileID;
  unsigned short m_nChunks;
};

class CBaseVMPIFileSystem : public CFileSystemPassThru {
 public:
  virtual ~CBaseVMPIFileSystem();
  virtual void Release();

  virtual void CreateVirtualFile(const char *pFilename, const void *pData,
                                 int fileLength) = 0;
  virtual bool HandleFileSystemPacket(MessageBuffer *pBuf, int iSource,
                                      int iPacketID) = 0;

  virtual void Close(FileHandle_t file);
  virtual int Read(void *pOutput, int size, FileHandle_t file);
  virtual int Write(void const *pInput, int size, FileHandle_t file);
  virtual void Seek(FileHandle_t file, int pos, FileSystemSeek_t seekType);
  virtual unsigned int Tell(FileHandle_t file);
  virtual unsigned int Size(FileHandle_t file);
  virtual unsigned int Size(const char *pFilename, const char *pathID);
  virtual bool FileExists(const char *pFileName, const char *pPathID);
  virtual void Flush(FileHandle_t file);
  virtual bool Precache(const char *pFileName, const char *pPathID);
  virtual bool ReadFile(const char *pFileName, const char *pPath,
                        CUtlBuffer &buf, int nMaxBytes = 0,
                        int nStartingByte = 0, FSAllocFunc_t pfnAlloc = 0);
  virtual bool WriteFile(const char *pFileName, const char *pPath,
                         CUtlBuffer &buf);

  // All the IFileSystem-specific ones pass the calls through.  The worker opens
  // its own filesystem_stdio through.
 protected:
  IpV4 m_MulticastIP;
};

struct IVMPIFile {
  virtual void Close() = 0;
  virtual void Seek(ptrdiff_t pos, FileSystemSeek_t seekType) = 0;
  virtual unsigned int Tell() = 0;
  virtual unsigned int Size() = 0;
  virtual void Flush() = 0;
  virtual ptrdiff_t Read(void *pOutput, ptrdiff_t size) = 0;
  virtual ptrdiff_t Write(void const *pInput, ptrdiff_t size) = 0;
};

// Both the workers and masters use this to hand out the file data.
class CVMPIFile_Memory : public IVMPIFile {
 public:
  CVMPIFile_Memory(const char *pData, ptrdiff_t len, char chMode = 'b');
  virtual void Close();
  virtual void Seek(ptrdiff_t pos, FileSystemSeek_t seekType);
  virtual unsigned int Tell();
  virtual unsigned int Size();
  virtual void Flush();
  virtual ptrdiff_t Read(void *pOutput, ptrdiff_t size);
  virtual ptrdiff_t Write(void const *pInput, ptrdiff_t size);

 private:
  const char *m_pData;
  ptrdiff_t m_DataLen;
  ptrdiff_t m_iCurPos;
  char m_chMode;  // 'b' or 't'
};

// We use different payload sizes if we're using TCP mode vs.
// multicast/broadcast mode.
inline int VMPI_GetChunkPayloadSize() {
  if (VMPI_GetFileSystemMode() == VMPIFileSystemMode::VMPI_FILESYSTEM_TCP)
    return TCP_CHUNK_PAYLOAD_SIZE;

  return MULTICAST_CHUNK_PAYLOAD_SIZE;
}

extern bool g_bDisableFileAccess;
extern CBaseVMPIFileSystem *g_pBaseVMPIFileSystem;

#endif  // SRC_UTILS_VMPI_VMPI_FILESYSTEM_INTERNAL_H_
