// Copyright Valve Corporation, All rights reserved.
//
// Purpose: Generic CRC functions

#ifndef VPC_TIER1_CHECKSUM_CRC_H_
#define VPC_TIER1_CHECKSUM_CRC_H_

#include <cstdint>

using CRC32_t = uint32_t;

void CRC32_Init(CRC32_t *pulCRC);
void CRC32_ProcessBuffer(CRC32_t *pulCRC, const void *p, ptrdiff_t len);
void CRC32_Final(CRC32_t *pulCRC);
CRC32_t CRC32_GetTableEntry(unsigned int slot);

inline CRC32_t CRC32_ProcessSingleBuffer(const void *p, ptrdiff_t len) {
  CRC32_t crc;

  CRC32_Init(&crc);
  CRC32_ProcessBuffer(&crc, p, len);
  CRC32_Final(&crc);

  return crc;
}

#endif  // VPC_TIER1_CHECKSUM_CRC_H_
