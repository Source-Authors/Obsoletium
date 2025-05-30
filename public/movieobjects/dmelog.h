//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef DMELOG_H
#define DMELOG_H
#ifdef _WIN32
#pragma once
#endif

#include "datamodel/dmelement.h"
#include "datamodel/dmattribute.h"
#include "datamodel/dmattributevar.h"
#include "datamodel/dmehandle.h"
#include "interpolatortypes.h"
#include "movieobjects/timeutils.h"
#include "movieobjects/dmetimeselectiontimes.h"

class IUniformRandomStream;

template < class T > class CDmeTypedLog;

enum
{
	FILTER_SMOOTH = 0,
	FILTER_JITTER,
	FILTER_SHARPEN,
	FILTER_SOFTEN,

	NUM_FILTERS
};

enum RecordingMode_t
{
	RECORD_PRESET = 0,  // Preset/fader slider being dragged
	RECORD_ATTRIBUTESLIDER,  // Single attribute slider being dragged
};

#define DMELOG_DEFAULT_THRESHHOLD	0.0001f

class DmeLog_TimeSelection_t
{
public:
	DmeLog_TimeSelection_t() :
		m_nResampleInterval( DmeTime_t( .05f ) ),// 50 msec sampling interval by default
		m_flIntensity( 1.0f ),
		m_flThreshold( DMELOG_DEFAULT_THRESHHOLD ),
		m_pPresetValue( 0 ),
		m_bAttachedMode( true ),
		m_bResampleMode( true ),
		m_bTimeAdvancing( false ),
		m_RecordingMode( RECORD_PRESET )
	{
		m_nTimes[ TS_LEFT_FALLOFF ] = m_nTimes[ TS_LEFT_HOLD ] = 
			m_nTimes[ TS_RIGHT_HOLD ] = m_nTimes[ TS_RIGHT_FALLOFF ] = DmeTime_t( 0 );
		m_nFalloffInterpolatorTypes[ 0 ] = m_nFalloffInterpolatorTypes[ 1 ] = INTERPOLATE_LINEAR_INTERP;
	}

	inline void	ResetTimeAdvancing()
	{
		// Reset the time advancing flag
		m_bTimeAdvancing = false;
	}

	inline void	StartTimeAdvancing()
	{
		m_bTimeAdvancing = true;
	}

	inline bool	IsTimeAdvancing() const
	{
		return m_bTimeAdvancing;
	}

	inline RecordingMode_t GetRecordingMode() const
	{
		return m_RecordingMode;
	}

	void SetRecordingMode( RecordingMode_t mode )
	{
		m_RecordingMode = mode;
	}

	float			GetAmountForTime( DmeTime_t curtime ) const;
	float			AdjustFactorForInterpolatorType( float factor, int side ) const;

	// NOTE: See DmeTimeSelectionTimes_t for return values, 0 means before, 1= left fallof, 2=hold, 3=right falloff, 4=after
	int				ComputeRegionForTime( DmeTime_t curtime ) const;

	DmeTime_t		m_nTimes[ TS_TIME_COUNT ];
	int				m_nFalloffInterpolatorTypes[ 2 ];
	DmeTime_t		m_nResampleInterval;	// Only used if m_bResampleMode is true
	float			m_flIntensity; // How much to drive values toward m_HeadValue (generally 1.0f)
	float			m_flThreshold;
	CDmAttribute*	m_pPresetValue;

	bool			m_bAttachedMode : 1; // Is the current time "attached" to the head position

	// Adds new, evenly spaced samples based on m_nResampleInterval
	// Also adds zero intensity samples at the falloff edges
	bool			m_bResampleMode : 1; 

private:
	bool			m_bTimeAdvancing : 1; // Has time ever been advancing
	RecordingMode_t	m_RecordingMode;
};

class CDmeChannel;
class CDmeChannelsClip;
class CDmeFilmClip;
class CDmeLog;
class CDmeLogLayer;

struct LayerSelectionData_t
{
	LayerSelectionData_t();
	void Release();

	CDmeHandle< CDmeChannel >		m_hChannel;
	CDmeHandle< CDmeChannelsClip >	m_hOwner;
	CDmeHandle< CDmeFilmClip >		m_hShot;
	CDmeHandle< CDmeLog >			m_hLog;
	DmAttributeType_t				m_DataType;
	int								m_nDuration;
	int								m_nHoldTimes[ 2 ];
	DmeTime_t						m_tStartOffset;

	// This is dynamic and needs to be released
	struct DataLayer_t
	{
		DataLayer_t( float frac, CDmeLogLayer *layer );
		
		float m_flStartFraction;
		CDmeHandle< CDmeLogLayer, true >		m_hData;
	};

	CUtlVector< DataLayer_t >		m_vecData;
};

