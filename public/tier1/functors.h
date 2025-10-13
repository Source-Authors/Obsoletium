//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements a generic infrastucture for functors combining
//			a number of techniques to provide transparent parameter type
//			deduction and packaging. Supports both member and non-member functions.
//
//			See also: https://en.wikipedia.org/wiki/Function_object
//
//			Note that getting into the guts of this file is not for the
//			feint of heart. The core concept here is that the compiler can
//			figure out all the parameter types.
//
//			E.g.:
//
//			struct CMyClass
//			{
//				void foo( int i) {}
//			};
//
//			int bar(int i) { return i; }
//
//			CMyClass myInstance;
//
//			CFunctor *pFunctor = CreateFunctor( &myInstance, CMyClass::foo, 8675 );
//			CFunctor *pFunctor2 = CreateFunctor( &bar, 309 );
//
//			void CallEm()
//			{
//				(*pFunctor)();
//				(*pFunctor2)();
//			}
//
//=============================================================================

#ifndef FUNCTORS_H
#define FUNCTORS_H

#include "tier0/platform.h"
#include "refcount.h"
#include "utlenvelope.h"
#include "utlcommon.h"

//-----------------------------------------------------------------------------
//
// Macros used as basis for template generation. Just ignore the man behind the
// curtain
//
//-----------------------------------------------------------------------------

#define	FUNC_TEMPLATE_ARG_PARAMS_0			
#define	FUNC_BASE_TEMPLATE_ARG_PARAMS_0		
#define	FUNC_ARG_MEMBERS_0					
#define	FUNC_ARG_FORMAL_PARAMS_0			
#define	FUNC_PROXY_ARG_FORMAL_PARAMS_0		
#define	FUNC_CALL_ARGS_INIT_0				
#define	FUNC_CALL_MEMBER_ARGS_0				
#define	FUNC_CALL_ARGS_0					
#define	FUNC_FUNCTOR_CALL_ARGS_0			
#define	FUNC_TEMPLATE_FUNC_PARAMS_0			
#define	FUNC_BASE_TEMPLATE_FUNC_PARAMS_0	
#define	FUNC_ADDL_TEMPLATE_FUNC_PARAMS_0	

#define	FUNC_TEMPLATE_ARG_PARAMS_1			, typename ARG_TYPE_1
#define	FUNC_BASE_TEMPLATE_ARG_PARAMS_1		, ARG_TYPE_1
#define	FUNC_ARG_MEMBERS_1					ARG_TYPE_1 m_arg1
#define	FUNC_ARG_FORMAL_PARAMS_1			, const ARG_TYPE_1 &arg1
#define	FUNC_PROXY_ARG_FORMAL_PARAMS_1		const ARG_TYPE_1 &arg1
#define	FUNC_CALL_ARGS_INIT_1				, m_arg1( arg1 )
#define	FUNC_CALL_MEMBER_ARGS_1				m_arg1
#define	FUNC_CALL_ARGS_1					arg1
#define	FUNC_FUNCTOR_CALL_ARGS_1			, arg1
#define	FUNC_TEMPLATE_FUNC_PARAMS_1			, typename FUNC_ARG_TYPE_1
#define	FUNC_BASE_TEMPLATE_FUNC_PARAMS_1	FUNC_ARG_TYPE_1
#define	FUNC_ADDL_TEMPLATE_FUNC_PARAMS_1	, FUNC_ARG_TYPE_1

#define	FUNC_TEMPLATE_ARG_PARAMS_2			, typename ARG_TYPE_1, typename ARG_TYPE_2
#define	FUNC_BASE_TEMPLATE_ARG_PARAMS_2		, ARG_TYPE_1, ARG_TYPE_2
#define	FUNC_ARG_MEMBERS_2					ARG_TYPE_1 m_arg1;	ARG_TYPE_2 m_arg2
#define	FUNC_ARG_FORMAL_PARAMS_2			, const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2
#define	FUNC_PROXY_ARG_FORMAL_PARAMS_2		const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2
#define	FUNC_CALL_ARGS_INIT_2				, m_arg1( arg1 ), m_arg2( arg2 )
#define	FUNC_CALL_MEMBER_ARGS_2				m_arg1, m_arg2
#define	FUNC_CALL_ARGS_2					arg1, arg2
#define	FUNC_FUNCTOR_CALL_ARGS_2			, arg1, arg2
#define	FUNC_TEMPLATE_FUNC_PARAMS_2			, typename FUNC_ARG_TYPE_1, typename	FUNC_ARG_TYPE_2
#define	FUNC_BASE_TEMPLATE_FUNC_PARAMS_2	FUNC_ARG_TYPE_1, FUNC_ARG_TYPE_2
#define	FUNC_ADDL_TEMPLATE_FUNC_PARAMS_2	, FUNC_ARG_TYPE_1, FUNC_ARG_TYPE_2

#define	FUNC_TEMPLATE_ARG_PARAMS_3			, typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3
#define	FUNC_BASE_TEMPLATE_ARG_PARAMS_3		, ARG_TYPE_1, ARG_TYPE_2, ARG_TYPE_3
#define	FUNC_ARG_MEMBERS_3					ARG_TYPE_1 m_arg1;	ARG_TYPE_2 m_arg2; ARG_TYPE_3 m_arg3
#define	FUNC_ARG_FORMAL_PARAMS_3			, const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3
#define	FUNC_PROXY_ARG_FORMAL_PARAMS_3		const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3
#define	FUNC_CALL_ARGS_INIT_3				, m_arg1( arg1 ), m_arg2( arg2 ), m_arg3( arg3 )
#define	FUNC_CALL_MEMBER_ARGS_3				m_arg1, m_arg2, m_arg3
#define	FUNC_CALL_ARGS_3					arg1, arg2, arg3
#define	FUNC_FUNCTOR_CALL_ARGS_3			, arg1, arg2, arg3
#define	FUNC_TEMPLATE_FUNC_PARAMS_3			, typename FUNC_ARG_TYPE_1, typename	FUNC_ARG_TYPE_2, typename FUNC_ARG_TYPE_3
#define	FUNC_BASE_TEMPLATE_FUNC_PARAMS_3	FUNC_ARG_TYPE_1, FUNC_ARG_TYPE_2, FUNC_ARG_TYPE_3
#define	FUNC_ADDL_TEMPLATE_FUNC_PARAMS_3	, FUNC_ARG_TYPE_1, FUNC_ARG_TYPE_2, FUNC_ARG_TYPE_3

#define	FUNC_TEMPLATE_ARG_PARAMS_4			, typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4
#define	FUNC_BASE_TEMPLATE_ARG_PARAMS_4		, ARG_TYPE_1, ARG_TYPE_2, ARG_TYPE_3, ARG_TYPE_4
#define	FUNC_ARG_MEMBERS_4					ARG_TYPE_1 m_arg1;	ARG_TYPE_2 m_arg2; ARG_TYPE_3 m_arg3; ARG_TYPE_4 m_arg4
#define	FUNC_ARG_FORMAL_PARAMS_4			, const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3, const ARG_TYPE_4 &arg4
#define	FUNC_PROXY_ARG_FORMAL_PARAMS_4		const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3, const ARG_TYPE_4 &arg4
#define	FUNC_CALL_ARGS_INIT_4				, m_arg1( arg1 ), m_arg2( arg2 ), m_arg3( arg3 ), m_arg4( arg4 )
#define	FUNC_CALL_MEMBER_ARGS_4				m_arg1, m_arg2, m_arg3, m_arg4
#define	FUNC_CALL_ARGS_4					arg1, arg2, arg3, arg4
#define	FUNC_FUNCTOR_CALL_ARGS_4			, arg1, arg2, arg3, arg4
#define	FUNC_TEMPLATE_FUNC_PARAMS_4			, typename FUNC_ARG_TYPE_1, typename	FUNC_ARG_TYPE_2, typename FUNC_ARG_TYPE_3, typename	FUNC_ARG_TYPE_4
#define	FUNC_BASE_TEMPLATE_FUNC_PARAMS_4	FUNC_ARG_TYPE_1, FUNC_ARG_TYPE_2, FUNC_ARG_TYPE_3,	FUNC_ARG_TYPE_4
#define	FUNC_ADDL_TEMPLATE_FUNC_PARAMS_4	, FUNC_ARG_TYPE_1, FUNC_ARG_TYPE_2, FUNC_ARG_TYPE_3,	FUNC_ARG_TYPE_4

#define	FUNC_TEMPLATE_ARG_PARAMS_5			, typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5
#define	FUNC_BASE_TEMPLATE_ARG_PARAMS_5		, ARG_TYPE_1, ARG_TYPE_2, ARG_TYPE_3, ARG_TYPE_4, ARG_TYPE_5
#define	FUNC_ARG_MEMBERS_5					ARG_TYPE_1 m_arg1;	ARG_TYPE_2 m_arg2; ARG_TYPE_3 m_arg3; ARG_TYPE_4 m_arg4; ARG_TYPE_5 m_arg5
#define	FUNC_ARG_FORMAL_PARAMS_5			, const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3, const ARG_TYPE_4 &arg4, const ARG_TYPE_5 &arg5
#define	FUNC_PROXY_ARG_FORMAL_PARAMS_5		const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3, const ARG_TYPE_4 &arg4, const ARG_TYPE_5 &arg5
#define	FUNC_CALL_ARGS_INIT_5				, m_arg1( arg1 ), m_arg2( arg2 ), m_arg3( arg3 ), m_arg4( arg4 ), m_arg5( arg5 )
#define	FUNC_CALL_MEMBER_ARGS_5				m_arg1, m_arg2, m_arg3, m_arg4, m_arg5
#define	FUNC_CALL_ARGS_5					arg1, arg2, arg3, arg4, arg5
#define	FUNC_FUNCTOR_CALL_ARGS_5			, arg1, arg2, arg3, arg4, arg5
#define	FUNC_TEMPLATE_FUNC_PARAMS_5			, typename FUNC_ARG_TYPE_1, typename	FUNC_ARG_TYPE_2, typename FUNC_ARG_TYPE_3, typename	FUNC_ARG_TYPE_4, typename FUNC_ARG_TYPE_5
#define	FUNC_BASE_TEMPLATE_FUNC_PARAMS_5	FUNC_ARG_TYPE_1, FUNC_ARG_TYPE_2, FUNC_ARG_TYPE_3,	FUNC_ARG_TYPE_4, FUNC_ARG_TYPE_5
#define	FUNC_ADDL_TEMPLATE_FUNC_PARAMS_5	, FUNC_ARG_TYPE_1, FUNC_ARG_TYPE_2, FUNC_ARG_TYPE_3,	FUNC_ARG_TYPE_4, FUNC_ARG_TYPE_5

