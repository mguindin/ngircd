/*
 * ngIRCd -- The Next Generation IRC Daemon
 * Copyright (c)2001-2012 Alexander Barton (alex@barton.de) and Contributors.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * Please read the file COPYING, README and AUTHORS for more information.
 */

#include "portab.h"

/**
 * @file
 * Functions to deal with client logins
 */

#include "imp.h"
#include <assert.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

#include "defines.h"
#include "conn.h"
#include "class.h"
#include "client.h"
#include "client-cap.h"
#include "channel.h"
#include "conf.h"
#include "io.h"
#include "parse.h"
#include "log.h"
#include "messages.h"
#include "ngircd.h"
#include "pam.h"
#include "irc-info.h"
#include "irc-write.h"

#include "exp.h"
#include "login.h"

#ifdef PAM
static void cb_Read_Auth_Result PARAMS((int r_fd, UNUSED short events));
#endif

/**
 * Initiate client login.
 *
 * This function is called after the daemon received the required NICK and
 * USER commands of a new client. If the daemon is compiled with support for
 * PAM, the authentication sub-processs is forked; otherwise the global server
 * password is checked.
 *
 * @param Client The client logging in.
 * @returns CONNECTED or DISCONNECTED.
 */
GLOBAL bool
Login_User(CLIENT * Client)
{
#ifdef PAM
	int pipefd[2], result;
	pid_t pid;
#endif
	CONN_ID conn;

	assert(Client != NULL);
	conn = Client_Conn(Client);

#ifndef STRICT_RFC
	if (Conf_AuthPing) {
		/* Did we receive the "auth PONG" already? */
		if (Conn_GetAuthPing(conn)) {
			Client_SetType(Client, CLIENT_WAITAUTHPING);
			LogDebug("Connection %d: Waiting for AUTH PONG ...", conn);
			return CONNECTED;
		}
	}
#endif

	/* Still waiting for "CAP END" command? */
	if (Client_Cap(Client) & CLIENT_CAP_PENDING) {
		Client_SetType(Client, CLIENT_WAITCAPEND);
		LogDebug("Connection %d: Waiting for CAP END ...", conn);
		return CONNECTED;
	}

#ifdef PAM
	if (!Conf_PAM) {
		/* Don't do any PAM authentication at all, instead emulate
		 * the beahiour of the daemon compiled without PAM support:
		 * because there can't be any "server password", all
		 * passwords supplied are classified as "wrong". */
		if(Client_Password(Client)[0] == '\0')
			return Login_User_PostAuth(Client);
		Client_Reject(Client, "Non-empty password", false);
		return DISCONNECTED;
	}

	if (Conf_PAMIsOptional && strcmp(Client_Password(Client), "") == 0) {
		/* Clients are not required to send a password and to be PAM-
		 * authenticated at all. If not, they won't become "identified"
		 * and keep the "~" in their supplied user name.
		 * Therefore it is sensible to either set Conf_PAMisOptional or
		 * to enable IDENT lookups -- not both. */
		return Login_User_PostAuth(Client);
	}

	/* Fork child process for PAM authentication; and make sure that the
	 * process timeout is set higher than the login timeout! */
	pid = Proc_Fork(Conn_GetProcStat(conn), pipefd,
			cb_Read_Auth_Result, Conf_PongTimeout + 1);
	if (pid > 0) {
		LogDebug("Authenticator for connection %d created (PID %d).",
			 conn, pid);
		return CONNECTED;
	} else {
		/* Sub process */
		Log_Init_Subprocess("Auth");
		Conn_CloseAllSockets(NONE);
		result = PAM_Authenticate(Client);
		if (write(pipefd[1], &result, sizeof(result)) != sizeof(result))
			Log_Subprocess(LOG_ERR,
				       "Failed to pipe result to parent!");
		Log_Exit_Subprocess("Auth");
		exit(0);
	}
#else
	/* Check global server password ... */
	if (strcmp(Client_Password(Client), Conf_ServerPwd) != 0) {
		/* Bad password! */
		Client_Reject(Client, "Bad server password", false);
		return DISCONNECTED;
	}
	return Login_User_PostAuth(Client);
#endif
}

/**
 * Finish client registration.
 *
 * Introduce the new client to the network and send all "hello messages"
 * to it after authentication has been succeeded.
 *
 * @param Client The client logging in.
 * @return CONNECTED or DISCONNECTED.
 */
GLOBAL bool
Login_User_PostAuth(CLIENT *Client)
{
	assert(Client != NULL);

	if (Class_HandleServerBans(Client) != CONNECTED)
		return DISCONNECTED;

	Client_Introduce(NULL, Client, CLIENT_USER);

	if (!IRC_WriteStrClient
	    (Client, RPL_WELCOME_MSG, Client_ID(Client), Client_Mask(Client)))
		return false;
	if (!IRC_WriteStrClient
	    (Client, RPL_YOURHOST_MSG, Client_ID(Client),
	     Client_ID(Client_ThisServer()), PACKAGE_VERSION, TARGET_CPU,
	     TARGET_VENDOR, TARGET_OS))
		return false;
	if (!IRC_WriteStrClient
	    (Client, RPL_CREATED_MSG, Client_ID(Client), NGIRCd_StartStr))
		return false;
	if (!IRC_WriteStrClient
	    (Client, RPL_MYINFO_MSG, Client_ID(Client),
	     Client_ID(Client_ThisServer()), PACKAGE_VERSION, USERMODES,
	     CHANMODES))
		return false;

	/* Features supported by this server (005 numeric, ISUPPORT),
	 * see <http://www.irc.org/tech_docs/005.html> for details. */
	if (!IRC_Send_ISUPPORT(Client))
		return DISCONNECTED;

	if (!IRC_Send_LUSERS(Client))
		return DISCONNECTED;
	if (!IRC_Show_MOTD(Client))
		return DISCONNECTED;

	/* Suspend the client for a second ... */
	IRC_SetPenalty(Client, 1);

	return CONNECTED;
}

#ifdef PAM

/**
 * Read result of the authenticatior sub-process from pipe
 *
 * @param r_fd		File descriptor of the pipe.
 * @param events	(ignored IO specification)
 */
static void
cb_Read_Auth_Result(int r_fd, UNUSED short events)
{
	CONN_ID conn;
	CLIENT *client;
	int result;
	size_t len;
	PROC_STAT *proc;

	LogDebug("Auth: Got callback on fd %d, events %d", r_fd, events);
	conn = Conn_GetFromProc(r_fd);
	if (conn == NONE) {
		/* Ops, none found? Probably the connection has already
		 * been closed!? We'll ignore that ... */
		io_close(r_fd);
		LogDebug("Auth: Got callback for unknown connection!?");
		return;
	}
	proc = Conn_GetProcStat(conn);
	client = Conn_GetClient(conn);

	/* Read result from pipe */
	len = Proc_Read(proc, &result, sizeof(result));
	Proc_Close(proc);
	if (len == 0)
		return;

	if (len != sizeof(result)) {
		Log(LOG_CRIT, "Auth: Got malformed result!");
		Client_Reject(client, "Internal error", false);
		return;
	}

	if (result == true) {
		Client_SetUser(client, Client_OrigUser(client), true);
		(void)Login_User_PostAuth(client);
	} else
		Client_Reject(client, "Bad password", false);
}

#endif

/* -eof- */