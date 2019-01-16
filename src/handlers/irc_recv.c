#include <ctype.h>

#include "src/components/server.h"
#include "src/handlers/irc_ctcp.h"
#include "src/handlers/irc_recv.gperf.out"
#include "src/handlers/irc_recv.h"
#include "src/io.h"
#include "src/state.h"
#include "src/utils/utils.h"

#define failf(S, ...) \
	do { server_err((S), __VA_ARGS__); \
	     return 1; \
	} while (0)

#define sendf(S, ...) \
	do { int ret; \
	     if ((ret = io_sendf((S)->connection, __VA_ARGS__))) \
	         failf((S), "Send fail: %s", io_err(ret)); \
	     return 0; \
	} while (0)

/* Generic handlers */
static int irc_error(struct server*, struct irc_message*);
static int irc_ignore(struct server*, struct irc_message*);
static int irc_message(struct server*, struct irc_message*);

/* Numeric handlers */
static int irc_001(struct server*, struct irc_message*);
static int irc_004(struct server*, struct irc_message*);
static int irc_005(struct server*, struct irc_message*);

static const irc_recv_f irc_numerics[] = {
	  [1] = irc_001,     /* RPL_WELCOME */
	  [2] = irc_message, /* RPL_YOURHOST */
	  [3] = irc_message, /* RPL_CREATED */
	  [4] = irc_004,     /* RPL_MYINFO */
	  [5] = irc_005,     /* RPL_ISUPPORT */
	[200] = NULL,        /* RPL_TRACELINK */
	[201] = NULL,        /* RPL_TRACECONNECTING */
	[202] = NULL,        /* RPL_TRACEHANDSHAKE */
	[203] = NULL,        /* RPL_TRACEUNKNOWN */
	[204] = NULL,        /* RPL_TRACEOPERATOR */
	[205] = NULL,        /* RPL_TRACEUSER */
	[206] = NULL,        /* RPL_TRACESERVER */
	[207] = NULL,        /* RPL_TRACESERVICE */
	[208] = NULL,        /* RPL_TRACENEWTYPE */
	[209] = NULL,        /* RPL_TRACECLASS */
	[210] = NULL,        /* RPL_TRACELOG */
	[211] = NULL,        /* RPL_STATSLINKINFO */
	[212] = NULL,        /* RPL_STATSCOMMANDS */
	[213] = NULL,        /* RPL_STATSCLINE */
	[214] = NULL,        /* RPL_STATSNLINE */
	[215] = NULL,        /* RPL_STATSILINE */
	[216] = NULL,        /* RPL_STATSKLINE */
	[217] = NULL,        /* RPL_STATSQLINE */
	[218] = NULL,        /* RPL_STATSYLINE */
	[219] = NULL,        /* RPL_ENDOFSTATS */
	[221] = NULL,        /* RPL_UMODEIS */
	[234] = NULL,        /* RPL_SERVLIST */
	[235] = NULL,        /* RPL_SERVLISTEND */
	[240] = NULL,        /* RPL_STATSVLINE */
	[241] = NULL,        /* RPL_STATSLLINE */
	[242] = NULL,        /* RPL_STATSUPTIME */
	[243] = NULL,        /* RPL_STATSOLINE */
	[244] = NULL,        /* RPL_STATSHLINE */
	[245] = NULL,        /* RPL_STATSSLINE */
	[246] = NULL,        /* RPL_STATSPING */
	[247] = NULL,        /* RPL_STATSBLINE */
	[250] = NULL,        /* RPL_STATSCONN */
	[251] = NULL,        /* RPL_LUSERCLIENT */
	[252] = NULL,        /* RPL_LUSEROP */
	[253] = NULL,        /* RPL_LUSERUNKNOWN */
	[254] = NULL,        /* RPL_LUSERCHANNELS */
	[255] = NULL,        /* RPL_LUSERME */
	[256] = NULL,        /* RPL_ADMINME */
	[257] = NULL,        /* RPL_ADMINLOC1 */
	[258] = NULL,        /* RPL_ADMINLOC2 */
	[259] = NULL,        /* RPL_ADMINEMAIL */
	[262] = NULL,        /* RPL_TRACEEND */
	[263] = NULL,        /* RPL_TRYAGAIN */
	[265] = NULL,        /* RPL_LOCALUSERS */
	[266] = NULL,        /* RPL_GLOBALUSERS */
	[301] = NULL,        /* RPL_AWAY */
	[302] = NULL,        /* ERR_USERHOST */
	[303] = NULL,        /* RPL_ISON */
	[305] = NULL,        /* RPL_UNAWAY */
	[306] = NULL,        /* RPL_NOWAWAY */
	[311] = NULL,        /* RPL_WHOISUSER */
	[312] = NULL,        /* RPL_WHOISSERVER */
	[313] = NULL,        /* RPL_WHOISOPERATOR */
	[314] = NULL,        /* RPL_WHOWASUSER */
	[315] = NULL,        /* RPL_ENDOFWHO */
	[317] = NULL,        /* RPL_WHOISIDLE */
	[318] = NULL,        /* RPL_ENDOFWHOIS */
	[319] = NULL,        /* RPL_WHOISCHANNELS */
	[322] = NULL,        /* RPL_LIST */
	[323] = NULL,        /* RPL_LISTEND */
	[324] = NULL,        /* RPL_CHANNELMODEIS */
	[325] = NULL,        /* RPL_UNIQOPIS */
	[328] = NULL,        /* RPL_CHANNEL_URL */
	[331] = irc_ignore,  /* RPL_NOTOPIC */
	[332] = NULL,        /* RPL_TOPIC */
	[333] = NULL,        /* RPL_TOPICWHOTIME */
	[341] = NULL,        /* RPL_INVITING */
	[346] = NULL,        /* RPL_INVITELIST */
	[347] = NULL,        /* RPL_ENDOFINVITELIST */
	[348] = NULL,        /* RPL_EXCEPTLIST */
	[349] = NULL,        /* RPL_ENDOFEXCEPTLIST */
	[351] = NULL,        /* RPL_VERSION */
	[352] = NULL,        /* RPL_WHOREPLY */
	[353] = NULL,        /* RPL_NAMREPLY */
	[364] = NULL,        /* RPL_LINKS */
	[365] = NULL,        /* RPL_ENDOFLINKS */
	[366] = irc_ignore,  /* RPL_ENDOFNAMES */
	[367] = NULL,        /* RPL_BANLIST */
	[368] = NULL,        /* RPL_ENDOFBANLIST */
	[369] = NULL,        /* RPL_ENDOFWHOWAS */
	[371] = NULL,        /* RPL_INFO */
	[372] = NULL,        /* RPL_MOTD */
	[374] = NULL,        /* RPL_ENDOFINFO */
	[375] = NULL,        /* RPL_MOTDSTART */
	[376] = irc_ignore,  /* RPL_ENDOFMOTD */
	[381] = NULL,        /* RPL_YOUREOPER */
	[391] = NULL,        /* RPL_TIME */
	[401] = NULL,        /* ERR_NOSUCHNICK */
	[402] = NULL,        /* ERR_NOSUCHSERVER */
	[403] = NULL,        /* ERR_NOSUCHCHANNEL */
	[404] = NULL,        /* ERR_CANNOTSENDTOCHAN */
	[405] = NULL,        /* ERR_TOOMANYCHANNELS */
	[406] = NULL,        /* ERR_WASNOSUCHNICK */
	[407] = NULL,        /* ERR_TOOMANYTARGETS */
	[408] = NULL,        /* ERR_NOSUCHSERVICE */
	[409] = NULL,        /* ERR_NOORIGIN */
	[411] = NULL,        /* ERR_NORECIPIENT */
	[412] = NULL,        /* ERR_NOTEXTTOSEND */
	[413] = NULL,        /* ERR_NOTOPLEVEL */
	[414] = NULL,        /* ERR_WILDTOPLEVEL */
	[415] = NULL,        /* ERR_BADMASK */
	[416] = NULL,        /* ERR_TOOMANYMATCHES */
	[421] = NULL,        /* ERR_UNKNOWNCOMMAND */
	[422] = NULL,        /* ERR_NOMOTD */
	[423] = NULL,        /* ERR_NOADMININFO */
	[431] = NULL,        /* ERR_NONICKNAMEGIVEN */
	[432] = NULL,        /* ERR_ERRONEUSNICKNAME */
	[433] = NULL,        /* ERR_NICKNAMEINUSE */
	[436] = NULL,        /* ERR_NICKCOLLISION */
	[437] = NULL,        /* ERR_UNAVAILRESOURCE */
	[441] = NULL,        /* ERR_USERNOTINCHANNEL */
	[442] = NULL,        /* ERR_NOTONCHANNEL */
	[443] = NULL,        /* ERR_USERONCHANNEL */
	[451] = NULL,        /* ERR_NOTREGISTERED */
	[461] = NULL,        /* ERR_NEEDMOREPARAMS */
	[462] = NULL,        /* ERR_ALREADYREGISTRED */
	[463] = NULL,        /* ERR_NOPERMFORHOST */
	[464] = NULL,        /* ERR_PASSWDMISMATCH */
	[465] = NULL,        /* ERR_YOUREBANNEDCREEP */
	[466] = NULL,        /* ERR_YOUWILLBEBANNED */
	[467] = NULL,        /* ERR_KEYSET */
	[471] = NULL,        /* ERR_CHANNELISFULL */
	[472] = NULL,        /* ERR_UNKNOWNMODE */
	[473] = NULL,        /* ERR_INVITEONLYCHAN */
	[474] = NULL,        /* ERR_BANNEDFROMCHAN */
	[475] = NULL,        /* ERR_BADCHANNELKEY */
	[476] = NULL,        /* ERR_BADCHANMASK */
	[477] = NULL,        /* ERR_NOCHANMODES */
	[478] = NULL,        /* ERR_BANLISTFULL */
	[481] = NULL,        /* ERR_NOPRIVILEGES */
	[482] = NULL,        /* ERR_CHANOPRIVSNEEDED */
	[483] = NULL,        /* ERR_CANTKILLSERVER */
	[484] = NULL,        /* ERR_RESTRICTED */
	[485] = NULL,        /* ERR_UNIQOPPRIVSNEEDED */
	[491] = NULL,        /* ERR_NOOPERHOST */
	[501] = NULL,        /* ERR_UMODEUNKNOWNFLAG */
	[502] = NULL,        /* ERR_USERSDONTMATCH */
	[704] = NULL,        /* RPL_HELPSTART */
	[705] = NULL,        /* RPL_HELP */
	[706] = NULL,        /* RPL_ENDOFHELP */
};