#define	FUNC_TEMPLATE_ARG_PARAMS_6			, typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6
#define	FUNC_BASE_TEMPLATE_ARG_PARAMS_6		, ARG_TYPE_1, ARG_TYPE_2, ARG_TYPE_3, ARG_TYPE_4, ARG_TYPE_5, ARG_TYPE_6
#define	FUNC_ARG_MEMBERS_6					ARG_TYPE_1 m_arg1;	ARG_TYPE_2 m_arg2; ARG_TYPE_3 m_arg3; ARG_TYPE_4 m_arg4; ARG_TYPE_5 m_arg5;	ARG_TYPE_6 m_arg6
#define	FUNC_ARG_FORMAL_PARAMS_6			, const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3, const ARG_TYPE_4 &arg4, const ARG_TYPE_5 &arg5,	const ARG_TYPE_6 &arg6
#define	FUNC_PROXY_ARG_FORMAL_PARAMS_6		const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3, const ARG_TYPE_4 &arg4, const ARG_TYPE_5 &arg5,	const ARG_TYPE_6 &arg6
#define	FUNC_CALL_ARGS_INIT_6				, m_arg1( arg1 ), m_arg2( arg2 ), m_arg3( arg3 ), m_arg4( arg4 ), m_arg5( arg5 ), m_arg6( arg6 )
#define	FUNC_CALL_MEMBER_ARGS_6				m_arg1, m_arg2, m_arg3, m_arg4, m_arg5, m_arg6
#define	FUNC_CALL_ARGS_6					arg1, arg2, arg3, arg4, arg5, arg6
#define	FUNC_FUNCTOR_CALL_ARGS_6			, arg1, arg2, arg3, arg4, arg5, arg6
#define	FUNC_TEMPLATE_FUNC_PARAMS_6			, typename FUNC_ARG_TYPE_1, typename	FUNC_ARG_TYPE_2, typename FUNC_ARG_TYPE_3, typename	FUNC_ARG_TYPE_4, typename FUNC_ARG_TYPE_5, typename	FUNC_ARG_TYPE_6
#define	FUNC_BASE_TEMPLATE_FUNC_PARAMS_6	FUNC_ARG_TYPE_1, FUNC_ARG_TYPE_2, FUNC_ARG_TYPE_3,	FUNC_ARG_TYPE_4, FUNC_ARG_TYPE_5, FUNC_ARG_TYPE_6
#define	FUNC_ADDL_TEMPLATE_FUNC_PARAMS_6	, FUNC_ARG_TYPE_1, FUNC_ARG_TYPE_2, FUNC_ARG_TYPE_3,	FUNC_ARG_TYPE_4, FUNC_ARG_TYPE_5, FUNC_ARG_TYPE_6

#define	FUNC_TEMPLATE_ARG_PARAMS_7			, typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7
#define	FUNC_BASE_TEMPLATE_ARG_PARAMS_7		, ARG_TYPE_1, ARG_TYPE_2, ARG_TYPE_3, ARG_TYPE_4, ARG_TYPE_5, ARG_TYPE_6, ARG_TYPE_7
#define	FUNC_ARG_MEMBERS_7					ARG_TYPE_1 m_arg1;	ARG_TYPE_2 m_arg2; ARG_TYPE_3 m_arg3; ARG_TYPE_4 m_arg4; ARG_TYPE_5 m_arg5;	ARG_TYPE_6 m_arg6;	ARG_TYPE_7 m_arg7;
#define	FUNC_ARG_FORMAL_PARAMS_7			, const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3, const ARG_TYPE_4 &arg4, const ARG_TYPE_5 &arg5,	const ARG_TYPE_6 &arg6,	const ARG_TYPE_7 &arg7
#define	FUNC_PROXY_ARG_FORMAL_PARAMS_7		const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3, const ARG_TYPE_4 &arg4, const ARG_TYPE_5 &arg5,	const ARG_TYPE_6 &arg6,	const ARG_TYPE_7 &arg7
#define	FUNC_CALL_ARGS_INIT_7				, m_arg1( arg1 ), m_arg2( arg2 ), m_arg3( arg3 ), m_arg4( arg4 ), m_arg5( arg5 ), m_arg6( arg6 ), m_arg7( arg7 )
#define	FUNC_CALL_MEMBER_ARGS_7				m_arg1, m_arg2, m_arg3, m_arg4, m_arg5, m_arg6, m_arg7
#define	FUNC_CALL_ARGS_7					arg1, arg2, arg3, arg4, arg5, arg6, arg7
#define	FUNC_FUNCTOR_CALL_ARGS_7			, arg1, arg2, arg3, arg4, arg5, arg6, arg7
#define	FUNC_TEMPLATE_FUNC_PARAMS_7			, typename FUNC_ARG_TYPE_1, typename	FUNC_ARG_TYPE_2, typename FUNC_ARG_TYPE_3, typename	FUNC_ARG_TYPE_4, typename FUNC_ARG_TYPE_5, typename	FUNC_ARG_TYPE_6, typename FUNC_ARG_TYPE_7
#define	FUNC_BASE_TEMPLATE_FUNC_PARAMS_7	FUNC_ARG_TYPE_1, FUNC_ARG_TYPE_2, FUNC_ARG_TYPE_3,	FUNC_ARG_TYPE_4, FUNC_ARG_TYPE_5, FUNC_ARG_TYPE_6, FUNC_ARG_TYPE_7
#define	FUNC_ADDL_TEMPLATE_FUNC_PARAMS_7	, FUNC_ARG_TYPE_1, FUNC_ARG_TYPE_2, FUNC_ARG_TYPE_3,	FUNC_ARG_TYPE_4, FUNC_ARG_TYPE_5, FUNC_ARG_TYPE_6, FUNC_ARG_TYPE_7

#define	FUNC_TEMPLATE_ARG_PARAMS_8			, typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8
#define	FUNC_BASE_TEMPLATE_ARG_PARAMS_8		, ARG_TYPE_1, ARG_TYPE_2, ARG_TYPE_3, ARG_TYPE_4, ARG_TYPE_5, ARG_TYPE_6, ARG_TYPE_7, ARG_TYPE_8
#define	FUNC_ARG_MEMBERS_8					ARG_TYPE_1 m_arg1;	ARG_TYPE_2 m_arg2; ARG_TYPE_3 m_arg3; ARG_TYPE_4 m_arg4; ARG_TYPE_5 m_arg5;	ARG_TYPE_6 m_arg6;	ARG_TYPE_7 m_arg7; ARG_TYPE_8 m_arg8;
#define	FUNC_ARG_FORMAL_PARAMS_8			, const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3, const ARG_TYPE_4 &arg4, const ARG_TYPE_5 &arg5,	const ARG_TYPE_6 &arg6,	const ARG_TYPE_7 &arg7, const ARG_TYPE_8 &arg8
#define	FUNC_PROXY_ARG_FORMAL_PARAMS_8		const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3, const ARG_TYPE_4 &arg4, const ARG_TYPE_5 &arg5,	const ARG_TYPE_6 &arg6,	const ARG_TYPE_7 &arg7, const ARG_TYPE_8 &arg8
#define	FUNC_CALL_ARGS_INIT_8				, m_arg1( arg1 ), m_arg2( arg2 ), m_arg3( arg3 ), m_arg4( arg4 ), m_arg5( arg5 ), m_arg6( arg6 ), m_arg7( arg7 ), m_arg8( arg8 )
#define	FUNC_CALL_MEMBER_ARGS_8				m_arg1, m_arg2, m_arg3, m_arg4, m_arg5, m_arg6, m_arg7, m_arg8
#define	FUNC_CALL_ARGS_8					arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8
#define	FUNC_FUNCTOR_CALL_ARGS_8			, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8
#define	FUNC_TEMPLATE_FUNC_PARAMS_8			, typename FUNC_ARG_TYPE_1, typename	FUNC_ARG_TYPE_2, typename FUNC_ARG_TYPE_3, typename	FUNC_ARG_TYPE_4, typename FUNC_ARG_TYPE_5, typename	FUNC_ARG_TYPE_6, typename FUNC_ARG_TYPE_7, typename FUNC_ARG_TYPE_8
#define	FUNC_BASE_TEMPLATE_FUNC_PARAMS_8	FUNC_ARG_TYPE_1, FUNC_ARG_TYPE_2, FUNC_ARG_TYPE_3,	FUNC_ARG_TYPE_4, FUNC_ARG_TYPE_5, FUNC_ARG_TYPE_6, FUNC_ARG_TYPE_7, FUNC_ARG_TYPE_8
#define	FUNC_ADDL_TEMPLATE_FUNC_PARAMS_8	, FUNC_ARG_TYPE_1, FUNC_ARG_TYPE_2, FUNC_ARG_TYPE_3,	FUNC_ARG_TYPE_4, FUNC_ARG_TYPE_5, FUNC_ARG_TYPE_6, FUNC_ARG_TYPE_7, FUNC_ARG_TYPE_8

