//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "hud.h"
#include "hud_crosshair.h"
#include "iclientmode.h"
#include "view.h"
#include "vgui_controls/controls.h"
#include "vgui/ISurface.h"
#include "IVRenderView.h"
#include "ff_weapon_base.h"
#include "c_ff_player.h"
#include "ff_utils.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar crosshair( "crosshair", "1", FCVAR_ARCHIVE | FCVAR_CLIENTDLL);
ConVar cl_observercrosshair( "cl_observercrosshair", "1", FCVAR_ARCHIVE | FCVAR_CLIENTDLL);
ConVar cl_acchargebar("cl_acchargebar", "0", FCVAR_ARCHIVE | FCVAR_CLIENTDLL);
ConVar cl_concaim_movexhair("cl_concaim_movexhair", "0", FCVAR_ARCHIVE | FCVAR_CLIENTDLL, "0 = always show xhair in centre. 1 = show xhair trueaim. 2 = hide xhair when conced");

#define FFDEV_CONCAIM_MOVEXHAIR cl_concaim_movexhair.GetInt()
	
using namespace vgui;

int ScreenTransform( const Vector& point, Vector& screen );

DECLARE_HUDELEMENT( CHudCrosshair );

CHudCrosshair::CHudCrosshair( const char *pElementName ) :
  CHudElement( pElementName ), BaseClass( NULL, "HudCrosshair" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	m_pCrosshair = 0;

	m_clrCrosshair = Color( 0, 0, 0, 0 );

	m_vecCrossHairOffsetAngle.Init();

	SetHiddenBits( HIDEHUD_PLAYERDEAD | HIDEHUD_CROSSHAIR );
}

void CHudCrosshair::ApplySchemeSettings( IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );

	m_pDefaultCrosshair = gHUD.GetIcon("crosshair_default");
	SetPaintBackgroundEnabled( false );

	// --> Mirv
	vgui::HScheme CrossHairScheme = vgui::scheme()->LoadSchemeFromFile("resource/CrosshairScheme.res", "CrosshairScheme");

	for (int i = 0; i < CROSSHAIR_SIZES; i++)
	{
		m_hPrimaryCrosshairs[i] = vgui::scheme()->GetIScheme(CrossHairScheme)->GetFont(VarArgs("PrimaryCrosshairs%d", (i + 1)));
		m_hSecondaryCrosshairs[i] = vgui::scheme()->GetIScheme(CrossHairScheme)->GetFont(VarArgs("SecondaryCrosshairs%d", (i + 1)));
	}
	// <-- Mirv


    SetSize( ScreenWidth(), ScreenHeight() );
}

//-----------------------------------------------------------------------------
// Purpose: Save CPU cycles by letting the HUD system early cull
// costly traversal.  Called per frame, return true if thinking and 
// painting need to occur.
//-----------------------------------------------------------------------------
bool CHudCrosshair::ShouldDraw( void )
{
	bool bNeedsDraw;

	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return false;

	C_FFPlayer *pFFPlayer = ToFFPlayer( pPlayer );

	// Dunno about this... specs might want a crosshair drawn?
	if( FF_IsPlayerSpec( pFFPlayer ) || !FF_HasPlayerPickedClass( pFFPlayer ) )
		return false;

	// draw a crosshair only if alive or spectating in eye
	if ( IsXbox() )
	{
		bNeedsDraw = m_pCrosshair && 
			!engine->IsDrawingLoadingImage() &&
			!engine->IsPaused() && 
			!pPlayer->IsSuitEquipped() &&
			g_pClientMode->ShouldDrawCrosshair() &&
			!( pPlayer->GetFlags() & FL_FROZEN ) &&
			( pPlayer->entindex() == render->GetViewEntity() ) &&
			( pPlayer->IsAlive() ||	( pPlayer->GetObserverMode() == OBS_MODE_IN_EYE ) || ( cl_observercrosshair.GetBool() && pPlayer->GetObserverMode() == OBS_MODE_ROAMING ) );
	}
	else
	{
		bNeedsDraw = m_pCrosshair && 
			crosshair.GetInt() &&
			!engine->IsDrawingLoadingImage() &&
			!engine->IsPaused() && 
			g_pClientMode->ShouldDrawCrosshair() &&
			!( pPlayer->GetFlags() & FL_FROZEN ) &&
			( pPlayer->entindex() == render->GetViewEntity() ) &&
			( pPlayer->IsAlive() ||	( pPlayer->GetObserverMode() == OBS_MODE_IN_EYE ) || ( cl_observercrosshair.GetBool() && pPlayer->GetObserverMode() == OBS_MODE_ROAMING ) );
	}

	return ( bNeedsDraw && CHudElement::ShouldDraw() );
}