static int
irc_error(struct server *s, struct irc_message *m)
{
	/* Generic error handling */

	/* TODO */
	(void)s;
	(void)m;
	(void)irc_error;
	(void)irc_ignore;
	(void)irc_message;

	return 0;
}

static int
irc_ignore(struct server *s, struct irc_message *m)
{
	/* Generic handling for ignored message types */

	/* TODO */
	(void)s;
	(void)m;
	(void)irc_error;
	(void)irc_ignore;
	(void)irc_message;

	return 0;
}

static int
irc_message(struct server *s, struct irc_message *m)
{
	/* Generic message handling */

	/* TODO */
	(void)s;
	(void)m;
	(void)irc_error;
	(void)irc_ignore;
	(void)irc_message;

	return 0;
}

static int
irc_001(struct server *s, struct irc_message *m)
{
	/* 001 :<Welcome message> */

	(void)s;
	(void)m;

	return 0;
}

static int
irc_004(struct server *s, struct irc_message *m)
{
	/* 004 1*<params> [:message] */

	// TODO: parse to trailing arg if it exists

	// newlinef(s->channel, 0, "--", "%s ~ supported by this server", p->params);
	// server_set_004(s, p->params);

	(void)s;
	(void)m;
	return 0;
}

static int
irc_005(struct server *s, struct irc_message *m)
{
	/* 005 1*<params> :Are supported by this server */

	// newlinef(s->channel, 0, "--", "%s ~ supported by this server", p->params);
	//server_set_004(s, p->params);

	(void)s;
	(void)m;
	return 0;
}

