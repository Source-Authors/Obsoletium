// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// $USE_STATIC_CONTROL_FLOW && ( $NUM_LIGHTS > 0 )
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH

#ifndef SKIN_VS20_H
#define SKIN_VS20_H

#include "shaderapi/ishaderapi.h"
#include "shaderapi/ishadershadow.h"
#include "materialsystem/imaterialvar.h"

class skin_vs20_Static_Index
{
	unsigned int m_nUSE_STATIC_CONTROL_FLOW : 2;
#ifndef NDEBUG
	bool m_bUSE_STATIC_CONTROL_FLOW : 1;
#endif  // !NDEBUG
public:
	void SetUSE_STATIC_CONTROL_FLOW( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nUSE_STATIC_CONTROL_FLOW = i;
#ifndef NDEBUG
		m_bUSE_STATIC_CONTROL_FLOW = true;
#endif  // !NDEBUG
	}

	skin_vs20_Static_Index()
	{
		m_nUSE_STATIC_CONTROL_FLOW = 0;
#ifndef NDEBUG
		m_bUSE_STATIC_CONTROL_FLOW = false;
#endif  // !NDEBUG
	}

	int GetIndex() const
	{
		Assert( m_bUSE_STATIC_CONTROL_FLOW );
		return ( 48 * m_nUSE_STATIC_CONTROL_FLOW ) + 0;
	}
};

#define shaderStaticTest_skin_vs20 vsh_forgot_to_set_static_USE_STATIC_CONTROL_FLOW


class skin_vs20_Dynamic_Index
{
	unsigned int m_nCOMPRESSED_VERTS : 2;
	unsigned int m_nDOWATERFOG : 2;
	unsigned int m_nSKINNING : 2;
	unsigned int m_nLIGHTING_PREVIEW : 2;
	unsigned int m_nNUM_LIGHTS : 2;
#ifndef NDEBUG
	bool m_bCOMPRESSED_VERTS : 1;
	bool m_bDOWATERFOG : 1;
	bool m_bSKINNING : 1;
	bool m_bLIGHTING_PREVIEW : 1;
	bool m_bNUM_LIGHTS : 1;
#endif  // !NDEBUG
public:
	void SetCOMPRESSED_VERTS( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nCOMPRESSED_VERTS = i;
#ifndef NDEBUG
		m_bCOMPRESSED_VERTS = true;
#endif  // !NDEBUG
	}

	void SetDOWATERFOG( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDOWATERFOG = i;
#ifndef NDEBUG
		m_bDOWATERFOG = true;
#endif  // !NDEBUG
	}

	void SetSKINNING( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSKINNING = i;
#ifndef NDEBUG
		m_bSKINNING = true;
#endif  // !NDEBUG
	}

	void SetLIGHTING_PREVIEW( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nLIGHTING_PREVIEW = i;
#ifndef NDEBUG
		m_bLIGHTING_PREVIEW = true;
#endif  // !NDEBUG
	}

	void SetNUM_LIGHTS( int i )
	{
		Assert( i >= 0 && i <= 2 );
		m_nNUM_LIGHTS = i;
#ifndef NDEBUG
		m_bNUM_LIGHTS = true;
#endif  // !NDEBUG
	}

	skin_vs20_Dynamic_Index()
	{
		m_nCOMPRESSED_VERTS = 0;
		m_nDOWATERFOG = 0;
		m_nSKINNING = 0;
		m_nLIGHTING_PREVIEW = 0;
		m_nNUM_LIGHTS = 0;
#ifndef NDEBUG
		m_bCOMPRESSED_VERTS = false;
		m_bDOWATERFOG = false;
		m_bSKINNING = false;
		m_bLIGHTING_PREVIEW = false;
		m_bNUM_LIGHTS = false;
#endif  // !NDEBUG
	}

	int GetIndex() const
	{
		Assert( m_bCOMPRESSED_VERTS && m_bDOWATERFOG && m_bSKINNING && m_bLIGHTING_PREVIEW && m_bNUM_LIGHTS );
		return ( 1 * m_nCOMPRESSED_VERTS ) + ( 2 * m_nDOWATERFOG ) + ( 4 * m_nSKINNING ) + ( 8 * m_nLIGHTING_PREVIEW ) + ( 16 * m_nNUM_LIGHTS ) + 0;
	}
};

#define shaderDynamicTest_skin_vs20 vsh_forgot_to_set_dynamic_COMPRESSED_VERTS + vsh_forgot_to_set_dynamic_DOWATERFOG + vsh_forgot_to_set_dynamic_SKINNING + vsh_forgot_to_set_dynamic_LIGHTING_PREVIEW + vsh_forgot_to_set_dynamic_NUM_LIGHTS


#endif	// SKIN_VS20_H
