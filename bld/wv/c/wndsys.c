/****************************************************************************
*
*                            Open Watcom Project
*
* Copyright (c) 2002-2018 The Open Watcom Contributors. All Rights Reserved.
*    Portions Copyright (c) 1983-2002 Sybase, Inc. All Rights Reserved.
*
*  ========================================================================
*
*    This file contains Original Code and/or Modifications of Original
*    Code as defined in and that are subject to the Sybase Open Watcom
*    Public License version 1.0 (the 'License'). You may not use this file
*    except in compliance with the License. BY USING THIS FILE YOU AGREE TO
*    ALL TERMS AND CONDITIONS OF THE LICENSE. A copy of the License is
*    provided with the Original Code and Modifications, and is also
*    available at www.sybase.com/developer/opensource.
*
*    The Original Code and all software distributed under the License are
*    distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
*    EXPRESS OR IMPLIED, AND SYBASE AND ALL CONTRIBUTORS HEREBY DISCLAIM
*    ALL SUCH WARRANTIES, INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF
*    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR
*    NON-INFRINGEMENT. Please see the License for the specific language
*    governing rights and limitations under the License.
*
*  ========================================================================
*
* Description:  Windowing 'system' routines.
*
****************************************************************************/


#include <ctype.h>
#if defined( __NT__ ) && defined( __GUI__ )
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif
#include "dbgdefn.h"
#include "dbgdata.h"
#include "dbgwind.h"
#include "dbgerr.h"
#include "dbgadget.h"
#include "dbghook.h"
#include "dbgio.h"
#include "dbgscrn.h"
#include "strutil.h"
#include "dbgscan.h"
#include "dbgutil.h"
#include "dbgmain.h"
#include "dbgbrk.h"
#include "dbgwdlg.h"
#include "wndsys.h"
#include "remcore.h"
#include "dipimp.h"
#include "dipinter.h"
#include "dbgwass.h"
#include "dbgwdisp.h"
#include "dbgwfil.h"
#include "dbgwfing.h"
#include "dbgwmac.h"
#include "dbgwmem.h"
#include "dbgwset.h"
#include "dbgwsrch.h"
#include "wnddump.h"
#include "wndmenu.h"
#include "dbgwpain.h"
#include "dbginit.h"


extern wnd_posn         WndPosition[WND_NUM_CLASSES];
extern gui_rect         WndMainRect;

extern bool             UsrScrnMode( void );
extern bool             UserScreen( void );
extern bool             DebugScreen( void );
extern bool             DebugScreenRecover( void );
extern void             WndPosToRect( wnd_posn*, gui_rect *, gui_coord * );
extern void             SetUpdateFlags( update_list );

static void             WndBadCmd( a_window );

#include "menudef.h"
char *WndGadgetHint[] =
{
    #define pick( a,b,c,d,e,f ) f,
    #include "gadgets.h"
    #undef pick
};

gui_resource WndGadgetArray[] = {
    #define pick( a,b,c,d,e,f ) { b, d },
    #include "gadgets.h"
    #undef pick
    #define pick( a,b,c,d,e,f ) { c, e },
    #include "gadgets.h"
    #undef pick
    { BITMAP_SPLASH, "splash" },
};

int         WndGadgetHintSize = ArraySize( WndGadgetHint );
int         WndGadgetArraySize = ArraySize( WndGadgetArray );
int         WndGadgetSecondary = GADGET_FIRST_SECONDARY;
wnd_attr    WndGadgetAttr = WND_HOTSPOT;
int         MaxGadgetLength;

static const char WindowNameTab[] =
{
    "CLose\0"
    "CURSORStart\0"
    "CURSOREnd\0"
    "CURSORDown\0"
    "CURSORLeft\0"
    "CURSORRight\0"
    "CURSORUp\0"
    "Dump\0"
    "Log\0"
    "FINDNext\0"
    "FINDPrev\0"
    "Next\0"
    "PAGEDown\0"
    "PAGEUp\0"
    "POpup\0"
    "SEarch\0"
    "SCROLLDown\0"
    "SCROLLUp\0"
    "SCROLLTop\0"
    "SCROLLBottom\0"
    "TABLeft\0"
    "TABRight\0"
    "MAXimize\0"
    "MINimize\0"
    "REStore\0"
    "TIle\0"
    "CAscade\0"
    "Move\0"
    "SIze\0"
    "PRevious\0"
};

