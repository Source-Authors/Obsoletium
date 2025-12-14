//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef IMATERIALSYSTEMINTERNAL_H
#define IMATERIALSYSTEMINTERNAL_H

#ifdef _WIN32
#pragma once
#endif

#include "materialsystem/imaterialsystem.h"
#include "tier1/callqueue.h"
#include "tier1/memstack.h"

class IMaterialInternal;

//-----------------------------------------------------------------------------
// Special call queue that knows (a) single threaded access, and (b) all
// functions called after last function added
//-----------------------------------------------------------------------------

class CMatCallQueue
{
public:
	CMatCallQueue()
	{
		MEM_ALLOC_CREDIT_( "CMatCallQueue.m_Allocator" );
#ifdef SWDS
		constexpr size_t size = 2u*1024;
#ifdef PLATFORM_64BITS
		// dimhotepus: x2 size on x86-64.
		if ( !m_Allocator.Init( size * 2, 0, 0, 8 ) )
#else
		if ( !m_Allocator.Init( size, 0, 0, 4 ) )
#endif
		{
			Error( "Material call queue allocator unable to allocate %zu virtual bytes.\n", size );
		}
#else
		constexpr size_t size = 8u*1024*1024;
#ifdef PLATFORM_64BITS
		// dimhotepus: x2 size on x86-64.
		if ( !m_Allocator.Init( size * 2, 64u*1024, 256u*1024, 8u ) )
#else
		if ( !m_Allocator.Init( size, 64u*1024, 256u*1024, 4u ) )
#endif
		{
			Error( "Material call queue allocator unable to allocate %zu virtual bytes.\n", size );
		}
#endif
		m_FunctorFactory.SetAllocator( &m_Allocator );
		m_pHead = m_pTail = nullptr;

#ifdef _DEBUG
		m_nCurSerialNumber = 0;
		m_nBreakSerialNumber = 0;
#endif
	}

	size_t GetMemoryUsed() const
	{
		return m_Allocator.GetUsed();
	}

	int Count() const
	{
		int i = 0;
		Elem_t *pCurrent = m_pHead;
		while ( pCurrent )
		{
			i++;
			pCurrent = pCurrent->pNext;
		}
		return i;
	}

	void CallQueued()
	{
		if ( !m_pHead )
		{
			return;
		}

		Elem_t *pCurrent = m_pHead;
		while ( pCurrent )
		{
			CFunctor *pFunctor = pCurrent->pFunctor;
#ifdef _DEBUG
			if ( pFunctor->m_nUserID == m_nBreakSerialNumber)
			{
				m_nBreakSerialNumber = std::numeric_limits<unsigned>::max();
			}
#endif
			(*pFunctor)();
			pFunctor->Release();
			pCurrent = pCurrent->pNext;
		}

		m_Allocator.FreeAll( false );
		m_pHead = m_pTail = nullptr;
	}

	void QueueFunctor( CFunctor *pFunctor )
	{
		Assert( pFunctor );
		QueueFunctorInternal( RetAddRef( pFunctor ) );
	}

	void Flush()
	{
		if ( !m_pHead )
		{
			return;
		}

		Elem_t *pCurrent = m_pHead;
		while ( pCurrent )
		{
			CFunctor *pFunctor = pCurrent->pFunctor;
			pFunctor->Release();
			pCurrent = pCurrent->pNext;
		}

		m_Allocator.FreeAll( false );
		m_pHead = m_pTail = nullptr;
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
		QueueFunctorInternal( m_FunctorFactory.CreateFunctor( pfnProxied, std::forward<Args>( args )... ) );
	}

