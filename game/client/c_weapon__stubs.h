//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_WEAPON__STUBS_H
#define C_WEAPON__STUBS_H
#ifdef _WIN32
#pragma once
#endif

#include "client_class.h"

// This is an ugly hack to link client classes to weapons for now
//  these will be removed once we predict all weapons, especially TF2 weapons
#define STUB_WEAPON_CLASS_IMPLEMENT( entityName, className )		\
	BEGIN_PREDICTION_DATA( className )								\
	END_PREDICTION_DATA()											\
	LINK_ENTITY_TO_CLASS( entityName, className );

// dimhotepus: Split STUB_WEAPON_CLASS on BEGIN .. END to allow method overloads.
#define STUB_WEAPON_CLASS_BEGIN(entityName, className, baseClassName) \
  class C_##className : public baseClassName {                          \
    DECLARE_CLASS(C_##className, baseClassName);                        \
                                                                        \
   public:                                                              \
    DECLARE_PREDICTABLE();                                              \
    DECLARE_CLIENTCLASS();                                              \
    C_##className(){};


// dimhotepus: Split STUB_WEAPON_CLASS on BEGIN .. END to allow method overloads.
#define STUB_WEAPON_CLASS_END(entityName, className, baseClassName) \
   private:                                                             \
    C_##className(const C_##className&);                                \
  };                                                                    \
  STUB_WEAPON_CLASS_IMPLEMENT(entityName, C_##className);               \
  IMPLEMENT_CLIENTCLASS_DT(C_##className, DT_##className, C##className) \
  END_RECV_TABLE                                                        \
  ()

#define STUB_WEAPON_CLASS( entityName, className, baseClassName )	\
	STUB_WEAPON_CLASS_BEGIN( entityName, className, baseClassName ) \
	STUB_WEAPON_CLASS_END( entityName, className, baseClassName )

#endif // C_WEAPON__STUBS_H
