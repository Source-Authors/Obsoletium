//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef QUANTIZE_H
#define QUANTIZE_H

#include <cstring>

#include "tier0/platform.h"

#define MAXDIMS 768
#define MAXQUANT 16000

struct Sample;

struct QuantizedValue {
	double MinError;										// minimum possible error. used
	// for neighbor searches.
	QuantizedValue *Children[2];							// splits
	int32 value;											// only exists for leaf nodes
	Sample *Samples;										// every sample quantized into this
	// entry
	int32 NSamples;											// how many were quantized to this.
	int32 TotSamples;
	double *ErrorMeasure;									// variance measure for each dimension
	double TotalError;										// sum of errors
	uint8 *Mean;											// average value of each dimension
	uint8 *Mins;											// min box for children and this
	uint8 *Maxs;											// max box for children and this
	int NQuant;												// the number of samples which were
															// quantzied to this node since the
															// last time OptimizeQuantizer()
															// was called.
	int *Sums;												// sum used by OptimizeQuantizer
	int sortdim;											// dimension currently sorted along.
};

struct Sample {
	int32 ID;												// identifier of this sample. can
															// be used for any purpose.
	int32 Count;											// number of samples this sample
															// represents
	int32 QNum;												// what value this sample ended up quantized
															// to.
	QuantizedValue *qptr;									// ptr to what this was quantized to.
	uint8 Value[1];											// array of values for multi-dimensional
	// variables.
};

void FreeQuantization(QuantizedValue *t);

QuantizedValue *Quantize(Sample *s, int nsamples, int ndims,
								int nvalues, uint8 *weights, int value0=0);

int CompressSamples(Sample *s, int nsamples, int ndims);

QuantizedValue *FindMatch(uint8 const *sample,
								 int ndims,uint8 *weights,
								 QuantizedValue *QTable);
void PrintSamples(Sample const *s, int nsamples, int ndims);

QuantizedValue *FindQNode(QuantizedValue const *q, int32 code);

inline Sample *NthSample(Sample *s, int i, int nd)
{
	uint8 *r=(uint8 *) s;
	r+=i*(sizeof(*s)+(nd-1));
	return (Sample *) r;
}

ALLOC_CALL inline Sample *AllocSamples(int ns, int nd)
{
	size_t size5=(sizeof(Sample)+(nd-1))*ns;
	void *ret=new uint8[size5];
	memset(ret,0,size5);
	for(int i=0;i<ns;i++)
		NthSample((Sample *)ret,i,nd)->Count=1;
	return (Sample *) ret;
}


// MinimumError: what is the min error which will occur if quantizing
// a sample to the given qnode? This is just the error if the qnode
// is a leaf.
double MinimumError(QuantizedValue const *q, uint8 const *sample,
					int ndims, uint8 const *weights);
double MaximumError(QuantizedValue const *q, uint8 const *sample,
					int ndims, uint8 const *weights);

void PrintQTree(QuantizedValue const *p,int idlevel=0);
void OptimizeQuantizer(QuantizedValue *q, int ndims);

// RecalculateVelues: update the means in a sample tree, based upon
// the samples. can be used to reoptimize when samples are deleted,
// for instance.

void RecalculateValues(QuantizedValue *q, int ndims);

extern double SquaredError;	// may be reset and examined. updated by FindMatch()

// color quantization of 24 bit images
#define QUANTFLAGS_NODITHER 1	// don't do Floyd-steinberg dither

extern void ColorQuantize(
uint8 const	*pImage,			// 4 byte pixels ARGB
int			nWidth,
int			nHeight,
int			nFlags, 			// QUANTFLAGS_xxx
int			nColors,			// # of colors to fill in in palette
uint8		*pOutPixels,		// where to store resulting 8 bit pixels
uint8		*pOutPalette,		// where to store resulting 768-byte palette
int			nFirstColor);		// first color to use in mapping

#endif
