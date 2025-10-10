//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================

#ifndef CALLQUEUE_H
#define CALLQUEUE_H

#include "tier0/tslist.h"
#include "functors.h"

//-----------------------------------------------------

template <typename QUEUE_TYPE = CTSQueue<CFunctor*>>
class CCallQueueT
{
public:
	CCallQueueT()
		: m_bNoQueue{ false }
#ifdef _DEBUG
		, m_nCurSerialNumber{ 0 }
		, m_nBreakSerialNumber{ std::numeric_limits<unsigned>::max() }
#endif
	{
	}

	void DisableQueue( bool bDisable )
	{
		if ( m_bNoQueue == bDisable )
		{
			return;
		}

		if ( !m_bNoQueue )
			CallQueued();

		m_bNoQueue = bDisable;
	}

	[[nodiscard]] bool IsDisabled() const
	{
		return m_bNoQueue;
	}

	[[nodiscard]] int Count() const
	{
		return m_queue.Count();
	}

	void CallQueued()
	{
		if ( !m_queue.Count() )
		{
			return;
		}

		m_queue.PushItem( nullptr );

		CFunctor *pFunctor;

		while ( m_queue.PopItem( &pFunctor ) && pFunctor != nullptr )
		{
#ifdef _DEBUG
			if ( pFunctor->m_nUserID == m_nBreakSerialNumber)
			{
				m_nBreakSerialNumber = std::numeric_limits<unsigned>::max();
			}
#endif
			(*pFunctor)();
			pFunctor->Release();
		}

	}

	void QueueFunctor( CFunctor *pFunctor )
	{
		Assert( pFunctor );
		QueueFunctorInternal( RetAddRef( pFunctor ) );
	}

	void Flush()
	{
		m_queue.PushItem( nullptr );

		CFunctor *pFunctor;

		while ( m_queue.PopItem( &pFunctor ) && pFunctor != nullptr )
		{
			pFunctor->Release();
		}
	}

	template <typename RetType, typename... FuncArgs, typename... Args>
	auto QueueCall( RetType( *pfnProxied )( FuncArgs... ), Args&&... args ) ->
		std::enable_if_t<
			std::is_invocable_r_v<
				RetType,
				decltype(pfnProxied),
				Args...
			>
		>
	{
		QueueFunctorInternal( CreateFunctor( pfnProxied, std::forward<Args>( args )... ) );
	}

	template <
		typename OBJECT_TYPE_PTR,
		typename FUNCTION_CLASS,
		typename FUNCTION_RETTYPE,
		typename... FuncArgs,
		typename... Args>
	auto QueueCall(
		OBJECT_TYPE_PTR* pObject,
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
		QueueFunctorInternal( CreateFunctor( pObject, pfnProxied, std::forward<Args>( args )... ) );
	}

	template <
		typename OBJECT_TYPE_PTR,
		typename FUNCTION_CLASS,
		typename FUNCTION_RETTYPE,
		typename... FuncArgs,
		typename... Args>
	auto QueueCall(
		OBJECT_TYPE_PTR* pObject,
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
		QueueFunctorInternal( CreateFunctor( pObject, pfnProxied, std::forward<Args>( args )... ) );
	}

	template <
		typename OBJECT_TYPE_PTR,
		typename FUNCTION_CLASS,
		typename FUNCTION_RETTYPE,
		typename... FuncArgs,
		typename... Args>
	auto QueueRefCall(
		OBJECT_TYPE_PTR* pObject,
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
		QueueFunctorInternal( CreateRefCountingFunctor( pObject, pfnProxied, std::forward<Args>( args )... ) );
	}

	template <
		typename OBJECT_TYPE_PTR,
		typename FUNCTION_CLASS,
		typename FUNCTION_RETTYPE,
		typename... FuncArgs,
		typename... Args>
	auto QueueRefCall(
		OBJECT_TYPE_PTR* pObject,
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
		QueueFunctorInternal( CreateRefCountingFunctor( pObject, pfnProxied, std::forward<Args>( args )... ) );
	}

private:
	void QueueFunctorInternal( CFunctor* pFunctor )
	{
		if ( !m_bNoQueue )
		{
#ifdef _DEBUG
			pFunctor->m_nUserID = m_nCurSerialNumber++;
#endif
			m_queue.PushItem( pFunctor );
		}
		else
		{
			(*pFunctor)();
			pFunctor->Release();
		}
	}

	QUEUE_TYPE m_queue;
	bool m_bNoQueue;
	unsigned m_nCurSerialNumber;
	unsigned m_nBreakSerialNumber;
};

class CCallQueue : public CCallQueueT<>
{
};

//-----------------------------------------------------
// Optional interface that can be bound to concrete CCallQueue
//-----------------------------------------------------

class ICallQueue
{
public:
	void QueueFunctor( CFunctor *pFunctor )
	{
		QueueFunctorInternal( RetAddRef( pFunctor ) );
	}

	template <typename RetType, typename... FuncArgs, typename... Args>
	auto QueueCall( RetType( *pfnProxied )( FuncArgs... ), Args&&... args ) ->
		std::enable_if_t<
			std::is_invocable_r_v<
				RetType,
				decltype(pfnProxied),
				Args...
			>
		>
	{
		QueueFunctorInternal( CreateFunctor( pfnProxied, std::forward<Args>( args )... ) );
	}

	template <
		typename OBJECT_TYPE_PTR,
		typename FUNCTION_CLASS,
		typename FUNCTION_RETTYPE,
		typename... FuncArgs,
		typename... Args>
	auto QueueCall(
		OBJECT_TYPE_PTR* pObject,
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
		QueueFunctorInternal( CreateFunctor( pObject, pfnProxied, std::forward<Args>( args )... ) );
	}

	template <
		typename OBJECT_TYPE_PTR,
		typename FUNCTION_CLASS,
		typename FUNCTION_RETTYPE,
		typename... FuncArgs,
		typename... Args>
	auto QueueCall(
		OBJECT_TYPE_PTR* pObject,
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
		QueueFunctorInternal( CreateFunctor( pObject, pfnProxied, std::forward<Args>( args )... ) );
	}

	template <
		typename OBJECT_TYPE_PTR,
		typename FUNCTION_CLASS,
		typename FUNCTION_RETTYPE,
		typename... FuncArgs,
		typename... Args>
	auto QueueRefCall(
		OBJECT_TYPE_PTR* pObject,
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
		QueueFunctorInternal( CreateRefCountingFunctor( pObject, pfnProxied, std::forward<Args>( args )... ) );
	}

	template <
		typename OBJECT_TYPE_PTR,
		typename FUNCTION_CLASS,
		typename FUNCTION_RETTYPE,
		typename... FuncArgs,
		typename... Args>
	auto QueueRefCall(
		OBJECT_TYPE_PTR* pObject,
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
		QueueFunctorInternal( CreateRefCountingFunctor( pObject, pfnProxied, std::forward<Args>( args )... ) );
	}

private:
	virtual void QueueFunctorInternal( CFunctor *pFunctor ) = 0;
};

#endif // CALLQUEUE_H