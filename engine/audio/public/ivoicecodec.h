//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Define the IVoiceCodec interface.
//
// $NoKeywords: $
//=============================================================================//

#ifndef IVOICECODEC_H
#define IVOICECODEC_H
#pragma once


#include "tier1/interface.h"


#define BYTES_PER_SAMPLE	2

// Defines voice codec encode / decode quality.
enum class VoiceCodecQuality
{
	// Vocoder (mostly for comfortable noise).
	Noise = 0,
	// Very noticeable artifacts/noise, good intelligibility.
	Lowest = 1,
	// Artifacts/noise sometimes noticeable with / without headphones.
	Average = 2,
	// Need good headphones to tell the difference or hard to tell even with good headphones.
	Good = 3,
	// Completely transparent for voice, good quality music.
	Perfect = 4
};


// This interface is for voice codecs to implement.

// Codecs are guaranteed to be called with the exact output from Compress into Decompress (ie:
// data won't be stuck together and sent to Decompress).

// Decompress is not guaranteed to be called in any specific order relative to Compress, but 
// Codecs maintain state between calls, so it is best to call Compress with consecutive voice data
// and decompress likewise. If you call it out of order, it will sound wierd. 

// In the same vein, calling Decompress twice with the same data is a bad idea since the state will be
// expecting the next block of data, not the same block.

class IVoiceCodec
{
protected:
	virtual			~IVoiceCodec() {}

public:
	// Initialize the object. The uncompressed format is always 8-bit signed mono.
	virtual bool	Init( VoiceCodecQuality quality )=0;

	// Use this to delete the object.
	virtual void	Release()=0;


	// Compress the voice data.
	// in		-	16-bit signed mono voice data.
	// max_out	-	The length of the out buffer. Don't exceed this.
	// bFinal        		-	Set to true on the last call to Compress (the user stopped talking).
	//							Some codecs like big block sizes and will hang onto data you give them in Compress calls.
	//							When you call with bFinal, the codec will give you compressed data no matter what.
	// Return the number of bytes you filled into out.
	virtual int		Compress(const char *in, int nSamples, char *out, int max_out, bool bFinal)=0;

	// Decompress voice data. pUncompressed is 16-bit signed mono.
	virtual int		Decompress(const char *in, int in_size, char *out, int max_out)=0;

	// Some codecs maintain state between Compress and Decompress calls. This should clear that state.
	virtual bool	ResetState()=0;
};


#endif // IVOICECODEC_H
