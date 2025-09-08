//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//
#ifndef SE_PUBLIC_TIER1_INSTANCE_LOG_H_
#define SE_PUBLIC_TIER1_INSTANCE_LOG_H_

#include <tuple>
#include <string>
#include <sstream>

#include "utlvector.h"
#include "utlstring.h"

template <class Object, class RecordType>
class InstanceLogger
{
public:
    using RecordType_t = RecordType;

protected:
    void InstanceLog( const char* pMsg )
    {
        m_records.AddToTail( BuildRecord( pMsg ) );
    }

    void DumpInstanceLog( )
    {
		CUtlString str;

		Msg( "--------\nDumping instance log for 0x%08p\n", this );
        for ( auto it : m_records )
		{
			FormatRecord( &str, it );
            Msg( "%s\n", str.Get() );
		}
        Msg( "========\nInstance log complete for 0x%08p\n", this );
    }

	virtual auto BuildRecord( const char* pMsg ) -> RecordType_t = 0;

private:
    CUtlVector< RecordType > m_records;
};

// // Your class needs to implement something like this. You don't have to use a tuple, 
// // but if you don't you also need to implement FormatRecord for your type. 
// RecordType BuildRecord( const char* pMsg )
// {
//     return std::make_tuple( this, pMsg, g_FrameNum, Plat_FloatTime(), m_nAllocatedWidth, m_nAllocatedHeight, m_nAllocatedDepth, m_nTargetResidence, m_nCurrentResidence );
// }

// helper function to print a tuple of any size
template<class Tuple, std::size_t N>
struct TuplePrinter {
	static void print( std::stringstream& os, const Tuple& t )
	{
		TuplePrinter<Tuple, N - 1>::print( os, t );
		os << ", " << std::get<N - 1>( t );
	}
};

template<class Tuple>
struct TuplePrinter < Tuple, 1 > {
	static void print( std::stringstream& os, const Tuple& t )
	{
		os << std::get<0>( t );
	}
};

template<class... Args>
void format( std::stringstream& os, const std::tuple<Args...>& t )
{
	os << "( ";
	TuplePrinter<decltype( t ), sizeof...( Args )>::print( os, t );
	os << " )";
}
// end helper function

template <class RecordType>
void FormatRecord( CUtlString *pOutStr, const RecordType& rt )
{
    std::stringstream stream;
    format( stream, rt );
	( *pOutStr ) = stream.str().c_str();
}

#endif  // !SE_PUBLIC_TIER1_INSTANCE_LOG_H_