//-----------------------------------------------------------------------------
// CDmeLogLayer - abstract base class
//-----------------------------------------------------------------------------
abstract_class CDmeLogLayer : public CDmElement
{
	friend class CDmeLog;

	DEFINE_ELEMENT( CDmeLogLayer, CDmElement );

public:
	virtual void CopyLayer( const CDmeLogLayer *src ) = 0;
	virtual void CopyPartialLayer( const CDmeLogLayer *src, DmeTime_t startTime, DmeTime_t endTime, bool bRebaseTimestamps ) = 0;
	virtual void ExplodeLayer( const CDmeLogLayer *src, DmeTime_t startTime, DmeTime_t endTime, bool bRebaseTimestamps, DmeTime_t tResampleInterval ) = 0;
	virtual void InsertKeyFromLayer( DmeTime_t keyTime, const CDmeLogLayer *src, DmeTime_t srcKeyTime ) = 0;

	DmeTime_t GetBeginTime() const;
	DmeTime_t GetEndTime() const;
	intp GetKeyCount() const;

	// Returns the index of a key closest to this time, within tolerance
	// NOTE: Insertion or removal may change this index!
	// Returns -1 if the time isn't within tolerance.
	intp FindKeyWithinTolerance( DmeTime_t time, DmeTime_t nTolerance );
	
	// Returns the type of attribute being logged
	virtual DmAttributeType_t GetDataType() const = 0;

	// Sets a key, removes all keys after this time
	virtual void SetKey( DmeTime_t time, const CDmAttribute *pAttr, uint index = 0, int curveType = CURVE_DEFAULT ) = 0;
	virtual bool SetDuplicateKeyAtTime( DmeTime_t time ) = 0;
	// This inserts a key using the current values to construct the proper value for the time
	virtual intp InsertKeyAtTime( DmeTime_t nTime, int curveType = CURVE_DEFAULT ) = 0;

	// Sets the interpolated value of the log at the specified time into the attribute
	virtual void GetValue( DmeTime_t time, CDmAttribute *pAttr, uint index = 0 ) const = 0;

	virtual float GetComponent( DmeTime_t time, int componentIndex ) const = 0;

	// Returns the time at which a particular key occurs
	DmeTime_t GetKeyTime( intp nKeyIndex ) const;
	void	SetKeyTime( intp nKeyIndex, DmeTime_t keyTime );

	// Scale + bias key times
	void ScaleBiasKeyTimes( double flScale, DmeTime_t nBias );

	// Removes a single key	by index
	virtual void RemoveKey( intp nKeyIndex, intp nNumKeysToRemove = 1 ) = 0;

	// Removes all keys
	virtual void ClearKeys() = 0;

	virtual bool IsConstantValued() const = 0;
	virtual void RemoveRedundantKeys() = 0;
	virtual void RemoveRedundantKeys( float threshold ) = 0;

	// resampling and filtering
	virtual void Resample( DmeFramerate_t samplerate ) = 0;
	virtual void Filter( intp nSampleRadius ) = 0;
	virtual void Filter2( DmeTime_t sampleRadius ) = 0;

	virtual void SetOwnerLog( CDmeLog *owner ) = 0;
	      CDmeLog *GetOwnerLog();
	const CDmeLog *GetOwnerLog() const;

	bool			IsUsingCurveTypes() const;
	int				GetDefaultCurveType() const;

	// Override curvetype for specific key
	void			SetKeyCurveType( intp nKeyIndex, int curveType );
	int				GetKeyCurveType( intp nKeyIndex ) const;

	// Validates that all keys are correctly sorted in time
	bool			ValidateKeys() const;

	// Removes all keys outside the specified time range
	void			RemoveKeysOutsideRange( DmeTime_t tStart, DmeTime_t tEnd );

protected:
	intp FindKey( DmeTime_t time ) const;

	void OnUsingCurveTypesChanged();

	CDmeLog *m_pOwnerLog;

	mutable intp m_lastKey;
	CDmaArray< int > m_times;
	CDmaArray< int > m_CurveTypes;
};

template< class T >
CDmeLogLayer *CreateLayer( CDmeTypedLog< T > *ownerLog );


//-----------------------------------------------------------------------------
// CDmeLogLayer - abstract base class
//-----------------------------------------------------------------------------
abstract_class CDmeCurveInfo : public CDmElement
{
	DEFINE_ELEMENT( CDmeCurveInfo, CDmElement );

public:
	// Global override for all keys unless overriden by specific key
	void			SetDefaultCurveType( int curveType );
	int				GetDefaultCurveType() const;

	void	SetMinValue( float val );
	float	GetMinValue() const;
	void	SetMaxValue( float val );
	float	GetMaxValue() const;

protected:
	CDmaVar< int >		m_DefaultCurveType;

	CDmaVar< float >	m_MinValue;
	CDmaVar< float >	m_MaxValue;
};

template <class T > class CDmeTypedLogLayer;

