/*
 * ngIRCd -- The Next Generation IRC Daemon
 * Copyright (c)2001,2002 by Alexander Barton (alex@barton.de)
 *
 * Dieses Programm ist freie Software. Sie koennen es unter den Bedingungen
 * der GNU General Public License (GPL), wie von der Free Software Foundation
 * herausgegeben, weitergeben und/oder modifizieren, entweder unter Version 2
 * der Lizenz oder (wenn Sie es wuenschen) jeder spaeteren Version.
 * Naehere Informationen entnehmen Sie bitter der Datei COPYING. Eine Liste
 * der an ngIRCd beteiligten Autoren finden Sie in der Datei AUTHORS.
 *
 * $Id: irc-op.c,v 1.1 2002/05/27 11:22:07 alex Exp $
 *
 * irc-op.c: Befehle zur Channel-Verwaltung
 */


#include "portab.h"

#include "imp.h"
#include <assert.h>
#include <string.h>

#include "conn.h"
#include "client.h"
#include "channel.h"
#include "defines.h"
#include "irc-write.h"
#include "log.h"
#include "messages.h"
#include "parse.h"

#include "exp.h"
#include "irc-op.h"


GLOBAL BOOLEAN
IRC_KICK( CLIENT *Client, REQUEST *Req )
{
	assert( Client != NULL );
	assert( Req != NULL );

	/* Valider Client? */
	if(( Client_Type( Client ) != CLIENT_USER ) && ( Client_Type( Client ) != CLIENT_SERVER )) return IRC_WriteStrClient( Client, ERR_NOTREGISTERED_MSG, Client_ID( Client ));

	/* Keine Parameter? */
	if( Req->argc < 1 ) return IRC_WriteStrClient( Client, ERR_NEEDMOREPARAMS_MSG, Client_ID( Client ), Req->command );

	return CONNECTED;
} /* IRC_KICK */	


GLOBAL BOOLEAN
IRC_BAN( CLIENT *Client, REQUEST *Req )
{
	assert( Client != NULL );
	assert( Req != NULL );

	/* Valider Client? */
	if(( Client_Type( Client ) != CLIENT_USER ) && ( Client_Type( Client ) != CLIENT_SERVER )) return IRC_WriteStrClient( Client, ERR_NOTREGISTERED_MSG, Client_ID( Client ));

	/* Keine Parameter? */
	if( Req->argc < 1 ) return IRC_WriteStrClient( Client, ERR_NEEDMOREPARAMS_MSG, Client_ID( Client ), Req->command );

	return CONNECTED;
} /* IRC_BAN */	


GLOBAL BOOLEAN
IRC_INVITE( CLIENT *Client, REQUEST *Req )
{
	assert( Client != NULL );
	assert( Req != NULL );

	/* Valider Client? */
	if(( Client_Type( Client ) != CLIENT_USER ) && ( Client_Type( Client ) != CLIENT_SERVER )) return IRC_WriteStrClient( Client, ERR_NOTREGISTERED_MSG, Client_ID( Client ));

	/* Keine Parameter? */
	if( Req->argc < 1 ) return IRC_WriteStrClient( Client, ERR_NEEDMOREPARAMS_MSG, Client_ID( Client ), Req->command );

	return CONNECTED;
} /* IRC_INVITE */


/* -eof- */