extern void GetCrosshair(FFWeaponID iWeapon, char &innerChar, Color &innerCol, int &innerSize, char &outerChar, Color &outerCol, int &outerSize);	// |-- Mirv

void CHudCrosshair::Paint( void )
{
	if ( !m_pCrosshair )
		return;

	if ( !IsCurrentViewAccessAllowed() )
		return;


	C_FFPlayer *pPlayer = ToFFPlayer(CBasePlayer::GetLocalPlayer());

	m_curViewAngles = CurrentViewAngles();
	m_curViewOrigin = CurrentViewOrigin();

	float x, y;
	x = ScreenWidth()/2;
	y = ScreenHeight()/2;

	// MattB - m_vecCrossHairOffsetAngle is the autoaim angle.
	// if we're not using autoaim, just draw in the middle of the 
	// screen
	if ( m_vecCrossHairOffsetAngle != vec3_angle )
	{
		Assert(0);	// |-- Mirv
		QAngle angles;
		Vector forward;
		Vector point, screen;

		// this code is wrong
		angles = m_curViewAngles + m_vecCrossHairOffsetAngle;
		AngleVectors( angles, &forward );
		VectorAdd( m_curViewOrigin, forward, point );
		ScreenTransform( point, screen );

		x += 0.5f * screen[0] * ScreenWidth() + 0.5f;
		y += 0.5f * screen[1] * ScreenHeight() + 0.5f;
	}

	// AfterShock: Conc aim -> plot crosshair properly
	if ( ( FFDEV_CONCAIM_MOVEXHAIR == 1) && ( (pPlayer->m_flConcTime > gpGlobals->curtime) || (pPlayer->m_flConcTime < 0) ) )
	{
		QAngle angles;
		Vector forward;
		Vector point, screen;

		// this code is wrong
		// AfterShock: No, the code is now right!
		angles = pPlayer->EyeAngles();
		AngleVectors( angles, &forward );
		forward *= 10000.0f;
		VectorAdd( m_curViewOrigin, forward, point );
		ScreenTransform( point, screen );

		x = (screen[0]*0.5 + 0.5f) * ScreenWidth();
		y = (1 - ( screen[1]*0.5 + 0.5f ) ) * ScreenHeight();
	}
	else if ( ( FFDEV_CONCAIM_MOVEXHAIR == 2) && ( (pPlayer->m_flConcTime > gpGlobals->curtime) || (pPlayer->m_flConcTime < 0) ) )
	{
		x = -1;
		y = -1;
	}


	// --> Mirv: Crosshair stuff
	//m_pCrosshair->DrawSelf( 
	//		x - 0.5f * m_pCrosshair->Width(), 
	//		y - 0.5f * m_pCrosshair->Height(),
	//		m_clrCrosshair );
	
	if (!pPlayer)
		return;

	C_FFWeaponBase *pWeapon = pPlayer->GetActiveFFWeapon();

	// No crosshair for no weapon
	if (!pWeapon)
		return;

	FFWeaponID weaponID = pPlayer->GetActiveFFWeapon()->GetWeaponID();

	// Weapons other than these don't get crosshairs
	if (weaponID <= FF_WEAPON_NONE || weaponID > FF_WEAPON_TOMMYGUN)
		return;

	Color innerCol, outerCol;
	char innerChar, outerChar;
	int innerSize, outerSize;
	wchar_t unicode[2];

	//
	// TODO: Clean this up!!!!
	//

	HFont currentFont;
	GetCrosshair(weaponID, innerChar, innerCol, innerSize, outerChar, outerCol, outerSize);

	currentFont = m_hSecondaryCrosshairs[clamp(outerSize, 1, CROSSHAIR_SIZES) - 1];

	surface()->DrawSetTextColor(outerCol.r(), outerCol.g(), outerCol.b(), outerCol.a());
	surface()->DrawSetTextFont(currentFont);

	int charOffsetX = surface()->GetCharacterWidth(currentFont, outerChar) / 2;
	int charOffsetY = surface()->GetFontTall(currentFont) / 2;

	swprintf(unicode, L"%c", outerChar);

	surface()->DrawSetTextPos(x - charOffsetX, y - charOffsetY);
	surface()->DrawUnicodeChar(unicode[0]);

	currentFont = m_hPrimaryCrosshairs[clamp(innerSize, 1, CROSSHAIR_SIZES) - 1];

	surface()->DrawSetTextColor(innerCol.r(), innerCol.g(), innerCol.b(), innerCol.a());
	surface()->DrawSetTextFont(currentFont);

	charOffsetX = surface()->GetCharacterWidth(currentFont, innerChar) / 2;
	charOffsetY = surface()->GetFontTall(currentFont) / 2;

	swprintf(unicode, L"%c", innerChar);

	surface()->DrawSetTextPos(x - charOffsetX, y - charOffsetY);
	surface()->DrawUnicodeChar(unicode[0]);
	// <-- Mirv

	// Mulch: Draw charge bar!
	if( (weaponID == FF_WEAPON_ASSAULTCANNON) && (cl_acchargebar.GetBool()) )
	{
		extern float GetAssaultCannonCharge();
		float flCharge = GetAssaultCannonCharge();

		if( flCharge <= 0.0f )
			return;

		int iLeft = x - charOffsetX;
		int iTop = y + charOffsetY;
		int iRight = iLeft + (charOffsetX * 2);
		int iBottom = iTop + 10;

		surface()->DrawSetColor( innerCol.r(), innerCol.g(), innerCol.b(), 150 );
		surface()->DrawFilledRect( iLeft, iTop, iLeft + ((float)(iRight - iLeft) * (flCharge / 100.0f)), iBottom );

		surface()->DrawSetColor( outerCol.r(), outerCol.g(), outerCol.b(), 200 );		
		surface()->DrawOutlinedRect( iLeft, iTop, iRight, iBottom );
	}
	else if( weaponID == FF_WEAPON_SNIPERRIFLE )
	{
		extern float GetSniperRifleCharge();
		float flCharge = GetSniperRifleCharge();

		if( flCharge <= 1.0f )
			return;

		int iLeft = x - charOffsetX;
		int iTop = y + charOffsetY;
		int iRight = iLeft + (charOffsetX * 2);
		int iBottom = iTop + 10;

		surface()->DrawSetColor( innerCol.r(), innerCol.g(), innerCol.b(), 150 );
		surface()->DrawFilledRect( iLeft, iTop, iLeft + ((float)(iRight - iLeft) * (flCharge / 100.0f)), iBottom );

		surface()->DrawSetColor( outerCol.r(), outerCol.g(), outerCol.b(), 200 );		
		surface()->DrawOutlinedRect( iLeft, iTop, iRight, iBottom );
	}
	else if ( weaponID == FF_WEAPON_JUMPDOWN )
	{
		extern float GetJumpdownCharge();
		float flCharge = GetJumpdownCharge();
		
		if( flCharge <= 0.0f )
			return;

		int iLeft = x - charOffsetX;
		int iTop = y + charOffsetY;
		int iRight = iLeft + (charOffsetX * 2);
		int iBottom = iTop + 10;

		if ( flCharge == 1.0f )
		{
			surface()->DrawSetColor( 128, 255, 64, 150 );
		}
		else
		{
			surface()->DrawSetColor( innerCol.r(), innerCol.g(), innerCol.b(), 150 );
		}
		surface()->DrawFilledRect( iLeft, iTop, iLeft + ((float)(iRight - iLeft) * (flCharge)), iBottom );

		surface()->DrawSetColor( outerCol.r(), outerCol.g(), outerCol.b(), 200 );		
		surface()->DrawOutlinedRect( iLeft, iTop, iRight, iBottom );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudCrosshair::SetCrosshairAngle( const QAngle& angle )
{
	VectorCopy( angle, m_vecCrossHairOffsetAngle );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudCrosshair::SetCrosshair( CHudTexture *texture, Color& clr )
{
	m_pCrosshair = texture;
	m_clrCrosshair = clr;
}

//-----------------------------------------------------------------------------
// Purpose: Resets the crosshair back to the default
//-----------------------------------------------------------------------------
void CHudCrosshair::ResetCrosshair()
{
	SetCrosshair( m_pDefaultCrosshair, Color(255, 255, 255, 255) );
}