//-----------------------------------------------------------------------------
// CDmeLog - abstract base class
//-----------------------------------------------------------------------------
abstract_class CDmeLog : public CDmElement
{
	DEFINE_ELEMENT( CDmeLog, CDmElement );

public:
	intp FindLayerForTime( DmeTime_t time ) const;
	intp FindLayerForTimeSkippingTopmost( DmeTime_t time ) const;
	void FindLayersForTime( DmeTime_t time, CUtlVector< intp >& list ) const;

	virtual void		FinishTimeSelection( DmeTime_t tHeadPosition, DmeLog_TimeSelection_t& params ) = 0; // in attached, timeadvancing mode, we need to blend out of the final sample over the fadeout interval
	virtual void		StampKeyAtHead( DmeTime_t tHeadPosition, DmeTime_t tPreviousHeadPosition, const DmeLog_TimeSelection_t& params, const CDmAttribute *pAttr, uint index = 0 ) = 0;
	virtual void		FilterUsingTimeSelection( IUniformRandomStream *random, float flScale, const DmeLog_TimeSelection_t& params, int filterType, bool bResample, bool bApplyFalloff, const CDmeLogLayer *baseLayer, CDmeLogLayer *writeLayer  ) = 0;
	virtual void		FilterUsingTimeSelection( IUniformRandomStream *random, const DmeLog_TimeSelection_t& params, int filterType, bool bResample, bool bApplyFalloff  ) = 0;
	virtual void		StaggerUsingTimeSelection( const DmeLog_TimeSelection_t& params, DmeTime_t tStaggerAmount, const CDmeLogLayer *baseLayer, CDmeLogLayer *writeLayer  ) = 0;
	virtual void		RevealUsingTimeSelection( const DmeLog_TimeSelection_t &params, CDmeLogLayer *savedLayer ) = 0;
	virtual void		BlendLayersUsingTimeSelection( const DmeLog_TimeSelection_t &params ) = 0;
	virtual void		BlendLayersUsingTimeSelection( const CDmeLogLayer *firstLayer, const CDmeLogLayer *secondLayer, CDmeLogLayer *outputLayer, const DmeLog_TimeSelection_t &params, bool bUseBaseLayerSamples, DmeTime_t tStartOffset ) = 0;
	virtual void		BlendTimesUsingTimeSelection( const CDmeLogLayer *firstLayer, const CDmeLogLayer *secondLayer, CDmeLogLayer *outputLayer, const DmeLog_TimeSelection_t &params, DmeTime_t tStartOffset ) = 0;
	virtual void		PasteAndRescaleSamples( const CDmeLogLayer *src, const DmeLog_TimeSelection_t& srcParams, const DmeLog_TimeSelection_t& destParams, bool bBlendAreaInFalloffRegion ) = 0;
	virtual void		PasteAndRescaleSamples( const CDmeLogLayer *pBaseLayer, const CDmeLogLayer *pDataLayer, CDmeLogLayer *pOutputLayer, const DmeLog_TimeSelection_t& srcParams, const DmeLog_TimeSelection_t& destParams, bool bBlendAreaInFalloffRegion ) = 0;
	virtual void		BuildNormalizedLayer( CDmeTypedLogLayer< float > *target ) = 0;
	virtual void		BuildCorrespondingLayer( const CDmeLogLayer *pReferenceLayer, const CDmeLogLayer *pDataLayer, CDmeLogLayer *pOutputLayer ) = 0;

	intp GetTopmostLayer() const;
	intp GetNumLayers() const;
	CDmeLogLayer *GetLayer( intp index );
	const CDmeLogLayer *GetLayer( intp index ) const;

	DmeTime_t GetBeginTime() const;
	DmeTime_t GetEndTime() const;
	intp GetKeyCount() const;

	bool	IsEmpty() const;

	// Returns the index of a key closest to this time, within tolerance
	// NOTE: Insertion or removal may change this index!
	// Returns -1 if the time isn't within tolerance.
	virtual intp FindKeyWithinTolerance( DmeTime_t time, DmeTime_t nTolerance ) = 0;
	
	// Returns the type of attribute being logged
	virtual DmAttributeType_t GetDataType() const = 0;

	// Sets a key, removes all keys after this time
	virtual void SetKey( DmeTime_t time, const CDmAttribute *pAttr, uint index = 0, int curveType = CURVE_DEFAULT ) = 0;
	virtual bool SetDuplicateKeyAtTime( DmeTime_t time ) = 0;
	virtual int InsertKeyAtTime( DmeTime_t nTime, int curveType = CURVE_DEFAULT ) = 0;
	// Sets the interpolated value of the log at the specified time into the attribute
	virtual void GetValue( DmeTime_t time, CDmAttribute *pAttr, uint index = 0 ) const = 0;
	virtual void GetValueSkippingTopmostLayer( DmeTime_t time, CDmAttribute *pAttr, uint index = 0 ) const = 0;

	virtual float GetComponent( DmeTime_t time, int componentIndex ) const = 0;

	// Returns the time at which a particular key occurs
	virtual DmeTime_t GetKeyTime( intp nKeyIndex ) const = 0;
	virtual void	SetKeyTime( intp nKeyIndex, DmeTime_t keyTime ) = 0;

	// Override curvetype for specific key
	void			SetKeyCurveType( intp nKeyIndex, int curveType );
	int				GetKeyCurveType( intp nKeyIndex ) const;

	// Removes a single key	by index
	virtual void RemoveKey( intp nKeyIndex, intp nNumKeysToRemove = 1 ) = 0;

	// Removes all keys within the time range, returns true if keys were removed
	bool RemoveKeys( DmeTime_t tStartTime, DmeTime_t tEndTime );

	// Removes all keys
	virtual void ClearKeys() = 0;

	// Scale + bias key times
	void ScaleBiasKeyTimes( double flScale, DmeTime_t nBias );

	virtual float GetValueThreshold() const = 0;
	virtual void SetValueThreshold( float thresh ) = 0;
	virtual bool IsConstantValued() const = 0;
	virtual void RemoveRedundantKeys() = 0;
	virtual void RemoveRedundantKeys( float threshold ) = 0;

	// resampling and filtering
	virtual void Resample( DmeFramerate_t samplerate ) = 0;
	virtual void Filter( int nSampleRadius ) = 0;
	virtual void Filter2( DmeTime_t sampleRadius ) = 0;

	// Creates a log of a requested type
	static CDmeLog *CreateLog( DmAttributeType_t type, DmFileId_t fileid );

	virtual CDmeLogLayer *AddNewLayer() = 0;
	enum
	{
		FLATTEN_NODISCONTINUITY_FIXUP = (1<<0), // Don't add "helper" samples to preserve discontinuities.  This occurs when the time selection is "detached" from the head position
		FLATTEN_SPEW				  = (1<<1),
	};
	virtual void		FlattenLayers( float threshold, int flags ) = 0;

	// Only used by undo system!!!
	virtual void		AddLayerToTail( CDmeLogLayer *layer ) = 0;
	virtual CDmeLogLayer *RemoveLayerFromTail() = 0;
	virtual CDmeLogLayer *RemoveLayer( intp iLayer ) = 0;


	// Resolve
	void Resolve() override;

	// curve info helpers
	bool IsUsingCurveTypes() const;
	const CDmeCurveInfo *GetCurveInfo() const;
	      CDmeCurveInfo *GetCurveInfo();
	virtual CDmeCurveInfo *GetOrCreateCurveInfo() = 0;
	virtual void SetCurveInfo( CDmeCurveInfo *pCurveInfo ) = 0;

	// accessors for CurveInfo data
	int GetDefaultCurveType() const;

	// FIXME - this should really be in the CurveInfo
	//         but the animset editor currently asks for these, without having set a curveinfo...
	void			SetMinValue( float val );
	void			SetMaxValue( float val );
	float			GetMinValue() const;
	float			GetMaxValue() const;

	virtual bool			HasDefaultValue() const = 0;


protected:
//	int FindKey( DmeTime_t time ) const;

	void	OnUsingCurveTypesChanged();

	void OnAttributeChanged( CDmAttribute *pAttribute ) override;

	CDmaElementArray< CDmeLogLayer >	m_Layers;
	CDmaElement< CDmeCurveInfo > m_CurveInfo;
};



