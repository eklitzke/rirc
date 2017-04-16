#ifndef CHANNEL_H
#define CHANNEL_H

#include "nicklist.h"

/* TODO: refactor -> channel.c */

#define MODE_SIZE (26 * 2) + 1 /* Supports modes [az-AZ] */

/* nav activity types */
typedef enum
{
	ACTIVITY_DEFAULT,
	ACTIVITY_ACTIVE,
	ACTIVITY_PINGED,
	ACTIVITY_T_SIZE
} activity_t;

/* Channel */
typedef struct channel
{
	activity_t active;
	char *name;
	char type_flag;
	char chanmodes[MODE_SIZE];
	int parted;
	struct buffer buffer;
	struct channel *next;
	struct channel *prev;
	struct input *input;
	struct nicklist nicklist;
	struct server *server;
} channel;

#endif