#define	FUNC_TEMPLATE_ARG_PARAMS_9			, typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9
#define	FUNC_BASE_TEMPLATE_ARG_PARAMS_9		, ARG_TYPE_1, ARG_TYPE_2, ARG_TYPE_3, ARG_TYPE_4, ARG_TYPE_5, ARG_TYPE_6, ARG_TYPE_7, ARG_TYPE_8, ARG_TYPE_9
#define	FUNC_ARG_MEMBERS_9					ARG_TYPE_1 m_arg1;	ARG_TYPE_2 m_arg2; ARG_TYPE_3 m_arg3; ARG_TYPE_4 m_arg4; ARG_TYPE_5 m_arg5;	ARG_TYPE_6 m_arg6;	ARG_TYPE_7 m_arg7; ARG_TYPE_8 m_arg8; ARG_TYPE_9 m_arg9;
#define	FUNC_ARG_FORMAL_PARAMS_9			, const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3, const ARG_TYPE_4 &arg4, const ARG_TYPE_5 &arg5,	const ARG_TYPE_6 &arg6,	const ARG_TYPE_7 &arg7, const ARG_TYPE_8 &arg8, const ARG_TYPE_9 &arg9
#define	FUNC_PROXY_ARG_FORMAL_PARAMS_9		const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3, const ARG_TYPE_4 &arg4, const ARG_TYPE_5 &arg5,	const ARG_TYPE_6 &arg6,	const ARG_TYPE_7 &arg7, const ARG_TYPE_8 &arg8, const ARG_TYPE_9 &arg9
#define	FUNC_CALL_ARGS_INIT_9				, m_arg1( arg1 ), m_arg2( arg2 ), m_arg3( arg3 ), m_arg4( arg4 ), m_arg5( arg5 ), m_arg6( arg6 ), m_arg7( arg7 ), m_arg8( arg8 ), m_arg9( arg9 )
#define	FUNC_CALL_MEMBER_ARGS_9				m_arg1, m_arg2, m_arg3, m_arg4, m_arg5, m_arg6, m_arg7, m_arg8, m_arg9
#define	FUNC_CALL_ARGS_9					arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9
#define	FUNC_FUNCTOR_CALL_ARGS_9			, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9
#define	FUNC_TEMPLATE_FUNC_PARAMS_9			, typename FUNC_ARG_TYPE_1, typename	FUNC_ARG_TYPE_2, typename FUNC_ARG_TYPE_3, typename	FUNC_ARG_TYPE_4, typename FUNC_ARG_TYPE_5, typename	FUNC_ARG_TYPE_6, typename FUNC_ARG_TYPE_7, typename FUNC_ARG_TYPE_8, typename	FUNC_ARG_TYPE_9
#define	FUNC_BASE_TEMPLATE_FUNC_PARAMS_9	FUNC_ARG_TYPE_1, FUNC_ARG_TYPE_2, FUNC_ARG_TYPE_3,	FUNC_ARG_TYPE_4, FUNC_ARG_TYPE_5, FUNC_ARG_TYPE_6, FUNC_ARG_TYPE_7, FUNC_ARG_TYPE_8, FUNC_ARG_TYPE_9
#define	FUNC_ADDL_TEMPLATE_FUNC_PARAMS_9	, FUNC_ARG_TYPE_1, FUNC_ARG_TYPE_2, FUNC_ARG_TYPE_3,	FUNC_ARG_TYPE_4, FUNC_ARG_TYPE_5, FUNC_ARG_TYPE_6, FUNC_ARG_TYPE_7, FUNC_ARG_TYPE_8, FUNC_ARG_TYPE_9

#define	FUNC_TEMPLATE_ARG_PARAMS_10			, typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9, typename ARG_TYPE_10
#define	FUNC_BASE_TEMPLATE_ARG_PARAMS_10	, ARG_TYPE_1, ARG_TYPE_2, ARG_TYPE_3, ARG_TYPE_4, ARG_TYPE_5, ARG_TYPE_6, ARG_TYPE_7, ARG_TYPE_8, ARG_TYPE_9, ARG_TYPE_10
#define	FUNC_ARG_MEMBERS_10					ARG_TYPE_1 m_arg1;	ARG_TYPE_2 m_arg2; ARG_TYPE_3 m_arg3; ARG_TYPE_4 m_arg4; ARG_TYPE_5 m_arg5;	ARG_TYPE_6 m_arg6;	ARG_TYPE_7 m_arg7; ARG_TYPE_8 m_arg8; ARG_TYPE_9 m_arg9; ARG_TYPE_10 m_arg10;
#define	FUNC_ARG_FORMAL_PARAMS_10			, const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3, const ARG_TYPE_4 &arg4, const ARG_TYPE_5 &arg5,	const ARG_TYPE_6 &arg6,	const ARG_TYPE_7 &arg7, const ARG_TYPE_8 &arg8, const ARG_TYPE_9 &arg9, const ARG_TYPE_10 &arg10
#define	FUNC_PROXY_ARG_FORMAL_PARAMS_10		const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3, const ARG_TYPE_4 &arg4, const ARG_TYPE_5 &arg5,	const ARG_TYPE_6 &arg6,	const ARG_TYPE_7 &arg7, const ARG_TYPE_8 &arg8, const ARG_TYPE_9 &arg9, const ARG_TYPE_10 &arg10
#define	FUNC_CALL_ARGS_INIT_10				, m_arg1( arg1 ), m_arg2( arg2 ), m_arg3( arg3 ), m_arg4( arg4 ), m_arg5( arg5 ), m_arg6( arg6 ), m_arg7( arg7 ), m_arg8( arg8 ), m_arg9( arg9 ), m_arg10( arg10 )
#define	FUNC_CALL_MEMBER_ARGS_10			m_arg1, m_arg2, m_arg3, m_arg4, m_arg5, m_arg6, m_arg7, m_arg8, m_arg9, m_arg10
#define	FUNC_CALL_ARGS_10					arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10
#define	FUNC_FUNCTOR_CALL_ARGS_10			, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10
#define	FUNC_TEMPLATE_FUNC_PARAMS_10		, typename FUNC_ARG_TYPE_1, typename	FUNC_ARG_TYPE_2, typename FUNC_ARG_TYPE_3, typename	FUNC_ARG_TYPE_4, typename FUNC_ARG_TYPE_5, typename	FUNC_ARG_TYPE_6, typename FUNC_ARG_TYPE_7, typename FUNC_ARG_TYPE_8, typename	FUNC_ARG_TYPE_9, typename FUNC_ARG_TYPE_10
#define	FUNC_BASE_TEMPLATE_FUNC_PARAMS_10	FUNC_ARG_TYPE_1, FUNC_ARG_TYPE_2, FUNC_ARG_TYPE_3,	FUNC_ARG_TYPE_4, FUNC_ARG_TYPE_5, FUNC_ARG_TYPE_6, FUNC_ARG_TYPE_7, FUNC_ARG_TYPE_8, FUNC_ARG_TYPE_9, FUNC_ARG_TYPE_10
#define	FUNC_ADDL_TEMPLATE_FUNC_PARAMS_10	, FUNC_ARG_TYPE_1, FUNC_ARG_TYPE_2, FUNC_ARG_TYPE_3,	FUNC_ARG_TYPE_4, FUNC_ARG_TYPE_5, FUNC_ARG_TYPE_6, FUNC_ARG_TYPE_7, FUNC_ARG_TYPE_8, FUNC_ARG_TYPE_9, FUNC_ARG_TYPE_10