//-----------------------------------------------------------------------------
// CDmeTypedCurveInfo - implementation class for all logs
//-----------------------------------------------------------------------------
template< class T >
class CDmeTypedCurveInfo : public CDmeCurveInfo
{
	DEFINE_ELEMENT( CDmeTypedCurveInfo, CDmeCurveInfo );

public:
	// For "faceposer" style left/right edges, this controls whether interpolators try to mimic faceposer left/right edge behavior
	void			SetUseEdgeInfo( bool state );
	bool			IsUsingEdgeInfo() const;

	void			SetEdgeInfo( int edge, bool active, const T& val, int curveType );
	void			GetEdgeInfo( int edge, bool& active, T& val, int& curveType ) const;

	void			SetDefaultEdgeZeroValue( const T& val );
	const T&		GetDefaultEdgeZeroValue() const;

	void			SetRightEdgeTime( DmeTime_t time );
	DmeTime_t		GetRightEdgeTime() const;

	bool			IsEdgeActive( int edge ) const;
	void			GetEdgeValue( int edge, T& value ) const;

	int				GetEdgeCurveType( int edge ) const;
	void			GetZeroValue( int side, T& val ) const;

protected:
	CDmaVar< bool >			m_bUseEdgeInfo;
	// Array of 2 for left/right edges...
	CDmaVar< bool >			m_bEdgeActive[ 2 ];
	CDmaVar< T >			m_EdgeValue[ 2 ];
	CDmaVar< int >			m_EdgeCurveType[ 2 ];
	CDmaVar< int >			m_RightEdgeTime;
	CDmaVar< T >			m_DefaultEdgeValue;
};


// forward declaration
template< class T > class CDmeTypedLog;


//-----------------------------------------------------------------------------
// CDmeTypedLogLayer - implementation class for all logs
//-----------------------------------------------------------------------------
template< class T >
class CDmeTypedLogLayer : public CDmeLogLayer
{
	DEFINE_ELEMENT( CDmeTypedLogLayer, CDmeLogLayer );

public:
	void CopyLayer( const CDmeLogLayer *src ) override;
	void CopyPartialLayer( const CDmeLogLayer *src, DmeTime_t startTime, DmeTime_t endTime, bool bRebaseTimestamps ) override;
	void ExplodeLayer( const CDmeLogLayer *src, DmeTime_t startTime, DmeTime_t endTime, bool bRebaseTimestamps, DmeTime_t tResampleInterval ) override;
	void InsertKeyFromLayer( DmeTime_t keyTime, const CDmeLogLayer *src, DmeTime_t srcKeyTime ) override;

	// Finds a key within tolerance, or adds one. Unlike SetKey, this will *not* delete keys after the specified time
	intp FindOrAddKey( DmeTime_t nTime, DmeTime_t nTolerance, const T& value, int curveType = CURVE_DEFAULT );

	// Sets a key, removes all keys after this time
	void SetKey( DmeTime_t time, const T& value, int curveType = CURVE_DEFAULT );
	// This inserts a key using the current values to construct the proper value for the time
	intp InsertKeyAtTime( DmeTime_t nTime, int curveType = CURVE_DEFAULT ) override;

	void SetKeyValue( intp nKey, const T& value );

	const T& GetValue( DmeTime_t time ) const;

	const T& GetKeyValue( intp nKeyIndex ) const;
	const T& GetValueSkippingKey( intp nKeyToSkip ) const;

	// This inserts a key. Unlike SetKey, this will *not* delete keys after the specified time
	intp InsertKey( DmeTime_t nTime, const T& value, int curveType = CURVE_DEFAULT );

	// inherited from CDmeLog
	void ClearKeys() override;
	void SetKey( DmeTime_t time, const CDmAttribute *pAttr, uint index = 0, int curveType = CURVE_DEFAULT ) override;
	bool SetDuplicateKeyAtTime( DmeTime_t time ) override;
	void GetValue( DmeTime_t time, CDmAttribute *pAttr, uint index = 0 ) const override;
	float GetComponent( DmeTime_t time, int componentIndex ) const override;
	DmAttributeType_t GetDataType() const override;
	bool IsConstantValued() const override;
	void RemoveRedundantKeys() override;
	void RemoveRedundantKeys( float threshold ) override;

	void RemoveKey( intp nKeyIndex, intp nNumKeysToRemove = 1 ) override;
	void Resample( DmeFramerate_t samplerate ) override;
	void Filter( intp nSampleRadius ) override;
	void Filter2( DmeTime_t sampleRadius ) override;

	void RemoveKeys( DmeTime_t starttime );

	// curve info helpers
	const CDmeTypedCurveInfo< T > *GetTypedCurveInfo() const;
	      CDmeTypedCurveInfo< T > *GetTypedCurveInfo();

	bool			IsUsingEdgeInfo() const;
	void			GetEdgeInfo( int edge, bool& active, T& val, int& curveType ) const;
	const T&		GetDefaultEdgeZeroValue() const;
	DmeTime_t		GetRightEdgeTime() const;

	void			SetOwnerLog( CDmeLog *owner ) override;

	      CDmeTypedLog< T >	*GetTypedOwnerLog();
	const CDmeTypedLog< T >	*GetTypedOwnerLog() const;

protected:

	int GetEdgeCurveType( int edge ) const;
	void GetZeroValue( int side, T& val ) const;

	void GetValueUsingCurveInfo( DmeTime_t time, T& out ) const;
	void GetValueUsingCurveInfoSkippingKey( intp nKeyToSkip, T& out ) const;
	void GetBoundedSample( intp keyindex, DmeTime_t& time, T& val, int& curveType ) const;

