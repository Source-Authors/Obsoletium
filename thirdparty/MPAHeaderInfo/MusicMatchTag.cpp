#include "stdafx.h"
#include ".\musicmatchtag.h"

#include <math.h>  // for floor

// information from documentation "musicmatch.txt" in id3lib's doc folder

CMusicMatchTag* CMusicMatchTag::FindTag(CMPAStream* stream, bool, unsigned,
                                        unsigned end) {
  assert(stream);

  // check for footer at the end of file
  unsigned offset{end - 48U};
  const unsigned char* buffer{stream->ReadBytes(32U, offset, true)};

  if (memcmp("Brava Software Inc.             ", buffer, 32U) == 0)
    return new CMusicMatchTag{stream, offset};

  return nullptr;
}

CMusicMatchTag::CMusicMatchTag(CMPAStream* stream, unsigned offset)
    : CTag{stream, _T("MM"), true, offset} {
  // get version info (from footer)
  char version[5];
  memcpy(version, stream->ReadBytes(4U, offset, false, true), 4U);
  version[4] = '\0';

  m_fVersion = strtof(version, nullptr);

  // data offsets in the 20 bytes before footer
  offset -= 52U;
  unsigned img_ext_offset = stream->ReadLEValue(4, offset);
  offset += 4U;
  // unsigned dwImgOffset = stream->ReadLEValue(4, offset);

  // next 4 bytes unused
  offset += 4U;
  unsigned version_offset = stream->ReadLEValue(4, offset);

  offset += 4U;
  //	unsigned dwMetadataOffset = stream->ReadLEValue(4, offset);

  unsigned metadata_size{0};
  const unsigned char* buffer{nullptr};

  if (m_fVersion > 3.00f) {
    // three possible lengths for metadata section: 7936, 8004, and 8132 bytes
    const unsigned poss_sizes[3]{7936U, 8004U, 8132U};

    for (int nIndex = 0; nIndex < 3; nIndex++) {
      // check for sync in version section
      unsigned sync_offset{offset - 20U - poss_sizes[nIndex] - 256U};

      buffer = stream->ReadBytes(8, sync_offset, false, true);

      if (memcmp("18273645", buffer, 8U) == 0) {
        metadata_size = poss_sizes[nIndex];
        break;
      }
    }
  } else
    metadata_size = 7868U;  // fixed size up to version 3

  if (metadata_size == 0)  // could not find correct metadata size
    throw std::exception("Incorrect metadata size (0)");

  // total size = footer + data offsets + metadata + version + img (incl.
  // unused)
  m_dwSize =
      48U + 20U + metadata_size + 256U + (version_offset - img_ext_offset);

  // is header there?
  offset -= (m_dwSize - 48U) + 256U;
  buffer = stream->ReadBytes(8U, offset, false);

  if (memcmp("18273645", buffer, 8U) == 0) {
    m_dwSize += 256U;  // header exists
    m_dwOffset = offset;
  } else
    m_dwOffset = offset + 256U;
}

CMusicMatchTag::~CMusicMatchTag() {}
