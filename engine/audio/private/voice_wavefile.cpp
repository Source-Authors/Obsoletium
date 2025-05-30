//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "voice_wavefile.h"

#undef fopen
#include <cstdio>

#include "posix_file_stream.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

[[nodiscard]] static se::posix::io_result<uint32_t> ReadDWord(se::posix::posix_file_stream &f) 
{
	uint32_t dword[1];
	auto [num, errc] = f.read(dword);
	return
		se::posix::io_result<uint32_t>{errc || num != 1 ? 0 : dword[0], errc};
}

[[nodiscard]] static se::posix::io_result<uint16_t> ReadWord(se::posix::posix_file_stream &f) 
{
	uint16_t word[1];
	auto [num, errc] = f.read(word);
	return 
		se::posix::io_result<uint16_t>{errc || num != 1
			? static_cast<uint16_t>(0) : word[0], errc};
}

[[nodiscard]] static std::error_code WriteDWord(se::posix::posix_file_stream &f, uint32_t val) 
{
	uint32_t dword[]{ val };
	auto [num, errc] = f.write(dword);
	return errc;
}

[[nodiscard]] static std::error_code WriteWord(se::posix::posix_file_stream &f, uint16_t val) 
{
	uint16_t word[]{ val };
	auto [num, errc] = f.write(word);
	return errc;
}



bool ReadWaveFile(
	const char *pFilename,
	char *&pData,
	int &nDataBytes,
	int &wBitsPerSample,
	int &nChannels,
	int &nSamplesPerSec)
{
	auto [f, errc] =
		se::posix::posix_file_stream_factory::open(pFilename, "rb");
	if (errc) return false; 

	std::tie(std::ignore, errc) = f.seek(22, SEEK_SET);
	if (errc) return false;

	// NbrChannels
	std::tie(nChannels, errc) = ReadWord(f);
	if (errc) return false;

	// Frequency
	std::tie(nSamplesPerSec, errc) = ReadDWord(f);
	if (errc) return false;

	std::tie(std::ignore, errc) = f.seek(34, SEEK_SET);
	if (errc) return false;
	
	// BitsPerSample
	std::tie(wBitsPerSample, errc) = ReadWord(f);
	if (errc) return false;
	
	std::tie(std::ignore, errc) = f.seek(40, SEEK_SET);
	if (errc) return false;

	// DataSize
	std::tie(nDataBytes, errc) = ReadDWord(f);
	if (errc) return false;

	pData = new char[nDataBytes];
	if (!pData)	return false;

	// SampledData
	size_t num;
	std::tie(num, errc) = f.read(pData, nDataBytes, 1, nDataBytes);
	if (errc || num != nDataBytes)
	{
		delete[] pData;
		pData = nullptr;
		nDataBytes = 0;
		return false;
	}

	return true;
}

bool WriteWaveFile(
	const char *pFilename, 
	const char *pData, 
	int nBytes, 
	int wBitsPerSample, 
	int nChannels, 
	int nSamplesPerSec)
{
	auto [f, errc] =
		se::posix::posix_file_stream_factory::open(pFilename, "wb");
	if (errc) return false; 

	// Write the RIFF chunk.
	std::tie(std::ignore, errc) = f.write("RIFF", 4);
	if (errc) return false;

	// FileSize. 0 for now, will set later.
	errc = WriteDWord(f, 0);
	if (errc) return false;

	// FileFormatID
	std::tie(std::ignore, errc) = f.write("WAVE", 4);
	if (errc) return false;
	

	// Write the FORMAT chunk.
	std::tie(std::ignore, errc) = f.write("fmt ", 4);
	if (errc) return false;
	
	// BlocSize
	errc = WriteDWord(f, 0x10);
	if (errc) return false;

	// AudioFormat. 1 = WAVE_FORMAT_PCM
	errc = WriteWord(f, 1);
	if (errc) return false;

	// NbrChannels
	errc = WriteWord(f, nChannels);
	if (errc) return false;

	// Frequency
	errc = WriteDWord(f, nSamplesPerSec);
	if (errc) return false;

	const uint16_t bytesPerBlock = (wBitsPerSample / 8) * nChannels;

	// BytePerSec
	errc = WriteDWord(f, bytesPerBlock * nSamplesPerSec);
	if (errc) return false;

	// BytePerBloc
	errc = WriteWord(f, bytesPerBlock);
	if (errc) return false;

	// BitsPerSample
	errc = WriteWord(f, wBitsPerSample);
	if (errc) return false;

	// Write the DATA chunk.
	std::tie(std::ignore, errc) = f.write("data", 4);
	if (errc) return false;

	// DataSize
	errc = WriteDWord(f, nBytes);
	if (errc) return false;

	// SampledData
	std::tie(std::ignore, errc) = f.write(pData, 1, nBytes);
	if (errc) return false;

	int64_t size;
	// Go back and write the length of the riff file.
	std::tie(size, errc) = f.tell();
	if (errc || size <= 8 || size > std::numeric_limits<uint32_t>::max())
	{
		return false;
	}

	const uint32_t len = static_cast<uint32_t>(size - 8);
	std::tie(std::ignore, errc) = f.seek( 4, SEEK_SET );
	if (errc) return false;

	// FileSize
	errc = WriteDWord(f, len);
	return !errc;
}
