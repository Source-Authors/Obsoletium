//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef CUSTOMGAMES_H
#define CUSTOMGAMES_H
#ifdef _WIN32
#pragma once
#endif

#define MAX_TAG_CHARACTERS			128

class TagInfoLabel : public vgui::URLLabel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( TagInfoLabel, vgui::URLLabel );
public:
	TagInfoLabel(Panel *parent, const char *panelName);
	TagInfoLabel(Panel *parent, const char *panelName, const char *text, const char *pszURL);

	void	OnMousePressed(vgui::MouseCode code) override;

	MESSAGE_FUNC( DoOpenCustomServerInfoURL, "DoOpenCustomServerInfoURL" );
};

class TagMenuButton : public vgui::MenuButton
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( TagMenuButton, vgui::MenuButton );
public:
	TagMenuButton( Panel *parent, const char *panelName, const char *text);

	void OnShowMenu(vgui::Menu *menu) override;
};

//-----------------------------------------------------------------------------
// Purpose: Internet games with tags
//-----------------------------------------------------------------------------

class CCustomGames : public CInternetGames
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CCustomGames, CInternetGames );
public:
	CCustomGames(vgui::Panel *parent);
	~CCustomGames();

	virtual void	UpdateDerivedLayouts( void ) override;
	virtual void	OnLoadFilter(KeyValues *filter) override;
	virtual void	OnSaveFilter(KeyValues *filter) override;
	bool	CheckTagFilter( gameserveritem_t &server ) override;
	bool	CheckWorkshopFilter( gameserveritem_t &server ) override;
	virtual void	SetRefreshing(bool state) override;
	virtual void	ServerResponded( int iServer, gameserveritem_t *pServerItem ) override;

	MESSAGE_FUNC_PARAMS( OnAddTag, "AddTag", params );
	MESSAGE_FUNC( OnTagMenuButtonOpened, "TagMenuButtonOpened" );

	void			RecalculateCommonTags( void );
	void			AddTagToFilterList( const char *pszTag );

private:
	TagMenuButton	*m_pAddTagList;
	vgui::Menu		*m_pTagListMenu;
	vgui::TextEntry	*m_pTagFilter;
	char			m_szTagFilter[MAX_TAG_CHARACTERS];
};


#endif // CUSTOMGAMES_H