OVL_EXTERN void ToWndChooseNew( a_window wnd )
{
    /* unused parameters */ (void)wnd;

    WndChooseNew();
}

static void (* const WndJmpTab[])( a_window ) =
{
    &WndClose,
    &WndCursorStart,
    &WndCursorEnd,
    &WndCursorDown,
    &WndCursorLeft,
    &WndCursorRight,
    &WndCursorUp,
    &WndDumpFile,
    &WndDumpLog,
    &ProcWndFindNext,
    &ProcWndFindPrev,
    &ToWndChooseNew,
    &WndPageDown,
    &WndPageUp,
    &ProcWndPopUp,
    &ProcWndSearch,
    &WndScrollDown,
    &WndScrollUp,
    &WndScrollTop,
    &WndScrollBottom,
    &ProcWndTabLeft,
    &ProcWndTabRight,
    &WndMaximizeWindow,
    &WndMinimizeWindow,
    &WndRestoreWindow,
    &ProcPUINYI,
    &ProcPUINYI,
    &ProcPUINYI,
    &ProcPUINYI,
    &ProcPUINYI,
};

wnd_metrics WndNormal =      { 5, 25,  0,  0 };
wnd_metrics WndLong =        { 5, 25, 20,  0 };
wnd_metrics WndMemMetrics =  { 5, 25,  5,  0 };
wnd_metrics WndWide =        { 5, 25,  0, 60 };
wnd_metrics WndWideLong =    { 5, 25, 20, 60 };
wnd_metrics WndFileMetrics = { 5, 25, 70, 70 };

wnd_metrics *WndMetrics[] = {
    #define pick(a,b,c,d,e,f) &e,
    #include "wndnames.h"
    #undef pick
};

wnd_metrics NoMetrics = { 0, 0, 0, 0 };

bool DbgWndSearch( a_window wnd, bool from_top, int direction )
{
    bool        rc;

    rc = WndSearch( wnd, from_top, direction );
    return( rc );
}

void ProcPUINYI( a_window wnd )
{
    /* unused parameters */ (void)wnd;

    Say( "NYI" );
}

void ProcWndSearch( a_window wnd )
{
    DbgWndSearch( wnd, false, DlgSearch( wnd, SrchHistory ) );
}

void ProcWndTabLeft( a_window wnd )
{
    WndTabLeft( wnd, true );
}

void ProcWndTabRight( a_window wnd )
{
    WndTabRight( wnd, true );
}

void ProcSearchAll( void )
{
    char        *expr;

    if( DlgSearchAll( &expr, SrchHistory ) ) {
        WndSrchOpen( expr );
    }
}

void ProcWndPopUp( a_window wnd )
{
    WndKeyPopUp( wnd, NULL );
}

void ProcWndFindNext( a_window wnd )
{
    DbgWndSearch( wnd, false, 1 );
}

void ProcWndFindPrev( a_window wnd )
{
    DbgWndSearch( wnd, false, -1 );
}

static void WndBadCmd( a_window wnd )
{
    /* unused parameters */ (void)wnd;

    Error( ERR_LOC, LIT_ENG( ERR_BAD_SUBCOMMAND ), GetCmdName( CMD_WINDOW ) );
}

void WndProcWindow( void )
{
    int         cmd;
    a_window    wnd = WndFindActive();

    cmd = ScanCmd( WindowNameTab );
    ReqEOC();
    if( wnd != NULL ) {
        if( cmd < 0 ) {
            WndBadCmd( wnd );
        } else {
            WndJmpTab[cmd]( wnd );
        }
    }
}


void WndSysStart( void )
{
#if defined(__UNIX__)
    ScrnSpawnStart();
#endif
    GUISpawnStart();
}


void WndSysEnd( bool pause )
{
    /* unused parameters */ (void)pause; // NYI - PUI

    GUISpawnEnd();
#if defined(__UNIX__)
    ScrnSpawnEnd();
#endif
}


static bool DoScreenSwap( void )
{
#if defined(__GUI__) && defined(__WINDOWS__)
    return( false );
#else
    return( _IsOff( SW_REMOTE_LINK ) );
#endif
}


void WndStop( void )
{
    if( DoScreenSwap() ) {
        WndUser();
    }
}

