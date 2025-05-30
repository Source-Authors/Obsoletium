// Copyright Valve Corporation, All rights reserved.

#ifndef TIER0_CL2CACHE_H_
#define TIER0_CL2CACHE_H_

class P4Event_BSQ_cache_reference;

class CL2Cache
{
public:

	CL2Cache();
	~CL2Cache();

	void Start( void );
	void End( void );

	//-------------------------------------------------------------------------
	// GetL2CacheMisses
	//-------------------------------------------------------------------------
	int GetL2CacheMisses() const
	{
		return m_iL2CacheMissCount;
	}

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, tchar *pchName );		// Validate our internal structures
#endif // DBGFLAG_VALIDATE

private:

	P4Event_BSQ_cache_reference		*m_pL2CacheEvent;
	int64							m_i64Start;
	int64							m_i64End;
	int								m_iL2CacheMissCount;
	int								m_nID;
};

#endif   // TIER0_CL2CACHE_H_