	void CurveSimplify_R( float thresholdSqr, intp startPoint, intp endPoint, CDmeTypedLogLayer< T > *output );

	friend CDmeTypedLog< T >;

protected:
	CDmaArray< T > m_values;
};


//-----------------------------------------------------------------------------
// CDmeTypedLog - implementation class for all logs
//-----------------------------------------------------------------------------
template< class T >
class CDmeTypedLog : public CDmeLog
{
	DEFINE_ELEMENT( CDmeTypedLog, CDmeLog );

public:

	void OnAttributeArrayElementAdded( CDmAttribute *pAttribute, intp nFirstElem, intp nLastElem ) override;

	CDmeTypedLogLayer< T > *GetLayer( intp index );
	const CDmeTypedLogLayer< T > *GetLayer( intp index ) const;

	void		StampKeyAtHead( DmeTime_t tHeadPosition, DmeTime_t tPreviousHeadPosition, const DmeLog_TimeSelection_t& params, const T& value );
	void		StampKeyAtHead( DmeTime_t tHeadPosition, DmeTime_t tPreviousHeadPosition, const DmeLog_TimeSelection_t& params, const CDmAttribute *pAttr, uint index = 0 ) override;
	void		FinishTimeSelection( DmeTime_t tHeadPosition, DmeLog_TimeSelection_t& params ) override; // in attached, timeadvancing mode, we need to blend out of the final sample over the fadeout interval
	void		FilterUsingTimeSelection( IUniformRandomStream *random, float flScale, const DmeLog_TimeSelection_t& params, int filterType, bool bResample, bool bApplyFalloff, const CDmeLogLayer *baseLayer, CDmeLogLayer *writeLayer ) override;
	void		FilterUsingTimeSelection( IUniformRandomStream *random, const DmeLog_TimeSelection_t& params, int filterType, bool bResample, bool bApplyFalloff ) override;
	void		StaggerUsingTimeSelection( const DmeLog_TimeSelection_t& params, DmeTime_t tStaggerAmount, const CDmeLogLayer *baseLayer, CDmeLogLayer *writeLayer ) override;
	void		RevealUsingTimeSelection( const DmeLog_TimeSelection_t &params, CDmeLogLayer *savedLayer ) override;
	void		BlendLayersUsingTimeSelection( const DmeLog_TimeSelection_t &params ) override;
	void		BlendLayersUsingTimeSelection( const CDmeLogLayer *firstLayer, const CDmeLogLayer *secondLayer, CDmeLogLayer *outputLayer, const DmeLog_TimeSelection_t &params, bool bUseBaseLayerSamples, DmeTime_t tStartOffset ) override;
	void		BlendTimesUsingTimeSelection( const CDmeLogLayer *firstLayer, const CDmeLogLayer *secondLayer, CDmeLogLayer *outputLayer, const DmeLog_TimeSelection_t &params, DmeTime_t tStartOffset ) override;
	
	void		PasteAndRescaleSamples( const CDmeLogLayer *src, const DmeLog_TimeSelection_t& srcParams, const DmeLog_TimeSelection_t& destParams, bool bBlendAreaInFalloffRegion ) override;
	void		PasteAndRescaleSamples( const CDmeLogLayer *pBaseLayer, const CDmeLogLayer *pDataLayer, CDmeLogLayer *pOutputLayer, const DmeLog_TimeSelection_t& srcParams, const DmeLog_TimeSelection_t& destParams, bool bBlendAreaInFalloffRegion ) override;
	void		BuildCorrespondingLayer( const CDmeLogLayer *pReferenceLayer, const CDmeLogLayer *pDataLayer, CDmeLogLayer *pOutputLayer ) override;

	void		BuildNormalizedLayer( CDmeTypedLogLayer< float > *target ) override;

	// Finds a key within tolerance, or adds one. Unlike SetKey, this will *not* delete keys after the specified time
	intp FindOrAddKey( DmeTime_t nTime, DmeTime_t nTolerance, const T& value, int curveType = CURVE_DEFAULT );

	// Sets a key, removes all keys after this time
	void SetKey( DmeTime_t time, const T& value, int curveType = CURVE_DEFAULT );
	int InsertKeyAtTime( DmeTime_t nTime, int curveType = CURVE_DEFAULT ) override;
	bool ValuesDiffer( const T& a, const T& b ) const;
	const T& GetValue( DmeTime_t time ) const;
	const T& GetValueSkippingTopmostLayer( DmeTime_t time ) const;

	const T& GetKeyValue( intp nKeyIndex ) const;

	// This inserts a key. Unlike SetKey, this will *not* delete keys after the specified time
	int InsertKey( DmeTime_t nTime, const T& value, int curveType = CURVE_DEFAULT );

	// inherited from CDmeLog
	void ClearKeys() override;
	void SetKey( DmeTime_t time, const CDmAttribute *pAttr, uint index = 0, int curveType = CURVE_DEFAULT ) override;
	bool SetDuplicateKeyAtTime( DmeTime_t time ) override;
	void GetValue( DmeTime_t time, CDmAttribute *pAttr, uint index = 0 ) const override;
	void GetValueSkippingTopmostLayer( DmeTime_t time, CDmAttribute *pAttr, uint index = 0 ) const override;
	float GetComponent( DmeTime_t time, int componentIndex ) const override;
	DmAttributeType_t GetDataType() const override;
	float GetValueThreshold() const override { return m_threshold; }
	void SetValueThreshold( float thresh ) override;
	bool IsConstantValued() const override;
	void RemoveRedundantKeys() override;
	void RemoveRedundantKeys( float threshold ) override;
	void RemoveKey( intp nKeyIndex, intp nNumKeysToRemove = 1 ) override;
	void Resample( DmeFramerate_t samplerate ) override;
	void Filter( int nSampleRadius ) override;
	void Filter2( DmeTime_t sampleRadius ) override;