void WndUser( void )
{
    if( (ScrnState & USR_SCRN_ACTIVE) == 0 ) {
        if( (ScrnState & USR_SCRN_VISIBLE) == 0 )
            RemoteSetUserScreen();
        ScrnState |= USR_SCRN_ACTIVE | USR_SCRN_VISIBLE;
        GUIGMouseOff();
        if( UserScreen() ) {
            ScrnState |= DBG_SCRN_VISIBLE;
        } else {
            ScrnState &= ~DBG_SCRN_VISIBLE;
        }
    }
    ScrnState &= ~DBG_SCRN_ACTIVE;
}


void WndDebug( void )
{
    if( _IsOn( SW_MIGHT_HAVE_LOST_DISPLAY ) ) {
        if( ScrnState & DBG_SCRN_ACTIVE ) {
            if( !DebugScreenRecover() ) {
                ScrnState &= ~DBG_SCRN_ACTIVE;
            }
        }
        _SwitchOff( SW_MIGHT_HAVE_LOST_DISPLAY );
    }
    if( (ScrnState & DBG_SCRN_ACTIVE) == 0 ) {
        if( (ScrnState & DBG_SCRN_VISIBLE) == 0 )
            RemoteSetDebugScreen();
        ScrnState |= DBG_SCRN_ACTIVE | DBG_SCRN_VISIBLE;
        if( DebugScreen() ) {
            ScrnState |= USR_SCRN_VISIBLE;
        } else {
            ScrnState &= ~USR_SCRN_VISIBLE;
        }
        GUIGMouseOn();
    }
    ScrnState &= ~USR_SCRN_ACTIVE;
}


void WndRedraw( wnd_class_wv wndclass )
{
    a_window    wnd;

    for( wnd = WndNext( NULL ); wnd != NULL; wnd = WndNext( wnd ) ) {
        if( WndClass( wnd ) == wndclass ) {
            WndZapped( wnd );
        }
    }
}


static void ProcessMacro( wnd_macro *mac )
{
    if( mac->type == MACRO_MAIN_MENU ) {
        if( mac->menu != NULL ) {
            AccelMenuItem( mac->menu, true );
        }
    } else if( mac->type == MACRO_POPUP_MENU ) {
        if( mac->menu != NULL ) {
            AccelMenuItem( mac->menu, false );
        }
    } else {
        PushCmdList( mac->cmd );
    }
}


static bool AmpEqual( const char *label, char ch )
{
    int lowerch;

    lowerch = tolower( ch );
    while( label[0] != NULLCHAR ) {
        if( label[0] == '&' && tolower( label[1] ) == lowerch )
            return( true );
        ++label;
    }
    return( false );
}


static gui_menu_struct *FindLocalMenu( char ch, gui_menu_struct *child, int size )
{
    gui_menu_struct     *curr;
    int                 i;

    curr = child;
    for( i = 0; i < size; ++i ) {
        if( AmpEqual( curr->label, ch ) )
            return( curr );
        ++curr;
    }
#ifdef DO_WE_REALLY_WANT_THIS
    gui_menu_struct     *sub;
    curr = child;
    for( i = 0; i < size; ++i ) {
        if( curr->num_child_menus != NULL ) {
            sub = FindLocalMenu( ch, curr->child, curr->num_child_menus );
            if( sub != NULL ) {
                return( sub );
            }
        }
        ++curr;
    }
#endif
    return( NULL );
}

bool    WndProcMacro( a_window wnd, unsigned key )
{
    wnd_macro           *mac;
    wnd_macro           *all;
    char                ch;
    gui_menu_struct     *menu;

    all = NULL;
    if( WndClass( wnd ) == WND_MACRO && MacKeyHit( wnd, key ) ) {
        return( true );
    }
    for( mac = WndMacroList; mac != NULL; mac = mac->link ) {
        if( mac->key == key ) {
            if( mac->wndclass == WND_ALL ) {
                all = mac;
            } else if( mac->wndclass == WndClass( wnd ) ) {
                ProcessMacro( mac );
                return( true );
            }
        }
    }
    if( all != NULL ) {
        ProcessMacro( all );
        return( true );
    }
    if( wnd == NULL )
        return( false );
    ch = LookUpCtrlKey( key );
    if( ch == 0 )
        return( false );
    menu = FindLocalMenu( ch, WndPopupMenu( wnd ), WndNumPopups( wnd ) );//
    if( menu == NULL )
        return( false );
    AccelMenuItem( menu, false );
    return( true );
}


void WndSysInit( void )
{
    ScrnState = DBG_SCRN_ACTIVE | DBG_SCRN_VISIBLE;

    if( UsrScrnMode() ) {
        ScrnState |= USR_SCRN_VISIBLE;
    }

    if( !DoScreenSwap() ) {
        ScrnState |= USR_SCRN_ACTIVE | USR_SCRN_VISIBLE;
    }
}