#define	FUNC_TEMPLATE_ARG_PARAMS_11			, typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9, typename ARG_TYPE_10, typename ARG_TYPE_11
#define	FUNC_BASE_TEMPLATE_ARG_PARAMS_11	, ARG_TYPE_1, ARG_TYPE_2, ARG_TYPE_3, ARG_TYPE_4, ARG_TYPE_5, ARG_TYPE_6, ARG_TYPE_7, ARG_TYPE_8, ARG_TYPE_9, ARG_TYPE_10, ARG_TYPE_11
#define	FUNC_ARG_MEMBERS_11					ARG_TYPE_1 m_arg1;	ARG_TYPE_2 m_arg2; ARG_TYPE_3 m_arg3; ARG_TYPE_4 m_arg4; ARG_TYPE_5 m_arg5;	ARG_TYPE_6 m_arg6;	ARG_TYPE_7 m_arg7; ARG_TYPE_8 m_arg8; ARG_TYPE_9 m_arg9; ARG_TYPE_10 m_arg10;	ARG_TYPE_11 m_arg11
#define	FUNC_ARG_FORMAL_PARAMS_11			, const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3, const ARG_TYPE_4 &arg4, const ARG_TYPE_5 &arg5,	const ARG_TYPE_6 &arg6,	const ARG_TYPE_7 &arg7, const ARG_TYPE_8 &arg8, const ARG_TYPE_9 &arg9, const ARG_TYPE_10 &arg10,	const ARG_TYPE_11 &arg11
#define	FUNC_PROXY_ARG_FORMAL_PARAMS_11		const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3, const ARG_TYPE_4 &arg4, const ARG_TYPE_5 &arg5,	const ARG_TYPE_6 &arg6,	const ARG_TYPE_7 &arg7, const ARG_TYPE_8 &arg8, const ARG_TYPE_9 &arg9, const ARG_TYPE_10 &arg10,	const ARG_TYPE_11 &arg11
#define	FUNC_CALL_ARGS_INIT_11				, m_arg1( arg1 ), m_arg2( arg2 ), m_arg3( arg3 ), m_arg4( arg4 ), m_arg5( arg5 ), m_arg6( arg6 ), m_arg7( arg7 ), m_arg8( arg8 ), m_arg9( arg9 ), m_arg10( arg10 ), m_arg11( arg11 )
#define	FUNC_CALL_MEMBER_ARGS_11			m_arg1, m_arg2, m_arg3, m_arg4, m_arg5, m_arg6, m_arg7, m_arg8, m_arg9, m_arg10, m_arg11
#define	FUNC_CALL_ARGS_11					arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11
#define	FUNC_FUNCTOR_CALL_ARGS_11			, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11
#define	FUNC_TEMPLATE_FUNC_PARAMS_11		, typename FUNC_ARG_TYPE_1, typename	FUNC_ARG_TYPE_2, typename FUNC_ARG_TYPE_3, typename	FUNC_ARG_TYPE_4, typename FUNC_ARG_TYPE_5, typename	FUNC_ARG_TYPE_6, typename FUNC_ARG_TYPE_7, typename FUNC_ARG_TYPE_8, typename	FUNC_ARG_TYPE_9, typename FUNC_ARG_TYPE_10, typename FUNC_ARG_TYPE_11
#define	FUNC_BASE_TEMPLATE_FUNC_PARAMS_11	FUNC_ARG_TYPE_1, FUNC_ARG_TYPE_2, FUNC_ARG_TYPE_3,	FUNC_ARG_TYPE_4, FUNC_ARG_TYPE_5, FUNC_ARG_TYPE_6, FUNC_ARG_TYPE_7, FUNC_ARG_TYPE_8, FUNC_ARG_TYPE_9, FUNC_ARG_TYPE_10, FUNC_ARG_TYPE_11
#define	FUNC_ADDL_TEMPLATE_FUNC_PARAMS_11	, FUNC_ARG_TYPE_1, FUNC_ARG_TYPE_2, FUNC_ARG_TYPE_3,	FUNC_ARG_TYPE_4, FUNC_ARG_TYPE_5, FUNC_ARG_TYPE_6, FUNC_ARG_TYPE_7, FUNC_ARG_TYPE_8, FUNC_ARG_TYPE_9, FUNC_ARG_TYPE_10, FUNC_ARG_TYPE_11

#define	FUNC_TEMPLATE_ARG_PARAMS_12			, typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9, typename ARG_TYPE_10, typename ARG_TYPE_11, typename ARG_TYPE_12
#define	FUNC_BASE_TEMPLATE_ARG_PARAMS_12	, ARG_TYPE_1, ARG_TYPE_2, ARG_TYPE_3, ARG_TYPE_4, ARG_TYPE_5, ARG_TYPE_6, ARG_TYPE_7, ARG_TYPE_8, ARG_TYPE_9, ARG_TYPE_10, ARG_TYPE_11, ARG_TYPE_12
#define	FUNC_ARG_MEMBERS_12					ARG_TYPE_1 m_arg1;	ARG_TYPE_2 m_arg2; ARG_TYPE_3 m_arg3; ARG_TYPE_4 m_arg4; ARG_TYPE_5 m_arg5;	ARG_TYPE_6 m_arg6;	ARG_TYPE_7 m_arg7; ARG_TYPE_8 m_arg8; ARG_TYPE_9 m_arg9; ARG_TYPE_10 m_arg10; ARG_TYPE_11 m_arg11; ARG_TYPE_12 m_arg12
#define	FUNC_ARG_FORMAL_PARAMS_12			, const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3, const ARG_TYPE_4 &arg4, const ARG_TYPE_5 &arg5,	const ARG_TYPE_6 &arg6,	const ARG_TYPE_7 &arg7, const ARG_TYPE_8 &arg8, const ARG_TYPE_9 &arg9, const ARG_TYPE_10 &arg10, const ARG_TYPE_11 &arg11, const ARG_TYPE_12 &arg12
#define	FUNC_PROXY_ARG_FORMAL_PARAMS_12		const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3, const ARG_TYPE_4 &arg4, const ARG_TYPE_5 &arg5,	const ARG_TYPE_6 &arg6,	const ARG_TYPE_7 &arg7, const ARG_TYPE_8 &arg8, const ARG_TYPE_9 &arg9, const ARG_TYPE_10 &arg10, const ARG_TYPE_11 &arg11, const ARG_TYPE_12 &arg12
#define	FUNC_CALL_ARGS_INIT_12				, m_arg1( arg1 ), m_arg2( arg2 ), m_arg3( arg3 ), m_arg4( arg4 ), m_arg5( arg5 ), m_arg6( arg6 ), m_arg7( arg7 ), m_arg8( arg8 ), m_arg9( arg9 ), m_arg10( arg10 ), m_arg11( arg11 ), m_arg12( arg12 )
#define	FUNC_CALL_MEMBER_ARGS_12			m_arg1, m_arg2, m_arg3, m_arg4, m_arg5, m_arg6, m_arg7, m_arg8, m_arg9, m_arg10, m_arg11, m_arg12
#define	FUNC_CALL_ARGS_12					arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12
#define	FUNC_FUNCTOR_CALL_ARGS_12			, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12
#define	FUNC_TEMPLATE_FUNC_PARAMS_12		, typename FUNC_ARG_TYPE_1, typename	FUNC_ARG_TYPE_2, typename FUNC_ARG_TYPE_3, typename	FUNC_ARG_TYPE_4, typename FUNC_ARG_TYPE_5, typename	FUNC_ARG_TYPE_6, typename FUNC_ARG_TYPE_7, typename FUNC_ARG_TYPE_8, typename	FUNC_ARG_TYPE_9, typename FUNC_ARG_TYPE_10, typename FUNC_ARG_TYPE_11, typename FUNC_ARG_TYPE_12
#define	FUNC_BASE_TEMPLATE_FUNC_PARAMS_12	FUNC_ARG_TYPE_1, FUNC_ARG_TYPE_2, FUNC_ARG_TYPE_3,	FUNC_ARG_TYPE_4, FUNC_ARG_TYPE_5, FUNC_ARG_TYPE_6, FUNC_ARG_TYPE_7, FUNC_ARG_TYPE_8, FUNC_ARG_TYPE_9, FUNC_ARG_TYPE_10, FUNC_ARG_TYPE_11, FUNC_ARG_TYPE_12
#define	FUNC_ADDL_TEMPLATE_FUNC_PARAMS_12	, FUNC_ARG_TYPE_1, FUNC_ARG_TYPE_2, FUNC_ARG_TYPE_3,	FUNC_ARG_TYPE_4, FUNC_ARG_TYPE_5, FUNC_ARG_TYPE_6, FUNC_ARG_TYPE_7, FUNC_ARG_TYPE_8, FUNC_ARG_TYPE_9, FUNC_ARG_TYPE_10, FUNC_ARG_TYPE_11, FUNC_ARG_TYPE_12