	intp FindKeyWithinTolerance( DmeTime_t time, DmeTime_t nTolerance ) override;
	DmeTime_t GetKeyTime( intp nKeyIndex ) const override;
	void	SetKeyTime( intp nKeyIndex, DmeTime_t keyTime ) override;

	CDmeLogLayer *AddNewLayer() override;
	void		FlattenLayers( float threshhold, int flags ) override;

	// Only used by undo system!!!
	void		AddLayerToTail( CDmeLogLayer *layer ) override;
	CDmeLogLayer *RemoveLayerFromTail() override;
	CDmeLogLayer *RemoveLayer( intp iLayer ) override;

	// curve info helpers
	const CDmeTypedCurveInfo< T > *GetTypedCurveInfo() const;
	      CDmeTypedCurveInfo< T > *GetTypedCurveInfo();
	CDmeCurveInfo *GetOrCreateCurveInfo() override;
	void SetCurveInfo( CDmeCurveInfo *pCurveInfo ) override;

	// For "faceposer" style left/right edges, this controls whether interpolators try to mimic faceposer left/right edge behavior
	void			SetUseEdgeInfo( bool state );
	bool			IsUsingEdgeInfo() const;

	void			SetEdgeInfo( int edge, bool active, const T& val, int curveType );
	void			GetEdgeInfo( int edge, bool& active, T& val, int& curveType ) const;

	void			SetDefaultEdgeZeroValue( const T& val );
	const T&		GetDefaultEdgeZeroValue() const;

	void			SetRightEdgeTime( DmeTime_t time );
	DmeTime_t		GetRightEdgeTime() const;

	bool			IsEdgeActive( int edge ) const;
	void			GetEdgeValue( int edge, T& value ) const;

	int				GetEdgeCurveType( int edge ) const;
	void			GetZeroValue( int side, T& val ) const;

	T				ClampValue( const T& value );

	void			SetDefaultValue( const T& value );
	const T&		GetDefaultValue() const;
	bool			HasDefaultValue() const override;
	void			ClearDefaultValue();

	static void SetDefaultValueThreshold( float thresh );
	static float GetDefaultValueThreshold();

	static float s_defaultThreshold;

protected:
	void RemoveKeys( DmeTime_t starttime );
	
	void		_StampKeyAtHeadResample( DmeTime_t tHeadPosition, const DmeLog_TimeSelection_t & params, const T& value, bool bSkipToHead, bool bClearPreviousKeys );
	void		_StampKeyAtHeadFilteredByTimeSelection( DmeTime_t tHeadPosition, DmeTime_t tPreviousHeadPosition, const DmeLog_TimeSelection_t & params, const T& value );
	void		_StampKeyFilteredByTimeSelection( CDmeTypedLogLayer< T > *pWriteLayer, DmeTime_t t, const DmeLog_TimeSelection_t &params, const T& value, bool bForce = false );

protected:
	// this really only makes sense for some of our subclasses, basically those which have float data
	// anything else's threshhold is almost certainly 0, and that class just ignores m_threshold
	float m_threshold;

	CDmaVar< bool >	m_UseDefaultValue;
	CDmaVar< T >	m_DefaultValue;
};


//-----------------------------------------------------------------------------
// Template methods
//-----------------------------------------------------------------------------
template< class T >
DmAttributeType_t CDmeTypedLogLayer<T>::GetDataType() const 
{
	return CDmAttributeInfo< T >::AttributeType();
}

template< class T >
bool CDmeTypedLogLayer<T>::IsConstantValued() const
{
	if ( m_values.Count() < 2 )
		return true;

	if ( m_values.Count() == 2 && !GetTypedOwnerLog()->ValuesDiffer( m_values[ 0 ], m_values[ 1 ] ) )
		return true;

	// we're throwing away duplicate values during recording, so this is generally correct
	// although there are paths to set keys that don't use the duplicate test, so it's not 100%
	return false;
}

//-----------------------------------------------------------------------------
// Template methods
//-----------------------------------------------------------------------------
template< class T >
DmAttributeType_t CDmeTypedLog<T>::GetDataType() const 
{
	return CDmAttributeInfo< T >::AttributeType();
}

template< class T >
void CDmeTypedLog<T>::SetDefaultValueThreshold( float thresh ) 
{
	s_defaultThreshold = thresh;
}

template< class T >
float CDmeTypedLog<T>::GetDefaultValueThreshold() 
{
	return s_defaultThreshold;
}

template< class T >
void CDmeTypedLog<T>::SetValueThreshold( float thresh ) 
{
	m_threshold = thresh;
}

template< class T >
bool CDmeTypedLog<T>::IsConstantValued() const
{
	intp c = m_Layers.Count();
	for ( intp i = 0; i < c; ++i )
	{
		if ( !GetLayer( i )->IsConstantValued() )
			return false;
	}

	return true;
}

template< class T >
void CDmeTypedLog<T>::RemoveRedundantKeys()
{
	intp bestLayer = GetTopmostLayer();
	if ( bestLayer < 0 )
		return;

	GetLayer( bestLayer )->RemoveRedundantKeys();
}

template< class T >
inline float Normalize( const T& val )
{
	Assert( 0 );
	return 0.5f;
}

// AT_INT
// AT_FLOAT
// AT_VECTOR*

template<>
inline float Normalize( const bool& val )
{
	return val ? 1.0f : 0.0f;
}

template<>
inline float Normalize( const Color& val )
{
	float sum = 0.0f;
	for ( int i = 0 ; i < 4; ++i ) //-V112
	{
		sum += val[ i ];
	}
	sum /= 4.0f;
	return clamp( sum / 255.0f, 0.0f, 1.0f );
}

template<>
inline float Normalize( const QAngle& val )
{
	float sum = 0.0f;
	for ( int i = 0 ; i < 3; ++i )
	{
		float ang = val[ i ];
		if ( ang < 0.0f )
		{
			ang += 360.0f;
		}

		sum += ang;
	}
	return clamp( ( sum / 3.0f ) / 360.0f, 0.0f, 1.0f );
}