	template <
		typename OBJECT_TYPE_PTR,
		typename FUNCTION_CLASS,
		typename FUNCTION_RETTYPE,
		typename... FuncArgs,
		typename... Args>
	auto QueueCall(
		OBJECT_TYPE_PTR pObject,
		FUNCTION_RETTYPE( FUNCTION_CLASS::* pfnProxied )( FuncArgs... ),
		Args&&... args
	) ->
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
		QueueFunctorInternal( m_FunctorFactory.CreateFunctor( pObject, pfnProxied, std::forward<Args>( args )... ) );
	}

	template <
		typename OBJECT_TYPE_PTR,
		typename FUNCTION_CLASS,
		typename FUNCTION_RETTYPE,
		typename... FuncArgs,
		typename... Args>
	auto QueueCall(
		OBJECT_TYPE_PTR pObject,
		FUNCTION_RETTYPE( FUNCTION_CLASS::* pfnProxied )( FuncArgs... ) const,
		Args&&... args
	) ->
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
		QueueFunctorInternal( m_FunctorFactory.CreateFunctor( pObject, pfnProxied, std::forward<Args>( args )... ) );
	}

	template <
		typename OBJECT_TYPE_PTR,
		typename FUNCTION_CLASS,
		typename FUNCTION_RETTYPE,
		typename... FuncArgs,
		typename... Args>
	auto QueueCall(
		CLateBoundPtr<OBJECT_TYPE_PTR> pObject,
		FUNCTION_RETTYPE( FUNCTION_CLASS::* pfnProxied )( FuncArgs... ),
		Args&&... args
	) ->
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
		QueueFunctorInternal( m_FunctorFactory.CreateFunctor( pObject, pfnProxied, std::forward<Args>( args )... ) );
	}

	template <
		typename OBJECT_TYPE_PTR,
		typename FUNCTION_CLASS,
		typename FUNCTION_RETTYPE,
		typename... FuncArgs,
		typename... Args>
	auto QueueCall(
		CLateBoundPtr<OBJECT_TYPE_PTR> pObject,
		FUNCTION_RETTYPE( FUNCTION_CLASS::* pfnProxied )( FuncArgs... ) const,
		Args&&... args
	) ->
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
		QueueFunctorInternal( m_FunctorFactory.CreateFunctor( pObject, pfnProxied, std::forward<Args>( args )... ) );
	}

private:
	void QueueFunctorInternal( CFunctor *pFunctor )
	{
#ifdef _DEBUG
		pFunctor->m_nUserID = m_nCurSerialNumber++;
#endif
		MEM_ALLOC_CREDIT_( "CMatCallQueue.m_Allocator" );
		Elem_t *pNew = (Elem_t *)m_Allocator.Alloc( sizeof(Elem_t) );
		if ( m_pTail )
		{
			m_pTail->pNext = pNew;
			m_pTail = pNew;
		}
		else
		{
			m_pHead = m_pTail = pNew;
		}

		pNew->pNext = nullptr;
		pNew->pFunctor = pFunctor;
	}

	struct Elem_t
	{
		Elem_t *pNext;
		CFunctor *pFunctor;
	};

	Elem_t *m_pHead;
	Elem_t *m_pTail;

	CMemoryStack m_Allocator;
	CCustomizedFunctorFactory<CMemoryStack, CRefCounted1<CFunctor, CRefCountServiceDestruct< CRefST > > > m_FunctorFactory;

#ifdef _DEBUG
	unsigned m_nCurSerialNumber;
	unsigned m_nBreakSerialNumber;
#endif
};


class IMaterialProxy;


//-----------------------------------------------------------------------------
// Additional interfaces used internally to the library
//-----------------------------------------------------------------------------
abstract_class IMaterialSystemInternal : public IMaterialSystem
{
public:
	// Returns the current material
	virtual IMaterial* GetCurrentMaterial() = 0;

	virtual int GetLightmapPage( void ) = 0;

	// Gets the maximum lightmap page size...
	virtual int GetLightmapWidth( int lightmap ) const = 0;
	virtual int GetLightmapHeight( int lightmap ) const = 0;

	virtual ITexture *GetLocalCubemap( void ) = 0;

//	virtual bool RenderZOnlyWithHeightClipEnabled( void ) = 0;
	virtual void ForceDepthFuncEquals( bool bEnable ) = 0;
	virtual enum MaterialHeightClipMode_t GetHeightClipMode( void ) = 0;

	// FIXME: Remove? Here for debugging shaders in CShaderSystem
	virtual void AddMaterialToMaterialList( IMaterialInternal *pMaterial ) = 0;
	virtual void RemoveMaterial( IMaterialInternal *pMaterial ) = 0;
	virtual void RemoveMaterialSubRect( IMaterialInternal *pMaterial ) = 0;
	virtual bool InFlashlightMode() const = 0;

	// Can we use editor materials?
	virtual bool CanUseEditorMaterials() const = 0;
	virtual const char *GetForcedTextureLoadPathID() = 0;

	virtual CMatCallQueue *GetRenderCallQueue() = 0;

	virtual void UnbindMaterial( IMaterial *pMaterial ) = 0;
	virtual ThreadId_t GetRenderThreadId() const = 0 ;

	virtual IMaterialProxy	*DetermineProxyReplacements( IMaterial *pMaterial, KeyValues *pFallbackKeyValues ) = 0;
};


#endif // IMATERIALSYSTEMINTERNAL_H