#define	FUNC_TEMPLATE_ARG_PARAMS_13			, typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9, typename ARG_TYPE_10, typename ARG_TYPE_11, typename ARG_TYPE_12, typename ARG_TYPE_13
#define	FUNC_BASE_TEMPLATE_ARG_PARAMS_13	, ARG_TYPE_1, ARG_TYPE_2, ARG_TYPE_3, ARG_TYPE_4, ARG_TYPE_5, ARG_TYPE_6, ARG_TYPE_7, ARG_TYPE_8, ARG_TYPE_9, ARG_TYPE_10, ARG_TYPE_11, ARG_TYPE_12, ARG_TYPE_13
#define	FUNC_ARG_MEMBERS_13					ARG_TYPE_1 m_arg1;	ARG_TYPE_2 m_arg2; ARG_TYPE_3 m_arg3; ARG_TYPE_4 m_arg4; ARG_TYPE_5 m_arg5;	ARG_TYPE_6 m_arg6;	ARG_TYPE_7 m_arg7; ARG_TYPE_8 m_arg8; ARG_TYPE_9 m_arg9; ARG_TYPE_10 m_arg10; ARG_TYPE_11 m_arg11; ARG_TYPE_12 m_arg12; ARG_TYPE_13 m_arg13
#define	FUNC_ARG_FORMAL_PARAMS_13			, const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3, const ARG_TYPE_4 &arg4, const ARG_TYPE_5 &arg5,	const ARG_TYPE_6 &arg6,	const ARG_TYPE_7 &arg7, const ARG_TYPE_8 &arg8, const ARG_TYPE_9 &arg9, const ARG_TYPE_10 &arg10, const ARG_TYPE_11 &arg11, const ARG_TYPE_12 &arg12, const ARG_TYPE_13 &arg13
#define	FUNC_PROXY_ARG_FORMAL_PARAMS_13		const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3, const ARG_TYPE_4 &arg4, const ARG_TYPE_5 &arg5,	const ARG_TYPE_6 &arg6,	const ARG_TYPE_7 &arg7, const ARG_TYPE_8 &arg8, const ARG_TYPE_9 &arg9, const ARG_TYPE_10 &arg10, const ARG_TYPE_11 &arg11, const ARG_TYPE_12 &arg12, const ARG_TYPE_13 &arg13
#define	FUNC_CALL_ARGS_INIT_13				, m_arg1( arg1 ), m_arg2( arg2 ), m_arg3( arg3 ), m_arg4( arg4 ), m_arg5( arg5 ), m_arg6( arg6 ), m_arg7( arg7 ), m_arg8( arg8 ), m_arg9( arg9 ), m_arg10( arg10 ), m_arg11( arg11 ), m_arg12( arg12 ), m_arg13( arg13 )
#define	FUNC_CALL_MEMBER_ARGS_13			m_arg1, m_arg2, m_arg3, m_arg4, m_arg5, m_arg6, m_arg7, m_arg8, m_arg9, m_arg10, m_arg11, m_arg12, m_arg13
#define	FUNC_CALL_ARGS_13					arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13
#define	FUNC_FUNCTOR_CALL_ARGS_13			, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13
#define	FUNC_TEMPLATE_FUNC_PARAMS_13		, typename FUNC_ARG_TYPE_1, typename	FUNC_ARG_TYPE_2, typename FUNC_ARG_TYPE_3, typename	FUNC_ARG_TYPE_4, typename FUNC_ARG_TYPE_5, typename	FUNC_ARG_TYPE_6, typename FUNC_ARG_TYPE_7, typename FUNC_ARG_TYPE_8, typename	FUNC_ARG_TYPE_9, typename FUNC_ARG_TYPE_10, typename FUNC_ARG_TYPE_11, typename FUNC_ARG_TYPE_12, typename FUNC_ARG_TYPE_13
#define	FUNC_BASE_TEMPLATE_FUNC_PARAMS_13	FUNC_ARG_TYPE_1, FUNC_ARG_TYPE_2, FUNC_ARG_TYPE_3,	FUNC_ARG_TYPE_4, FUNC_ARG_TYPE_5, FUNC_ARG_TYPE_6, FUNC_ARG_TYPE_7, FUNC_ARG_TYPE_8, FUNC_ARG_TYPE_9, FUNC_ARG_TYPE_10, FUNC_ARG_TYPE_11, FUNC_ARG_TYPE_12, FUNC_ARG_TYPE_13
#define	FUNC_ADDL_TEMPLATE_FUNC_PARAMS_13	, FUNC_ARG_TYPE_1, FUNC_ARG_TYPE_2, FUNC_ARG_TYPE_3,	FUNC_ARG_TYPE_4, FUNC_ARG_TYPE_5, FUNC_ARG_TYPE_6, FUNC_ARG_TYPE_7, FUNC_ARG_TYPE_8, FUNC_ARG_TYPE_9, FUNC_ARG_TYPE_10, FUNC_ARG_TYPE_11, FUNC_ARG_TYPE_12, FUNC_ARG_TYPE_13

#define	FUNC_TEMPLATE_ARG_PARAMS_14			, typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9, typename ARG_TYPE_10, typename ARG_TYPE_11, typename ARG_TYPE_12, typename ARG_TYPE_13, typename ARG_TYPE_14
#define	FUNC_BASE_TEMPLATE_ARG_PARAMS_14	, ARG_TYPE_1, ARG_TYPE_2, ARG_TYPE_3, ARG_TYPE_4, ARG_TYPE_5, ARG_TYPE_6, ARG_TYPE_7, ARG_TYPE_8, ARG_TYPE_9, ARG_TYPE_10, ARG_TYPE_11, ARG_TYPE_12, ARG_TYPE_13, ARG_TYPE_14
#define	FUNC_ARG_MEMBERS_14					ARG_TYPE_1 m_arg1;	ARG_TYPE_2 m_arg2; ARG_TYPE_3 m_arg3; ARG_TYPE_4 m_arg4; ARG_TYPE_5 m_arg5;	ARG_TYPE_6 m_arg6;	ARG_TYPE_7 m_arg7; ARG_TYPE_8 m_arg8; ARG_TYPE_9 m_arg9; ARG_TYPE_10 m_arg10; ARG_TYPE_11 m_arg11; ARG_TYPE_12 m_arg12; ARG_TYPE_13 m_arg13; ARG_TYPE_14 m_arg14
#define	FUNC_ARG_FORMAL_PARAMS_14			, const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3, const ARG_TYPE_4 &arg4, const ARG_TYPE_5 &arg5,	const ARG_TYPE_6 &arg6,	const ARG_TYPE_7 &arg7, const ARG_TYPE_8 &arg8, const ARG_TYPE_9 &arg9, const ARG_TYPE_10 &arg10, const ARG_TYPE_11 &arg11, const ARG_TYPE_12 &arg12, const ARG_TYPE_13 &arg13, const ARG_TYPE_14 &arg14
#define	FUNC_PROXY_ARG_FORMAL_PARAMS_14		const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3, const ARG_TYPE_4 &arg4, const ARG_TYPE_5 &arg5,	const ARG_TYPE_6 &arg6,	const ARG_TYPE_7 &arg7, const ARG_TYPE_8 &arg8, const ARG_TYPE_9 &arg9, const ARG_TYPE_10 &arg10, const ARG_TYPE_11 &arg11, const ARG_TYPE_12 &arg12, const ARG_TYPE_13 &arg13, const ARG_TYPE_14 &arg14
#define	FUNC_CALL_ARGS_INIT_14				, m_arg1( arg1 ), m_arg2( arg2 ), m_arg3( arg3 ), m_arg4( arg4 ), m_arg5( arg5 ), m_arg6( arg6 ), m_arg7( arg7 ), m_arg8( arg8 ), m_arg9( arg9 ), m_arg10( arg10 ), m_arg11( arg11 ), m_arg12( arg12 ), m_arg13( arg13 ), m_arg14( arg14 )
#define	FUNC_CALL_MEMBER_ARGS_14			m_arg1, m_arg2, m_arg3, m_arg4, m_arg5, m_arg6, m_arg7, m_arg8, m_arg9, m_arg10, m_arg11, m_arg12, m_arg13, m_arg14
#define	FUNC_CALL_ARGS_14					arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13, arg14
#define	FUNC_FUNCTOR_CALL_ARGS_14			, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13, arg14
#define	FUNC_TEMPLATE_FUNC_PARAMS_14		, typename FUNC_ARG_TYPE_1, typename	FUNC_ARG_TYPE_2, typename FUNC_ARG_TYPE_3, typename	FUNC_ARG_TYPE_4, typename FUNC_ARG_TYPE_5, typename	FUNC_ARG_TYPE_6, typename FUNC_ARG_TYPE_7, typename FUNC_ARG_TYPE_8, typename	FUNC_ARG_TYPE_9, typename FUNC_ARG_TYPE_10, typename FUNC_ARG_TYPE_11, typename FUNC_ARG_TYPE_12, typename FUNC_ARG_TYPE_13, typename FUNC_ARG_TYPE_14
#define	FUNC_BASE_TEMPLATE_FUNC_PARAMS_14	FUNC_ARG_TYPE_1, FUNC_ARG_TYPE_2, FUNC_ARG_TYPE_3,	FUNC_ARG_TYPE_4, FUNC_ARG_TYPE_5, FUNC_ARG_TYPE_6, FUNC_ARG_TYPE_7, FUNC_ARG_TYPE_8, FUNC_ARG_TYPE_9, FUNC_ARG_TYPE_10, FUNC_ARG_TYPE_11, FUNC_ARG_TYPE_12, FUNC_ARG_TYPE_13, FUNC_ARG_TYPE_14
#define	FUNC_ADDL_TEMPLATE_FUNC_PARAMS_14	, FUNC_ARG_TYPE_1, FUNC_ARG_TYPE_2, FUNC_ARG_TYPE_3,	FUNC_ARG_TYPE_4, FUNC_ARG_TYPE_5, FUNC_ARG_TYPE_6, FUNC_ARG_TYPE_7, FUNC_ARG_TYPE_8, FUNC_ARG_TYPE_9, FUNC_ARG_TYPE_10, FUNC_ARG_TYPE_11, FUNC_ARG_TYPE_12, FUNC_ARG_TYPE_13, FUNC_ARG_TYPE_14

#define FUNC_GENERATE_ALL( INNERMACRONAME ) \
	INNERMACRONAME(0); \
	INNERMACRONAME(1); \
	INNERMACRONAME(2); \
	INNERMACRONAME(3); \
	INNERMACRONAME(4); \
	INNERMACRONAME(5); \
	INNERMACRONAME(6); \
	INNERMACRONAME(7); \
	INNERMACRONAME(8); \
	INNERMACRONAME(9); \
	INNERMACRONAME(10);\
	INNERMACRONAME(11);\
	INNERMACRONAME(12);\
	INNERMACRONAME(13);\
	INNERMACRONAME(14)

//-----------------------------------------------------------------------------
//
// Purpose: Base class of all function objects
//
//-----------------------------------------------------------------------------

abstract_class CFunctor : public IRefCounted
{
public:
	CFunctor() = default;

	// Add a virtual destructor to silence the clang warning.
	// This is harmless but not important since the only derived class
	// doesn't have a destructor.
	virtual ~CFunctor() = default;

	virtual void operator()() = 0;

	unsigned m_nUserID{0}; // For debugging
};


//-----------------------------------------------------------------------------
// When calling through a functor, care needs to be taken to not pass objects that might go away. 
// Since this code determines the type to store in the functor based on the actual arguments,
// this is achieved by changing the point of call. 
//
// See also CUtlEnvelope
//-----------------------------------------------------------------------------

// convert a reference to a passable value
template <typename T>
[[nodiscard]] constexpr inline T RefToVal( const T& item )
{
   return item;
}

//-----------------------------------------------------------------------------
// This class can be used to pass into a functor a proxy object for a pointer
// to be resolved later. For example, you can execute a "call" on a resource
// whose actual value is not known until a later time
//-----------------------------------------------------------------------------