void SetUnderLine( a_window wnd, wnd_line_piece *line )
{
    line->attr = WND_STANDOUT;
    line->tabstop = false;
    line->static_text = true;
    line->text = LIT_ENG( Empty );
    line->underline = true;
    line->indent = 0;
    line->extent = WndWidth( wnd );
}

void SetGadgetLine( a_window wnd, wnd_line_piece *line, wnd_gadget_type type )
{
    WndSetGadgetLine( wnd, line, type, MaxGadgetLength );
}

void FiniGadget( void )
{
    int                 i;

    for( i = 0; i < WndGadgetHintSize; ++i ) {
        WndFree( WndGadgetHint[i] );
        WndGadgetHint[i] = NULL;
    }
}

void InitGadget( void )
{
    int                 i;
    gui_coord           size;

    WndGadgetInit();
    MaxGadgetLength = 0;
    for( i = 0; i < WndGadgetHintSize; ++i ) {
        WndGadgetHint[i] = WndLoadString( (gui_res_id)(pointer_int)WndGadgetHint[i] );
    }
    for( i = 1; i <= WndGadgetArraySize - 1; ++i ) {
        if( GUIGetHotSpotSize( i, &size ) && size.x > MaxGadgetLength ) {
            MaxGadgetLength = size.x;
        }
    }
}

bool OpenGadget( a_window wnd, wnd_line_piece *line, mod_handle mod, bool src )
{
    if( src ) {
        return( FileOpenGadget( wnd, line, mod ) );
    } else {
        return( AsmOpenGadget( wnd, line, mod ) );
    }
}

bool CheckOpenGadget( a_window wnd, wnd_row row,
                      bool open, mod_handle mod, bool src, int piece )
{
    bool        is_open;

    is_open = OpenGadget( wnd, NULL, mod, src );
    if( is_open != open ) {
        WndPieceDirty( wnd, row, piece );
    }
    return( is_open );
}

void WndStartFreshAll( void )
{
    WndDebug();
}

void WndEndFreshAll( void )
{
    UpdateFlags = 0;
    CheckBPErrors();
}

void WndFlushKeys( void )
{
    GUIFlushKeys();
}

void WndMsgBox( const char *msg )
{
    FingClose(); // close this if we have a startup error.  2x won't hurt
    WndDebug();
    WndDisplayMessage( msg, "", GUI_INFORMATION+GUI_SYSTEMMODAL);
}

void WndInfoBox( const char *msg )
{
    if( !( WndDlgTxt( msg ) || WndStatusText( msg ) ) ) {
        WndMsgBox( msg );
    }
}

void WndSetOpenNoShow( void )
{
    _SwitchOn( SW_OPEN_NO_SHOW );
}

a_window DbgTitleWndCreate( const char *title, wnd_info *wndinfo,
                                    wnd_class_wv wndclass, void *extra,
                                    gui_resource *icon,
                                    int title_size, bool vdrag )
{
    a_window            wnd;
    wnd_create_struct   info;
    char                *p;

    WndInitCreateStruct( &info );
    WndPosToRect( &WndPosition[wndclass], &info.rect, WndMainClientSize() );
    if( (wnd = WndFindClass( NULL, wndclass )) != NULL ) {
        info.rect.x += WndMaxCharX( wnd );
        info.rect.y += WndMaxCharY( wnd );
    }
    switch( wndclass ) {
    case WND_VARIABLE: // let it draw with it's 'natural' spacing before shrink
        info.rect.x = info.rect.y = 0;
        info.rect.width = info.rect.height = WND_APPROX_SIZE;
        break;
    case WND_TMPFILE: // always shrinks (ignore last position info)
        info.rect.x = info.rect.y = 0;
        info.rect.width = info.rect.height = 0;
        break;
    }
    p = TxtBuff;
    info.title = p;
    for( ;; ++title ) {
        if( *title == '&' )
            continue;
        *p = *title;
        if( *p++ == NULLCHAR ) {
            break;
        }
    }
    info.info = wndinfo;
    info.wndclass = wndclass;
    info.extra = extra;
    info.colour = GetWndColours( wndclass );
    info.title_size = title_size;
    info.style |= GUI_INIT_INVISIBLE;
#if defined(__GUI__) && defined(__OS2__)
    info.style &= ~GUI_CHANGEABLE_FONT;
#endif
    if( _IsOn( SW_DETACHABLE_WINDOWS ) )
        info.style |= GUI_POPUP;
    if( !vdrag )
        info.scroll &= ~GUI_VDRAG;
    wnd = WndCreateWithStruct( &info );
    if( wnd == NULL )
        return( wnd );
    WndSetFontInfo( wnd, GetWndFont( wnd ) );
    WndClrSwitches( wnd, WSW_MUST_CLICK_ON_PIECE | WSW_MENU_ACCURATE_ROW );
    WndSetSwitches( wnd, WSW_RBUTTON_CHANGE_CURR | WSW_CACHE_LINES );
    if( !WndHasCurrent( wnd ) )
        WndFirstCurrent( wnd );
    if( _IsOff( SW_OPEN_NO_SHOW ) ) {
        WndForcePaint( wnd );
        if( info.rect.width == 0 || info.rect.height == 0 ) {
            WndShrinkToMouse( wnd, WndMetrics[wndclass] );
        }
        if( _IsOff( SW_RUNNING_PROFILE ) ) {
            WndShowWindow( wnd );
        }
    }
    WndSetIcon( wnd, icon );
    _SwitchOff( SW_OPEN_NO_SHOW );
    return( wnd );
}

