//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef HUD_CHAT_H
#define HUD_CHAT_H
#ifdef _WIN32
#pragma once
#endif

#include <hud_basechat.h>

class CHudChat : public CBaseHudChat
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CHudChat, CBaseHudChat );

public:
	CHudChat( const char *pElementName );

	void	Init( void ) override;

	void			MsgFunc_SayText(bf_read &msg) override;
	void			MsgFunc_SayText2( bf_read &msg ) override;
	void			MsgFunc_TextMsg(bf_read &msg) override;
};

#endif	//HUD_CHAT_H