template<>
inline float Normalize( const Quaternion& val )
{
	QAngle angle;
	QuaternionAngles( val, angle );
	return Normalize( angle );
}

template< class T >
inline void CDmeTypedLog< T >::BuildNormalizedLayer( CDmeTypedLogLayer< float > *pTarget )
{
	Assert( pTarget );
	Assert( GetDataType() != AT_FLOAT );

	CDmeTypedLogLayer< T > *pBaseLayer = static_cast< CDmeTypedLogLayer< T > * >( GetLayer( 0 ) );
	if ( !pBaseLayer )
		return;

	intp kc = pBaseLayer->GetKeyCount();
	for ( intp i = 0; i < kc; ++i )
	{
		DmeTime_t tKeyTime = pBaseLayer->GetKeyTime( i );
		T keyValue = pBaseLayer->GetKeyValue( i );
		float flNormalized = Normalize( keyValue );

		pTarget->InsertKey( tKeyTime, flNormalized );
	}

	if ( HasDefaultValue() )
	{
		pTarget->GetTypedOwnerLog()->SetDefaultValue( Normalize( GetDefaultValue() ) );
	}
}

// Generic implementations all stubbed
// Forward declare specific typed instantiations for float types

template< class T > T CDmeTypedLog< T >::ClampValue( const T& value ) { return value; }
template<> float CDmeTypedLog< float >::ClampValue( const float& value );


template< class T > void CDmeTypedCurveInfo< T >::GetZeroValue( int side, T& val ) const{ Assert( 0 ); }
template< class T > bool CDmeTypedCurveInfo< T >::IsEdgeActive( int edge ) const{ Assert( 0 ); return false; }
template< class T > void CDmeTypedCurveInfo< T >::GetEdgeValue( int edge, T &value ) const{ Assert( 0 ); }

template<> void CDmeTypedCurveInfo< float >::GetZeroValue( int side, float& val ) const;
template<> bool CDmeTypedCurveInfo< float >::IsEdgeActive( int edge ) const;
template<> void CDmeTypedCurveInfo< float >::GetEdgeValue( int edge, float &value ) const;

template<> void CDmeTypedCurveInfo< Vector >::GetZeroValue( int side, Vector& val ) const;
template<> void CDmeTypedCurveInfo< Quaternion >::GetZeroValue( int side, Quaternion& val ) const;

template< class T > void CDmeTypedLogLayer< T >::GetValueUsingCurveInfo( DmeTime_t time, T& out ) const { Assert( 0 ); }
template< class T > void CDmeTypedLogLayer< T >::GetValueUsingCurveInfoSkippingKey( intp nKeyToSkip, T& out ) const { Assert( 0 ); }
template<> void CDmeTypedLogLayer< float >::GetValueUsingCurveInfo( DmeTime_t time, float& out ) const;
template<> void CDmeTypedLogLayer< float >::GetValueUsingCurveInfoSkippingKey( intp nKeyToSkip, float& out ) const;

template<> void CDmeTypedLogLayer< Vector >::GetValueUsingCurveInfo( DmeTime_t time, Vector& out ) const;
template<> void CDmeTypedLogLayer< Vector >::GetValueUsingCurveInfoSkippingKey( intp nKeyToSkip, Vector& out ) const;

template<> void CDmeTypedLogLayer< Quaternion >::GetValueUsingCurveInfo( DmeTime_t time, Quaternion& out ) const;
template<> void CDmeTypedLogLayer< Quaternion >::GetValueUsingCurveInfoSkippingKey( intp nKeyToSkip, Quaternion& out ) const;

template< class T >
float LengthOf( const T& value );
template<>
float LengthOf( const bool& value );
template<>
float LengthOf( const Color& value );
template<>
float LengthOf( const Vector4D& value );
template<>
float LengthOf( const Quaternion& value );
template<>
float LengthOf( const VMatrix& value );
template<>
float LengthOf( const CUtlString& value );
template<>
float LengthOf( const Vector2D& value );
template<>
float LengthOf( const Vector& value );
template<>
float LengthOf( const QAngle& value );

template< class T >
T Subtract( const T& v1, const T& v2 );
template<>
bool Subtract( const bool& v1, const bool& v2 );
template<>
CUtlString Subtract( const CUtlString& v1, const CUtlString& v2 );
template<>
Color Subtract( const Color& v1, const Color& v2 );
template<>
Vector4D Subtract( const Vector4D& v1, const Vector4D& v2 );
template<>
Quaternion Subtract( const Quaternion& v1, const Quaternion& v2 );

// Implementation of Douglas-Peucker curve simplification routine (hacked to only care about error against original curve (sort of 1D)
template< class T >
void CDmeTypedLogLayer< T >::CurveSimplify_R( float thresholdSqr, intp startPoint, intp endPoint, CDmeTypedLogLayer< T > *output )
{
	if ( endPoint <= startPoint + 1 )
	{
		return;
	}

	intp maxPoint = startPoint;
	float maxDistanceSqr = 0.0f;

	for ( intp i = startPoint + 1 ; i < endPoint; ++i )
	{
		DmeTime_t keyTime = GetKeyTime( i );
        T check = GetKeyValue( i );
		T check2 = output->GetValue( keyTime );
		T dist = Subtract( check, check2 );
		float distSqr = LengthOf( dist ) * LengthOf( dist );

		if ( distSqr < maxDistanceSqr )
			continue;

		maxPoint = i;
		maxDistanceSqr = distSqr;
	}

	if ( maxDistanceSqr > thresholdSqr )
	{
		output->InsertKey( GetKeyTime( maxPoint ), GetKeyValue( maxPoint ) );
		CurveSimplify_R( thresholdSqr, startPoint, maxPoint, output );
		CurveSimplify_R( thresholdSqr, maxPoint, endPoint, output );
	}
}


