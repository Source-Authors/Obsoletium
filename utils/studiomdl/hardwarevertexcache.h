// Copyright Valve Corporation, All rights reserved.

#ifndef SE_UTILS_STUDIOMDL_HARDWARE_VERTEX_CACHE_H_
#define SE_UTILS_STUDIOMDL_HARDWARE_VERTEX_CACHE_H_

// Emulate a hardware post T&L vertex fifo
class CHardwareVertexCache
{
public:
	CHardwareVertexCache();
	void Init( int size );
	void Insert( int index );
	bool IsPresent( int index );
	void Flush( void );
	void Print( void );
private:
	int m_Size;
	int *m_Fifo;
	int m_HeadIndex;
	int m_NumEntries;
};

#endif  // !SE_UTILS_STUDIOMDL_HARDWARE_VERTEX_CACHE_H_