template <typename T>
class CLateBoundPtr
{
public:
	CLateBoundPtr( T **ppObject )
		: m_ppObject( ppObject )
	{
		Assert( ppObject );
	}

	[[nodiscard]] T *operator->() { return *m_ppObject;	}
	[[nodiscard]] T &operator *() { return **m_ppObject;	}
	[[nodiscard]] operator T *() const { return *m_ppObject;	}
	[[nodiscard]] operator void *() { return *m_ppObject;	}

	[[nodiscard]] bool operator ==(std::nullptr_t) const
	{
		return *m_ppObject == nullptr;
	}
	[[nodiscard]] bool operator !=(std::nullptr_t) const
	{
		return !(*this == nullptr);
	}

private:
	T **m_ppObject;
};

//-----------------------------------------------------------------------------
//
// Purpose: Classes to define memory management policies when operating
//			on pointers to members
//
//-----------------------------------------------------------------------------

class CFuncMemPolicyNone
{
public:
	static void OnAcquire(void *)	{}
	static void OnRelease(void *)	{}
};

template <typename OBJECT_TYPE_PTR = IRefCounted*>
class CFuncMemPolicyRefCount
{
public:
	static void OnAcquire( OBJECT_TYPE_PTR pObject ) { pObject->AddRef(); }
	static void OnRelease( OBJECT_TYPE_PTR pObject ) { pObject->Release(); }
};

//-----------------------------------------------------------------------------
//
// The actual functor implementation
//
//-----------------------------------------------------------------------------

#include "tier0/memdbgon.h"
#include <tuple>

using CFunctorBase = CRefCounted1<CFunctor, CRefCountServiceMT>;

template <typename FUNC_TYPE, typename FUNCTOR_BASE = CFunctorBase, typename... Args>
class CFunctorI : public FUNCTOR_BASE
{
public:
	CFunctorI( FUNC_TYPE pfnProxied, Args&&... args )
		: m_pfnProxied( pfnProxied ), m_params( std::forward<Args>( args )... ) {}
	~CFunctorI() = default;
	void operator()() override { apply(); }

private:
	template<size_t... S>
	FORCEINLINE void apply( std::index_sequence<S...> )
	{
		m_pfnProxied( std::get<S>( m_params )... );
	}

	FORCEINLINE void apply()
	{
		apply( std::make_index_sequence<sizeof...( Args )>() );
	}
	
	FUNC_TYPE m_pfnProxied;
	std::tuple<std::decay_t<Args>...> m_params;
};

template <
	typename OBJECT_TYPE_PTR,
	typename FUNCTION_TYPE,
	typename FUNCTOR_BASE = CFunctorBase,
	typename MEM_POLICY = CFuncMemPolicyNone,
	typename... Args>
class CMemberFunctorI : public FUNCTOR_BASE
{
public:
	CMemberFunctorI( OBJECT_TYPE_PTR pObject, FUNCTION_TYPE pfnProxied, Args&&... args )
		: m_Proxy( pObject, pfnProxied ), m_params( std::forward<Args>( args )... ) {}
	~CMemberFunctorI() = default;
	void operator()() override { apply(); }

private:
	template<size_t... S>
	FORCEINLINE void apply( std::index_sequence<S...> )
	{
		m_Proxy( std::get<S>( m_params )... );
	}

	FORCEINLINE void apply()
	{
		apply( std::make_index_sequence<sizeof...( Args )>() );
	}

	class CMemberFuncProxy
	{
	public:
		CMemberFuncProxy( OBJECT_TYPE_PTR pObject, FUNCTION_TYPE pfnProxied )
			: m_pfnProxied( pfnProxied ), m_pObject( pObject )
		{
			MEM_POLICY::OnAcquire( m_pObject );
		}

		~CMemberFuncProxy()
		{
			MEM_POLICY::OnRelease( m_pObject );
		}

		void operator()( const Args&... args )
		{
			Assert( m_pObject != nullptr );
			( ( *m_pObject ).*m_pfnProxied )( args... );
		}

		FUNCTION_TYPE m_pfnProxied;
		OBJECT_TYPE_PTR m_pObject;
	};

	CMemberFuncProxy m_Proxy;
	std::tuple<std::decay_t<Args>...> m_params;
};

//-----------------------------------------------------------------------------
//
// The real magic, letting the compiler figure out all the right template parameters
//
//-----------------------------------------------------------------------------

template <
	typename FUNCTION_RETTYPE,
	typename... FuncArgs,
	typename... Args>
inline auto CreateFunctor( FUNCTION_RETTYPE( *pfnProxied )( FuncArgs... ), Args&&... args )	->
	std::enable_if_t<
		std::is_invocable_r_v<
			FUNCTION_RETTYPE,
			decltype(pfnProxied),
			Args...
		>,
		CFunctor*
	>
{
	using Func_t = FUNCTION_RETTYPE( * )( FuncArgs... );
	return new CFunctorI<Func_t, CFunctorBase, Args...>( pfnProxied, std::forward<Args>( args )... );
}

template <
	typename OBJECT_TYPE_PTR,
	typename FUNCTION_CLASS,
	typename FUNCTION_RETTYPE,
	typename... FuncArgs,
	typename... Args>
inline auto CreateFunctor(
	OBJECT_TYPE_PTR pObject,
	FUNCTION_RETTYPE( FUNCTION_CLASS::* pfnProxied )( FuncArgs... ),
	Args&&... args ) ->
	std::enable_if_t<
		std::is_base_of_v<FUNCTION_CLASS, std::remove_pointer_t<OBJECT_TYPE_PTR>>
		&&
		std::is_invocable_r_v<
			FUNCTION_RETTYPE,
			decltype(pfnProxied),
			FUNCTION_CLASS&,
			Args...
		>,
		CFunctor*
	>
{
	using Func_t = FUNCTION_RETTYPE( FUNCTION_CLASS::* )( FuncArgs... );
	return new CMemberFunctorI<OBJECT_TYPE_PTR, Func_t, CFunctorBase, CFuncMemPolicyNone, Args...>( pObject, pfnProxied, std::forward<Args>( args )... );
}

template <
	typename OBJECT_TYPE_PTR,
	typename FUNCTION_CLASS,
	typename FUNCTION_RETTYPE,
	typename... FuncArgs,
	typename... Args>
inline auto CreateFunctor(
	OBJECT_TYPE_PTR pObject,
	FUNCTION_RETTYPE( FUNCTION_CLASS::* pfnProxied )( FuncArgs... ) const,
	Args&&... args ) ->
	std::enable_if_t<
		std::is_base_of_v<FUNCTION_CLASS, std::remove_pointer_t<OBJECT_TYPE_PTR>>
		&&
		std::is_invocable_r_v<
			FUNCTION_RETTYPE,
			decltype(pfnProxied),
			const FUNCTION_CLASS&,
			Args...
		>,
		CFunctor*
	>
{
	using Func_t = FUNCTION_RETTYPE( FUNCTION_CLASS::* )( FuncArgs... ) const;
	return new CMemberFunctorI<OBJECT_TYPE_PTR, Func_t, CFunctorBase, CFuncMemPolicyNone, Args...>( pObject, pfnProxied, std::forward<Args>( args )... );
}

template <
	typename OBJECT_TYPE_PTR,
	typename FUNCTION_CLASS,
	typename FUNCTION_RETTYPE,
	typename... FuncArgs,
	typename... Args>
inline auto CreateRefCountingFunctor(
	OBJECT_TYPE_PTR pObject,
	FUNCTION_RETTYPE( FUNCTION_CLASS::* pfnProxied )( FuncArgs... ),
	Args&&... args ) ->
	std::enable_if_t<
		std::is_base_of_v<FUNCTION_CLASS, std::remove_pointer_t<OBJECT_TYPE_PTR>>
		&&
		std::is_invocable_r_v<
			FUNCTION_RETTYPE,
			decltype(pfnProxied),
			FUNCTION_CLASS&,
			Args...
		>,
		CFunctor*
	>
{
	using Func_t = FUNCTION_RETTYPE( FUNCTION_CLASS::* )( FuncArgs... );
	return new CMemberFunctorI<OBJECT_TYPE_PTR, Func_t, CFunctorBase, CFuncMemPolicyRefCount<OBJECT_TYPE_PTR>, Args...>( pObject, pfnProxied, std::forward<Args>( args )... );
}

template <
	typename OBJECT_TYPE_PTR,
	typename FUNCTION_CLASS,
	typename FUNCTION_RETTYPE,
	typename... FuncArgs,
	typename... Args>
inline auto CreateRefCountingFunctor(
	OBJECT_TYPE_PTR pObject,
	FUNCTION_RETTYPE( FUNCTION_CLASS::* pfnProxied )( FuncArgs... ) const,
	Args&&... args ) ->
	std::enable_if_t<
		std::is_base_of_v<FUNCTION_CLASS, std::remove_pointer_t<OBJECT_TYPE_PTR>>
		&&
		std::is_invocable_r_v<
			FUNCTION_RETTYPE,
			decltype(pfnProxied),
			const FUNCTION_CLASS&,
			Args...
		>,
		CFunctor*
	>
{
	using Func_t = FUNCTION_RETTYPE( FUNCTION_CLASS::* )( FuncArgs... ) const;
	return new CMemberFunctorI<OBJECT_TYPE_PTR, Func_t, CFunctorBase, CFuncMemPolicyRefCount<OBJECT_TYPE_PTR>, Args...>( pObject, pfnProxied, std::forward<Args>( args )... );
}

//-----------------------------------------------------------------------------
//
// Templates to assist early-out direct call code
//
//-----------------------------------------------------------------------------