template<> void CDmeTypedLogLayer< bool >::CurveSimplify_R( float thresholdSqr, intp startPoint, intp endPoint, CDmeTypedLogLayer< bool > *output );
template<> void CDmeTypedLogLayer< int >::CurveSimplify_R( float thresholdSqr, intp startPoint, intp endPoint, CDmeTypedLogLayer< int > *output );
template<> void CDmeTypedLogLayer< Color >::CurveSimplify_R( float thresholdSqr, intp startPoint, intp endPoint, CDmeTypedLogLayer< Color > *output );
template<> void CDmeTypedLogLayer< Quaternion >::CurveSimplify_R( float thresholdSqr, intp startPoint, intp endPoint, CDmeTypedLogLayer< Quaternion > *output );
template<> void CDmeTypedLogLayer< VMatrix >::CurveSimplify_R( float thresholdSqr, intp startPoint, intp endPoint, CDmeTypedLogLayer< VMatrix > *output );

template<> void CDmeTypedLog< Vector >::BuildNormalizedLayer( CDmeTypedLogLayer< float > *target );
template<> void CDmeTypedLog< Vector2D >::BuildNormalizedLayer( CDmeTypedLogLayer< float > *target );
template<> void CDmeTypedLog< Vector4D >::BuildNormalizedLayer( CDmeTypedLogLayer< float > *target );
template<> void CDmeTypedLog< float >::BuildNormalizedLayer( CDmeTypedLogLayer< float > *target );
template<> void CDmeTypedLog< int >::BuildNormalizedLayer( CDmeTypedLogLayer< float > *target );

//template<> void CDmeTypedLog< float >::FinishTimeSelection( DmeTime_t tHeadPosition, DmeLog_TimeSelection_t& params );
//template<> void CDmeTypedLog< bool >::_StampKeyAtHeadResample( const DmeLog_TimeSelection_t& params, const bool& value ) { Assert( 0 ); }

//-----------------------------------------------------------------------------
// typedefs for convenience (and so the user-supplied names match the programmer names)
//-----------------------------------------------------------------------------
typedef CDmeTypedLog<int>			CDmeIntLog;
typedef CDmeTypedLog<float>			CDmeFloatLog;
typedef CDmeTypedLog<bool>			CDmeBoolLog;
typedef CDmeTypedLog<Color>			CDmeColorLog;
typedef CDmeTypedLog<Vector2D>		CDmeVector2Log;
typedef CDmeTypedLog<Vector>		CDmeVector3Log;
typedef CDmeTypedLog<Vector4D>		CDmeVector4Log;
typedef CDmeTypedLog<QAngle>		CDmeQAngleLog;
typedef CDmeTypedLog<Quaternion>	CDmeQuaternionLog;
typedef CDmeTypedLog<VMatrix>		CDmeVMatrixLog;
typedef CDmeTypedLog<CUtlString>	CDmeStringLog;

//-----------------------------------------------------------------------------
// typedefs for convenience (and so the user-supplied names match the programmer names)
//-----------------------------------------------------------------------------
typedef CDmeTypedLogLayer<int>			CDmeIntLogLayer;
typedef CDmeTypedLogLayer<float>		CDmeFloatLogLayer;
typedef CDmeTypedLogLayer<bool>			CDmeBoolLogLayer;
typedef CDmeTypedLogLayer<Color>		CDmeColorLogLayer;
typedef CDmeTypedLogLayer<Vector2D>		CDmeVector2LogLayer;
typedef CDmeTypedLogLayer<Vector>		CDmeVector3LogLayer;
typedef CDmeTypedLogLayer<Vector4D>		CDmeVector4LogLayer;
typedef CDmeTypedLogLayer<QAngle>		CDmeQAngleLogLayer;
typedef CDmeTypedLogLayer<Quaternion>	CDmeQuaternionLogLayer;
typedef CDmeTypedLogLayer<VMatrix>		CDmeVMatrixLogLayer;
typedef CDmeTypedLogLayer<CUtlString>	CDmeStringLogLayer;

//-----------------------------------------------------------------------------
// typedefs for convenience (and so the user-supplied names match the programmer names)
//-----------------------------------------------------------------------------
typedef CDmeTypedCurveInfo<int>			CDmeIntCurveInfo;
typedef CDmeTypedCurveInfo<float>		CDmeFloatCurveInfo;
typedef CDmeTypedCurveInfo<bool>		CDmeBoolCurveInfo;
typedef CDmeTypedCurveInfo<Color>		CDmeColorCurveInfo;
typedef CDmeTypedCurveInfo<Vector2D>	CDmeVector2CurveInfo;
typedef CDmeTypedCurveInfo<Vector>		CDmeVector3CurveInfo;
typedef CDmeTypedCurveInfo<Vector4D>	CDmeVector4CurveInfo;
typedef CDmeTypedCurveInfo<QAngle>		CDmeQAngleCurveInfo;
typedef CDmeTypedCurveInfo<Quaternion>	CDmeQuaternionCurveInfo;
typedef CDmeTypedCurveInfo<VMatrix>		CDmeVMatrixCurveInfo;
typedef CDmeTypedCurveInfo<CUtlString>	CDmeStringCurveInfo;

// the following types are not supported
// AT_ELEMENT,
// AT_VOID,
// AT_OBJECTID,
// <all array types>

//-----------------------------------------------------------------------------
// Helpers for particular types of log layers
//-----------------------------------------------------------------------------
void GenerateRotationLog( CDmeQuaternionLogLayer *pLayer, const Vector &vecAxis, DmeTime_t pTime[4], float pRevolutionsPerSec[4] );

// rotates a position log
void RotatePositionLog( CDmeVector3LogLayer *pPositionLog, const matrix3x4_t& matrix );

// rotates an orientation log
void RotateOrientationLog( CDmeQuaternionLogLayer *pOrientationLog, const matrix3x4_t& matrix, bool bPreMultiply );


#endif // DMELOG_H
