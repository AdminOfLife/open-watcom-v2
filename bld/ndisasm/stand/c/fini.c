/****************************************************************************
*
*                            Open Watcom Project
*
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
* Description:  WHEN YOU FIGURE OUT WHAT THIS FILE DOES, PLEASE
*               DESCRIBE IT HERE!
*
****************************************************************************/


#include <stdlib.h>
#include <string.h>
#include "wio.h"
#include "dis.h"
#include "global.h"
#include "fini.h"
#include "memfuncs.h"
#include "hashtabl.h"
#include "identsec.h"
#include "init.h"
#include "main.h"
#include "labproc.h"

extern wd_options       Options;
extern orl_handle       ORLHnd;
extern dis_handle       DHnd;

extern char             *ListFileName;
extern char             *SourceFileName;
extern char             *SourceFileInDwarf;

extern int              OutputDest;
extern orl_file_handle  ObjFileHnd;

extern hash_table       HandleToSectionTable;
extern hash_table       HandleToLabelListTable;
extern hash_table       HandleToRefListTable;
extern hash_table       SymbolToLabelTable;
extern hash_table       NameRecognitionTable;
extern hash_table       SkipRefTable;

extern section_list_struct      Sections;
extern publics_struct           Publics;

static void freeScanTabList( scantab_ptr st )
{
    scantab_ptr         next;

    for( ; st != NULL; st = next ) {
        next = st->next;
        MemFree( st );
    }
}

static void freeSectionList( section_list_struct *list )
{
    section_ptr         next;

    for( ; list->first != NULL; list->first = next ) {
        next = list->first->next;
        freeScanTabList( list->first->scan );
        MemFree( list->first );
    }
}

static void freeLabelList( orl_sec_handle shnd )
{
    hash_data           *data_ptr;
    label_list          list;
    label_entry         entry;
    label_entry         next;

    data_ptr = HashTableQuery( HandleToLabelListTable, shnd );
    if( data_ptr != NULL ) {
        list = (label_list)*data_ptr;
        for( entry = list->first; entry != NULL; entry = next ) {
            next = entry->next;
            FreeLabel( entry );
        }
        MemFree( list );
    }
}

static void freeRefList( orl_sec_handle shnd )
{
    hash_data           *data_ptr;
    ref_list            list;
    ref_entry           entry;
    ref_entry           next;

    data_ptr = HashTableQuery( HandleToRefListTable, shnd );
    if( data_ptr != NULL ) {
        list = (ref_list)*data_ptr;
        for( entry = list->first; entry != NULL; entry = next ) {
            next = entry->next;
            MemFree( entry );
        }
        MemFree( list );
    }
}

static orl_return SectionFini( orl_sec_handle shnd )
{
    section_type        type;

    type = IdentifySec( shnd );
    switch( type ) {
    case SECTION_TYPE_TEXT:
    case SECTION_TYPE_BSS:
    case SECTION_TYPE_DATA:
    case SECTION_TYPE_DRECTVE:
    case SECTION_TYPE_PDATA:
    default:
        freeRefList( shnd );
        freeLabelList( shnd );
        break;
    }
    return( ORL_OKAY );
}

static void freePublics( void )
{
    label_list_ptr      ptr;

    while( (ptr = Publics.label_lists) != NULL ) {
        Publics.label_lists = Publics.label_lists->next;
        MemFree( ptr );
    }
    if( Publics.public_symbols != NULL ) {
        MemFree( Publics.public_symbols );
    }
}

void FreeHashTables( void )
{
    HashTableFree( HandleToSectionTable );
    HashTableFree( SymbolToLabelTable );
    HashTableFree( HandleToLabelListTable );
    HashTableFree( HandleToRefListTable );
    HashTableFree( NameRecognitionTable );
    HashTableFree( SkipRefTable );
}

void FreeServicesUsed( void )
{
    orl_return          status;

    DisFini( &DHnd );
    status = ORLFini( ORLHnd );
    if( status != ORL_OKAY ) {
        PrintErrorMsg( RC_ERROR, WHERE_CLOSING_ORL );
    }
}

void CloseFiles( void )
{
    CloseObjFile();
    if( ListFileName ) {
        close( OutputDest );
        MemFree( ListFileName );
    }
    if( SourceFileInDwarf ) {
        MemFree( SourceFileInDwarf );
    }
    if( SourceFileName ) {
        MemFree( SourceFileName );
    }
}

void Fini( void )
{
    freeSectionList( &Sections );
    freeLabelList( 0 );
    ORLFileScan( ObjFileHnd, NULL, SectionFini );
    if( Options & PRINT_PUBLICS ) {
        freePublics();
    }
    FreeHashTables();
    FreeServicesUsed();
    CloseFiles();
    MemPrtList();
    MemClose();
    MsgFini();
}