static int
irc_recv_numeric(struct server *s, struct irc_message *m)
{
	/* :server <code> <target> [args] */

	char *targ;
	int code = 0;
	irc_recv_f handler = NULL;

	for (const char *p = m->command; *p; p++) {

		if (!isdigit(*p))
			failf(s, "NUMERIC: invalid");

		code *= 10;
		code += *p - '0';

		if (code > 999)
			failf(s, "NUMERIC: out of range");
	}

	/* Message target is only used to establish s->nick when registering with a server */
	if ((irc_message_param(m, &targ))) {
		io_dx(s->connection);
		failf(s, "NUMERIC: target is null");
	}

	/* Message target should match s->nick or '*' if unregistered, otherwise out of sync */
	if (strcmp(targ, s->nick) && strcmp(targ, "*") && code != 1) {
		io_dx(s->connection);
		failf(s, "NUMERIC: target mismatched, nick is '%s', received '%s'", s->nick, targ);
	}

	if (ARR_ELEM(irc_numerics, code))
		handler = irc_numerics[code];

	if (handler)
		return (*handler)(s, m);

	failf(s, "Numeric type '%u' unknown", code);
}

int
irc_recv(struct server *s, struct irc_message *m)
{
	const struct recv_handler* handler;

	if (isdigit(*m->command))
		return irc_recv_numeric(s, m);

	if ((handler = recv_handler_lookup(m->command, m->len_command)))
		return handler->f(s, m);

	failf(s, "Message type '%s' unknown", m->command);
}

static int recv_error(struct server *s, struct irc_message *m) { (void)s; (void)m; return 0; }
static int recv_invite(struct server *s, struct irc_message *m) { (void)s; (void)m; return 0; }
static int recv_join(struct server *s, struct irc_message *m) { (void)s; (void)m; return 0; }
static int recv_kick(struct server *s, struct irc_message *m) { (void)s; (void)m; return 0; }
static int recv_mode(struct server *s, struct irc_message *m) { (void)s; (void)m; return 0; }
static int recv_nick(struct server *s, struct irc_message *m) { (void)s; (void)m; return 0; }
static int recv_notice(struct server *s, struct irc_message *m) { (void)s; (void)m; return 0; }
static int recv_part(struct server *s, struct irc_message *m) { (void)s; (void)m; return 0; }
static int recv_ping(struct server *s, struct irc_message *m) { (void)s; (void)m; return 0; }
static int recv_pong(struct server *s, struct irc_message *m) { (void)s; (void)m; return 0; }
static int recv_privmsg(struct server *s, struct irc_message *m) { (void)s; (void)m; return 0; }
static int recv_quit(struct server *s, struct irc_message *m) { (void)s; (void)m; return 0; }
static int recv_topic(struct server *s, struct irc_message *m) { (void)s; (void)m; return 0; }