template <typename FUNCTION_RETTYPE, typename... FuncArgs, typename... Args>
inline auto FunctorDirectCall( FUNCTION_RETTYPE( *pfnProxied )( FuncArgs... ), Args&&... args ) ->
	std::enable_if_t<
		std::is_invocable_r_v<
			FUNCTION_RETTYPE,
			decltype(pfnProxied),
			Args...
		>,
		CFunctor*
	>
{
	( *pfnProxied )( std::forward<Args>( args )... );
}

template <
	typename OBJECT_TYPE_PTR,
	typename FUNCTION_CLASS,
	typename FUNCTION_RETTYPE,
	typename... FuncArgs,
	typename... Args>
inline auto FunctorDirectCall(
	OBJECT_TYPE_PTR pObject,
	FUNCTION_RETTYPE( FUNCTION_CLASS::* pfnProxied )( FuncArgs... ),
	Args&&... args ) ->
	std::enable_if_t<
		std::is_base_of_v<FUNCTION_CLASS, std::remove_pointer_t<OBJECT_TYPE_PTR>>
		&&
		std::is_invocable_r_v<
			FUNCTION_RETTYPE,
			decltype(pfnProxied),
			FUNCTION_CLASS&,
			Args...
		>
	>
{
	( ( *pObject ).*pfnProxied )( std::forward<Args>( args )... );
}

template <
	typename OBJECT_TYPE_PTR,
	typename FUNCTION_CLASS,
	typename FUNCTION_RETTYPE,
	typename... FuncArgs,
	typename... Args>
inline auto FunctorDirectCall(
	OBJECT_TYPE_PTR pObject,
	FUNCTION_RETTYPE( FUNCTION_CLASS::* pfnProxied )( FuncArgs... ) const,
	Args&&... args ) ->
	std::enable_if_t<
		std::is_base_of_v<FUNCTION_CLASS, std::remove_pointer_t<OBJECT_TYPE_PTR>>
		&&
		std::is_invocable_r_v<
			FUNCTION_RETTYPE,
			decltype(pfnProxied),
			const FUNCTION_CLASS&,
			Args...
		>
	>
{
	( ( *pObject ).*pfnProxied )( std::forward<Args>( args )... );
}

#include "tier0/memdbgoff.h"

//-----------------------------------------------------------------------------
// Factory class useable as templated traits
//-----------------------------------------------------------------------------

class CDefaultFunctorFactory
{
public:
	template <typename FUNCTION_RETTYPE, typename... FuncArgs, typename... Args>
	inline auto CreateFunctor( FUNCTION_RETTYPE( *pfnProxied )( FuncArgs... ), Args&&... args ) ->
		std::enable_if_t<
			std::is_invocable_r_v<
				FUNCTION_RETTYPE,
				decltype(pfnProxied),
				Args...
			>,
			CFunctor*
		>
	{
		using Func_t = FUNCTION_RETTYPE(*)( FuncArgs... );
		return new CFunctorI<Func_t, CFunctorBase, Args...>( pfnProxied, std::forward<Args>( args )... );
	}

	template <
		typename OBJECT_TYPE_PTR,
		typename FUNCTION_CLASS,
		typename FUNCTION_RETTYPE,
		typename... FuncArgs,
		typename... Args>
	inline auto CreateFunctor(
		OBJECT_TYPE_PTR pObject,
		FUNCTION_RETTYPE( FUNCTION_CLASS::* pfnProxied )( FuncArgs... ),
		Args&&... args ) ->
		std::enable_if_t<
			std::is_base_of_v<FUNCTION_CLASS, std::remove_pointer_t<OBJECT_TYPE_PTR>>
			&&
			std::is_invocable_r_v<
				FUNCTION_RETTYPE,
				decltype(pfnProxied),
				FUNCTION_CLASS&,
				Args...
			>,
			CFunctor*
		>
	{
		using Func_t = FUNCTION_RETTYPE(FUNCTION_CLASS::*)( FuncArgs... );
		return new CMemberFunctorI<OBJECT_TYPE_PTR, Func_t, CFunctorBase, CFuncMemPolicyNone, Args...>( pObject, pfnProxied, std::forward<Args>( args )... );
	}

	template <
		typename OBJECT_TYPE_PTR,
		typename FUNCTION_CLASS,
		typename FUNCTION_RETTYPE,
		typename... FuncArgs,
		typename... Args>
	inline auto CreateFunctor(
		OBJECT_TYPE_PTR pObject,
		FUNCTION_RETTYPE( FUNCTION_CLASS::* pfnProxied )( FuncArgs... ) const,
		Args&&... args ) ->
		std::enable_if_t<
			std::is_base_of_v<FUNCTION_CLASS, std::remove_pointer_t<OBJECT_TYPE_PTR>>
			&&
			std::is_invocable_r_v<
				FUNCTION_RETTYPE,
				decltype(pfnProxied),
				const FUNCTION_CLASS&,
				Args...
			>,
			CFunctor*
		>
	{
		using Func_t = FUNCTION_RETTYPE(FUNCTION_CLASS::*)( FuncArgs... ) const;
		return new CMemberFunctorI<OBJECT_TYPE_PTR, Func_t, CFunctorBase, CFuncMemPolicyNone, Args...>( pObject, pfnProxied, std::forward<Args>( args )... );
	}

	template <
		typename OBJECT_TYPE_PTR,
		typename FUNCTION_CLASS,
		typename FUNCTION_RETTYPE,
		typename... FuncArgs,
		typename... Args>
	inline auto CreateRefCountingFunctor(
		OBJECT_TYPE_PTR pObject,
		FUNCTION_RETTYPE( FUNCTION_CLASS::* pfnProxied )( FuncArgs... ),
		Args&&... args ) ->
		std::enable_if_t<
			std::is_base_of_v<FUNCTION_CLASS, std::remove_pointer_t<OBJECT_TYPE_PTR>>
			&&
			std::is_invocable_r_v<
				FUNCTION_RETTYPE,
				decltype(pfnProxied),
				FUNCTION_CLASS&,
				Args...
			>,
			CFunctor*
		>
	{
		using Func_t = FUNCTION_RETTYPE(FUNCTION_CLASS::*)( FuncArgs... );
		return new CMemberFunctorI<OBJECT_TYPE_PTR, Func_t, CFunctorBase, CFuncMemPolicyRefCount<OBJECT_TYPE_PTR>, Args...>( pObject, pfnProxied, std::forward<Args>( args )... );
	}

	template <
		typename OBJECT_TYPE_PTR,
		typename FUNCTION_CLASS,
		typename FUNCTION_RETTYPE,
		typename... FuncArgs,
		typename... Args>
	inline auto CreateRefCountingFunctor(
		OBJECT_TYPE_PTR pObject,
		FUNCTION_RETTYPE( FUNCTION_CLASS::* pfnProxied )( FuncArgs... ) const,
		Args&&... args ) ->
		std::enable_if_t<
			std::is_base_of_v<FUNCTION_CLASS, std::remove_pointer_t<OBJECT_TYPE_PTR>>
			&&
			std::is_invocable_r_v<
				FUNCTION_RETTYPE,
				decltype(pfnProxied),
				const FUNCTION_CLASS&,
				Args...
			>,
			CFunctor*
		>
	{
		using Func_t = FUNCTION_RETTYPE(FUNCTION_CLASS::*)( FuncArgs... ) const;
		return new CMemberFunctorI<OBJECT_TYPE_PTR, Func_t, CFunctorBase, CFuncMemPolicyRefCount<OBJECT_TYPE_PTR>, Args...>( pObject, pfnProxied, std::forward<Args>( args )... );
	}
};

template <typename CAllocator, typename CCustomFunctorBase = CFunctorBase>
class CCustomizedFunctorFactory
{
public:
	void SetAllocator( CAllocator *pAllocator )
	{
		m_pAllocator = pAllocator;
	}
	
	template <typename FUNCTION_RETTYPE, typename... FuncArgs, typename... Args>
	inline auto CreateFunctor( FUNCTION_RETTYPE (*pfnProxied)( FuncArgs... ), Args&&... args ) ->
		std::enable_if_t<
			std::is_invocable_r_v<
				FUNCTION_RETTYPE,
				decltype(pfnProxied),
				Args...
			>,
			CFunctor*
		>
	{
		using Func_t = FUNCTION_RETTYPE( * )( FuncArgs... );
		using Functor_t = CFunctorI<Func_t, CCustomFunctorBase, Args...>;
		return new ( m_pAllocator->Alloc( sizeof( Functor_t ) ) ) Functor_t( pfnProxied, std::forward<Args>( args )... );
	}

	//-------------------------------------

	template <
		typename OBJECT_TYPE_PTR,
		typename FUNCTION_CLASS,
		typename FUNCTION_RETTYPE,
		typename... FuncArgs,
		typename... Args>
	inline auto CreateFunctor(
		OBJECT_TYPE_PTR pObject,
		FUNCTION_RETTYPE ( FUNCTION_CLASS::*pfnProxied )( FuncArgs... ),
		Args&&... args ) ->
		std::enable_if_t<
			std::is_base_of_v<FUNCTION_CLASS, std::remove_pointer_t<OBJECT_TYPE_PTR>>
			&&
			std::is_invocable_r_v<
				FUNCTION_RETTYPE,
				decltype(pfnProxied),
				FUNCTION_CLASS&,
				Args...
			>,
			CFunctor*
		>
	{
		using Func_t = FUNCTION_RETTYPE( FUNCTION_CLASS::* )( FuncArgs... );
		using Functor_t = CMemberFunctorI<OBJECT_TYPE_PTR, Func_t, CCustomFunctorBase, CFuncMemPolicyNone, Args...>;
		return new ( m_pAllocator->Alloc( sizeof( Functor_t ) ) ) Functor_t( pObject, pfnProxied, std::forward<Args>( args )... );
	}

