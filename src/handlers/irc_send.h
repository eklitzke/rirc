#ifndef IRC_SEND_H
#define IRC_SEND_H

int irc_send_command(struct server*, struct channel*, char*);
int irc_send_privmsg(struct server*, struct channel*, char*);

#endif