a_window DbgWndCreate( const char *title, wnd_info *info,
                               wnd_class_wv wndclass, void *extra, gui_resource *icon )
{
    return( DbgTitleWndCreate( title, info, wndclass, extra, icon, 0, true ) );
}

static char **RXErrTxt[] = {
    LITREF_DUI( ERR_RX_1 ),
    LITREF_DUI( ERR_RX_2 ),
    LITREF_DUI( ERR_RX_3 ),
    LITREF_DUI( ERR_RX_4 ),
    LITREF_DUI( ERR_RX_5 ),
    LITREF_DUI( ERR_RX_6 ),
    LITREF_DUI( ERR_RX_7 ),
    LITREF_DUI( ERR_RX_8 ),
    LITREF_DUI( ERR_RX_9 ),
    LITREF_DUI( ERR_RX_10 ),
    LITREF_DUI( ERR_RX_11 ),
    LITREF_DUI( ERR_RX_12 ),
    LITREF_DUI( ERR_RX_13 )
};

void WndRXError( int num )
{
    Error( ERR_NONE, *RXErrTxt[num - 1] );
}

#if defined(__GUI__)
static bool GetInitName( char *name, char *buff, size_t buff_len )
{
    buff_len = GetSystemDir( buff, buff_len );
    if( buff_len == 0 )
        return( false );
    buff += buff_len;
    *buff++ = '\\';
    buff = StrCopy( name, buff );
    buff = StrCopy( ".INI", buff );
    return( true );
}

void SaveMainScreen( char *name )
{
    file_handle fh;
    char        buff[FILENAME_MAX];

    if( !GetInitName( name, buff, sizeof( buff ) ) )
        return;
    fh = FileOpen( buff, OP_WRITE | OP_CREATE );
    if( fh == NIL_HANDLE )
        return;
    WriteStream( fh, &WndMainRect, sizeof( WndMainRect ) );
    FileClose( fh );
}

void RestoreMainScreen( char *name )
{
    file_handle fh;
    char        buff[FILENAME_MAX];

    if( !GetInitName( name, buff, sizeof( buff ) ) )
        return;
    fh = FileOpen( buff, OP_READ );
    if( fh == NIL_HANDLE )
        return;
    ReadStream( fh, &WndMainRect, sizeof( WndMainRect ) );
    FileClose( fh );
}

void WndSetWndMainSize( wnd_create_struct *info )
{
#ifdef __NT__
    RECT        workarea;
    gui_rect    screen;
    int         sx;
    int         sy;
    int         w;
    int         h;
    SystemParametersInfo( SPI_GETWORKAREA, 0, &workarea, 0 );
    GUIGetScale( &screen );
    sx = screen.width / GetSystemMetrics( SM_CXSCREEN );
    sy = screen.height / GetSystemMetrics( SM_CYSCREEN );
    w = sx * (workarea.right - workarea.left);
    h = sy * (workarea.bottom - workarea.top);
#endif
    info->rect = WndMainRect;
#ifdef __NT__
    if( info->rect.width == 0 || info->rect.width > w ) {
        info->rect.width = w;
    }
    if( info->rect.height == 0 || info->rect.height > h ) {
        info->rect.height = h;
    }
#endif
}
#endif