	//-------------------------------------

	template <
		typename OBJECT_TYPE_PTR,
		typename FUNCTION_CLASS,
		typename FUNCTION_RETTYPE,
		typename... FuncArgs,
		typename... Args>
	inline auto CreateFunctor(
		OBJECT_TYPE_PTR pObject,
		FUNCTION_RETTYPE ( FUNCTION_CLASS::*pfnProxied )( FuncArgs... ) const,
		Args&&... args ) ->
		std::enable_if_t<
			std::is_base_of_v<FUNCTION_CLASS, std::remove_pointer_t<OBJECT_TYPE_PTR>>
			&&
			std::is_invocable_r_v<
				FUNCTION_RETTYPE,
				decltype(pfnProxied),
				const FUNCTION_CLASS&,
				Args...
			>,
			CFunctor*
		>
	{
		using Func_t = FUNCTION_RETTYPE( FUNCTION_CLASS::* )( FuncArgs... ) const;
		using Functor_t = CMemberFunctorI<OBJECT_TYPE_PTR, Func_t, CCustomFunctorBase, CFuncMemPolicyNone, Args...>;
		return new ( m_pAllocator->Alloc( sizeof( Functor_t ) ) ) Functor_t( pObject, pfnProxied, std::forward<Args>( args )... );
	}

	//-------------------------------------

	template <
		typename OBJECT_TYPE_PTR,
		typename FUNCTION_CLASS,
		typename FUNCTION_RETTYPE,
		typename... FuncArgs,
		typename... Args>
	inline auto CreateFunctor(
		CLateBoundPtr<OBJECT_TYPE_PTR> pObject,
		FUNCTION_RETTYPE ( FUNCTION_CLASS::*pfnProxied )( FuncArgs... ),
		Args&&... args ) ->
		std::enable_if_t<
			std::is_base_of_v<FUNCTION_CLASS, std::remove_pointer_t<OBJECT_TYPE_PTR>>
			&&
			std::is_invocable_r_v<
				FUNCTION_RETTYPE,
				decltype(pfnProxied),
				FUNCTION_CLASS&,
				Args...
			>,
			CFunctor*
		>
	{
		using Func_t = FUNCTION_RETTYPE( FUNCTION_CLASS::* )( FuncArgs... );
		using Functor_t = CMemberFunctorI<CLateBoundPtr<OBJECT_TYPE_PTR>, Func_t, CCustomFunctorBase, CFuncMemPolicyNone, Args...>;
		return new ( m_pAllocator->Alloc( sizeof( Functor_t ) ) ) Functor_t( pObject, pfnProxied, std::forward<Args>( args )... );
	}

	//-------------------------------------

	template <
		typename OBJECT_TYPE_PTR,
		typename FUNCTION_CLASS,
		typename FUNCTION_RETTYPE,
		typename... FuncArgs,
		typename... Args>
	inline auto CreateFunctor(
		CLateBoundPtr<OBJECT_TYPE_PTR> pObject,
		FUNCTION_RETTYPE ( FUNCTION_CLASS::*pfnProxied )( FuncArgs... ) const,
		Args&&... args ) ->
		std::enable_if_t<
			std::is_base_of_v<FUNCTION_CLASS, std::remove_pointer_t<OBJECT_TYPE_PTR>>
			&&
			std::is_invocable_r_v<
				FUNCTION_RETTYPE,
				decltype(pfnProxied),
				const FUNCTION_CLASS&,
				Args...
			>,
			CFunctor*
		>
	{
		using Func_t = FUNCTION_RETTYPE( FUNCTION_CLASS::* )( FuncArgs... ) const;
		using Functor_t = CMemberFunctorI<CLateBoundPtr<OBJECT_TYPE_PTR>, Func_t, CCustomFunctorBase, CFuncMemPolicyNone, Args...>;
		return new ( m_pAllocator->Alloc( sizeof( Functor_t ) ) ) Functor_t( pObject, pfnProxied, std::forward<Args>( args )... );
	}

	//-------------------------------------

	template <typename OBJECT_TYPE_PTR, typename FUNCTION_CLASS, typename FUNCTION_RETTYPE, typename... FuncArgs, typename... Args>
	inline auto CreateRefCountingFunctor(
		OBJECT_TYPE_PTR pObject,
		FUNCTION_RETTYPE ( FUNCTION_CLASS::*pfnProxied )( FuncArgs... ),
		Args&&... args ) ->
		std::enable_if_t<
			std::is_base_of_v<FUNCTION_CLASS, std::remove_pointer_t<OBJECT_TYPE_PTR>>
			&&
			std::is_invocable_r_v<
				FUNCTION_RETTYPE,
				decltype(pfnProxied),
				FUNCTION_CLASS&,
				Args...
			>,
			CFunctor*
		>
	{
		using Func_t = FUNCTION_RETTYPE( FUNCTION_CLASS::* )( FuncArgs... );
		using Functor_t = CMemberFunctorI<OBJECT_TYPE_PTR, Func_t, CCustomFunctorBase, CFuncMemPolicyRefCount<OBJECT_TYPE_PTR>, Args...>;
		return new ( m_pAllocator->Alloc( sizeof( Functor_t ) ) ) Functor_t( pObject, pfnProxied, std::forward<Args>( args )... );
	}

	//-------------------------------------

	template <
		typename OBJECT_TYPE_PTR,
		typename FUNCTION_CLASS,
		typename FUNCTION_RETTYPE,
		typename... FuncArgs,
		typename... Args>
	inline auto CreateRefCountingFunctor(
		OBJECT_TYPE_PTR pObject,
		FUNCTION_RETTYPE ( FUNCTION_CLASS::*pfnProxied )( FuncArgs... ) const,
		Args&&... args ) ->
		std::enable_if_t<
			std::is_base_of_v<FUNCTION_CLASS, std::remove_pointer_t<OBJECT_TYPE_PTR>>
			&&
			std::is_invocable_r_v<
				FUNCTION_RETTYPE,
				decltype(pfnProxied),
				const FUNCTION_CLASS&,
				Args...
			>,
			CFunctor*
		>
	{
		using Func_t = FUNCTION_RETTYPE( FUNCTION_CLASS::* )( FuncArgs... ) const;
		using Functor_t = CMemberFunctorI<OBJECT_TYPE_PTR, Func_t, CCustomFunctorBase, CFuncMemPolicyRefCount<OBJECT_TYPE_PTR>, Args...>;
		return new ( m_pAllocator->Alloc( sizeof( Functor_t ) ) ) Functor_t( pObject, pfnProxied, std::forward<Args>( args )... );
	}

	//-------------------------------------

	template <typename OBJECT_TYPE_PTR, typename FUNCTION_CLASS, typename FUNCTION_RETTYPE, typename... FuncArgs, typename... Args>
	inline auto CreateRefCountingFunctor(
		CLateBoundPtr<OBJECT_TYPE_PTR> pObject,
		FUNCTION_RETTYPE ( FUNCTION_CLASS::*pfnProxied )( FuncArgs... ),
		Args&&... args ) ->
		std::enable_if_t<
			std::is_base_of_v<FUNCTION_CLASS, std::remove_pointer_t<OBJECT_TYPE_PTR>>
			&&
			std::is_invocable_r_v<
				FUNCTION_RETTYPE,
				decltype(pfnProxied),
				FUNCTION_CLASS&,
				Args...
			>,
			CFunctor*
		>
	{
		using Func_t = FUNCTION_RETTYPE( FUNCTION_CLASS::* )( FuncArgs... );
		using Functor_t = CMemberFunctorI<CLateBoundPtr<OBJECT_TYPE_PTR>, Func_t, CCustomFunctorBase, CFuncMemPolicyRefCount<OBJECT_TYPE_PTR>, Args...>;
		return new ( m_pAllocator->Alloc( sizeof( Functor_t ) ) ) Functor_t( pObject, pfnProxied, std::forward<Args>( args )... );
	}

	//-------------------------------------

	template <
		typename OBJECT_TYPE_PTR,
		typename FUNCTION_CLASS,
		typename FUNCTION_RETTYPE,
		typename... FuncArgs,
		typename... Args>
	inline auto CreateRefCountingFunctor(
		CLateBoundPtr<OBJECT_TYPE_PTR> pObject,
		FUNCTION_RETTYPE ( FUNCTION_CLASS::*pfnProxied )( FuncArgs... ) const,
		Args&&... args ) ->
		std::enable_if_t<
			std::is_base_of_v<FUNCTION_CLASS, std::remove_pointer_t<OBJECT_TYPE_PTR>>
			&&
			std::is_invocable_r_v<
				FUNCTION_RETTYPE,
				decltype(pfnProxied),
				const FUNCTION_CLASS&,
				Args...
			>,
			CFunctor*
		>
	{
		using Func_t = FUNCTION_RETTYPE( FUNCTION_CLASS::* )( FuncArgs... ) const;
		using Functor_t = CMemberFunctorI<CLateBoundPtr<OBJECT_TYPE_PTR>, Func_t, CCustomFunctorBase, CFuncMemPolicyRefCount<OBJECT_TYPE_PTR>, Args...>;
		return new ( m_pAllocator->Alloc( sizeof( Functor_t ) ) ) Functor_t( pObject, pfnProxied, std::forward<Args>( args )... );
	}

	//-------------------------------------

private:
	CAllocator* m_pAllocator;
	
};

//-----------------------------------------------------------------------------

#endif // FUNCTORS_H