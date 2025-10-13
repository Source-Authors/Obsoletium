//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Shared object based on a CBaseRecord subclass
//
//=============================================================================

#ifndef PROTOBUFSHAREDOBJECT_H
#define PROTOBUFSHAREDOBJECT_H
#ifdef _WIN32
#pragma once
#endif

#include "google/protobuf/descriptor.h"
#include "tier1/KeyValues.h"

#if defined( GC ) && defined( DEBUG )
#include "gcbase.h"
#endif

namespace google
{
	namespace protobuf
	{
		class Message;
	}
}

namespace GCSDK
{

//----------------------------------------------------------------------------
// Purpose: Base class for CProtoBufSharedObject. This is where all the actual
//			code lives.
//----------------------------------------------------------------------------
class CProtoBufSharedObjectBase : public CSharedObject
{
public:
	typedef CSharedObject BaseClass;

	virtual bool BParseFromMessage( const CUtlBuffer & buffer ) override;
	virtual bool BParseFromMessage( const std::string &buffer ) override;
	virtual bool BUpdateFromNetwork( const CSharedObject & objUpdate ) override;

	bool BIsKeyLess( const CSharedObject & soRHS ) const override;
	void Copy( const CSharedObject & soRHS ) override;
	void Dump() const override;

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const char *pchName );
#endif

#ifdef GC
	virtual bool BAddToMessage( CUtlBuffer & bufOutput ) const override;
	virtual bool BAddToMessage( std::string *pBuffer ) const override;
	virtual bool BAddDestroyToMessage( CUtlBuffer & bufDestroy ) const override;
	virtual bool BAddDestroyToMessage( std::string *pBuffer ) const override;

	virtual bool BParseFromMemcached( CUtlBuffer & buffer ) override;
	virtual bool BAddToMemcached( CUtlBuffer & bufOutput ) const override;

	static bool SerializeToBuffer( const ::google::protobuf::Message & msg, CUtlBuffer & bufOutput );
#endif //GC

	// Static helpers
	static void Dump( const ::google::protobuf::Message & msg );
	static KeyValues *CreateKVFromProtoBuf( const ::google::protobuf::Message & msg );
	static void RecursiveAddProtoBufToKV( KeyValues *pKVDest, const ::google::protobuf::Message & msg );

protected:
	virtual ::google::protobuf::Message *GetPObject() = 0;
	const ::google::protobuf::Message *GetPObject() const { return const_cast<CProtoBufSharedObjectBase *>(this)->GetPObject(); }

private:
#ifdef GC
	static ::google::protobuf::Message *BuildDestroyToMessage( const ::google::protobuf::Message & msg );
#endif //GC

};


//----------------------------------------------------------------------------
// Purpose: Template for making a shared object that uses a specific protobuf
//			message class for its wire protocol and in-memory representation.
//----------------------------------------------------------------------------
template< typename Message_t, int nTypeID, bool bPublicMutable = true >
class CProtoBufSharedObject : public CProtoBufSharedObjectBase
{
public:
	~CProtoBufSharedObject()
	{
#if defined( GC ) && defined( DEBUG )
		// Ensure this SO is not in any cache, or we have an error. We must provide the type since it is a virutal function otherwise
		Assert( !GGCBase()->IsSOCached( this, nTypeID ) );
#endif
	}

	virtual int GetTypeID() const { return nTypeID; }

	// WHERE IS YOUR GOD NOW
	template< typename T >
		using Public_Message_t = typename std::enable_if_t< bPublicMutable && std::is_same_v< T, Message_t >, Message_t & >;
	template< typename T >
		using Protected_Message_t = typename std::enable_if_t< !bPublicMutable && std::is_same_v< T, Message_t >, Message_t & >;

	template< typename T = Message_t >
		Public_Message_t<T> Obj() { return m_msgObject; }

	const Message_t & Obj() const { return m_msgObject; }

	using SchObjectType_t = Message_t;
	static constexpr inline int k_nTypeID = nTypeID;

protected:
	template< typename T = Message_t >
		Protected_Message_t<T> MutObj() { return m_msgObject; }

	::google::protobuf::Message *GetPObject() { return &m_msgObject; }

private:
	Message_t m_msgObject;
};

//----------------------------------------------------------------------------
// Purpose: Template for making a shared object that uses a specific protobuf
//          message class for its wire protocol and in-memory representation.
//
//          The wrapper version of this class wraps a message allocated and
//          owned elsewhere. The user of this class is in charge of
//          guaranteeing that lifetime.
//----------------------------------------------------------------------------
template< typename Message_t, int nTypeID >
class CProtoBufSharedObjectWrapper : public CProtoBufSharedObjectBase
{
public:
	CProtoBufSharedObjectWrapper( Message_t *pMsgToWrap )
		: m_pMsgObject( pMsgToWrap )
	{}

	~CProtoBufSharedObjectWrapper()
	{
#if defined( GC ) && defined( DEBUG )
		// Ensure this SO is not in any cache, or we have an error. We must provide the type since it is a virutal function otherwise
		Assert( !GGCBase()->IsSOCached( this, nTypeID ) );
#endif
	}

	virtual int GetTypeID() const { return nTypeID; }

	Message_t & Obj() { return *m_pMsgObject; }
	const Message_t & Obj() const { return *m_pMsgObject; }

	using SchObjectType_t = Message_t ;
public:
	static constexpr inline int k_nTypeID = nTypeID;

protected:
	::google::protobuf::Message *GetPObject() { return m_pMsgObject; }

private:
	Message_t *m_pMsgObject;
};

} // GCSDK namespace

#endif //PROTOBUFSHAREDOBJECT_H
