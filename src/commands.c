/*  commands.c
 *
 *
 *  Copyright (C) 2014 toxbot All Rights Reserved.
 *
 *  This file is part of toxbot.
 *
 *  toxbot is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  toxbot is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with toxbot. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <time.h>
#include <inttypes.h>

#include <tox/tox.h>
#include <tox/toxav.h>

#include "toxbot.h"
#include "misc.h"
#include "groupchats.h"

#define MAX_COMMAND_LENGTH TOX_MAX_MESSAGE_LENGTH
#define MAX_NUM_ARGS 4

extern char *DATA_FILE;
extern char *MASTERLIST_FILE;
extern struct Tox_Bot Tox_Bot;

void send_msg(Tox *m, int friendnum, char *msg) {
    tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) msg, strlen(msg), NULL);
}

static void authent_failed(Tox *m, int friendnum)
{
    send_msg(m, friendnum, "Invalid command.");
}

static void cmd_default(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
        send_msg(m, friendnum, "Error: Room number required");
        return;
    }

    int groupnum = atoi(argv[1]);

    if ((groupnum == 0 && strcmp(argv[1], "0")) || groupnum < 0) {
        send_msg(m, friendnum, "Error: Invalid room number");
        return;
    }

    Tox_Bot.default_groupnum = groupnum;

    char msg[MAX_COMMAND_LENGTH];
    snprintf(msg, sizeof(msg), "Default room number set to %d", groupnum);
    send_msg(m, friendnum, msg);

    char name[TOX_MAX_NAME_LENGTH];
    tox_friend_get_name(m, friendnum, (uint8_t *) name, NULL);
    size_t len = tox_friend_get_name_size(m, friendnum, NULL);
    name[len] = '\0';

    printf("Default room number set to %d by %s", groupnum, name);
}

static void cmd_gmessage(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
        send_msg(m, friendnum, "Error: Group number required");
        return;
    }

    if (argc < 2) {
        send_msg(m, friendnum, "Error: Message required");
        return;
    }

    int groupnum = atoi(argv[1]);

    if (groupnum == 0 && strcmp(argv[1], "0")) {
        send_msg(m, friendnum, "Error: Invalid group number");
        return;
    }

    if (group_index(groupnum) == -1) {
        send_msg(m, friendnum, "Error: Invalid group number");
        return;
    }

    if (argv[2][0] != '\"') {
        send_msg(m, friendnum, "Error: Message must be enclosed in quotes");
        return;
    }

    /* remove opening and closing quotes */
    char msg[MAX_COMMAND_LENGTH];
    snprintf(msg, sizeof(msg), "%s", &argv[2][1]);
    int len = strlen(msg) - 1;
    msg[len] = '\0';

    if (tox_group_message_send(m, groupnum, (uint8_t *) msg, strlen(msg)) == -1) {
        send_msg(m, friendnum, "Error: Failed to send message");
        return;
    }

    char name[TOX_MAX_NAME_LENGTH];
    tox_friend_get_name(m, friendnum, (uint8_t *) name, NULL);
    size_t nlen = tox_friend_get_name_size(m, friendnum, NULL);
    name[nlen] = '\0';

    send_msg(m, friendnum, "Message sent");
    printf("<%s> message to group %d: %s\n", name, groupnum, msg);
}

static void cmd_group(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    if (argc < 1) {
        send_msg(m, friendnum, "Please specify the group type: audio or text");
        return;
    }

    uint8_t type = TOX_GROUPCHAT_TYPE_AV ? !strcasecmp(argv[1], "audio") : TOX_GROUPCHAT_TYPE_TEXT;

    char name[TOX_MAX_NAME_LENGTH];
    tox_friend_get_name(m, friendnum, (uint8_t *) name, NULL);
    size_t len = tox_friend_get_name_size(m, friendnum, NULL);
    name[len] = '\0';

    int groupnum = -1;

    if (type == TOX_GROUPCHAT_TYPE_TEXT)
        groupnum = tox_add_groupchat(m);
    else if (type == TOX_GROUPCHAT_TYPE_AV)
        groupnum = toxav_add_av_groupchat(m, NULL, NULL);

    if (groupnum == -1) {
        printf("Group chat creation by %s failed to initialize\n", name);
        send_msg(m, friendnum, "Group chat instance failed to initialize");
        return;
    }

    const char *password = argc >= 2 ? argv[2] : NULL;

    if (password && strlen(argv[2]) >= MAX_PASSWORD_SIZE) {
        printf("Group chat creation by %s failed: Password too long\n", name);
        send_msg(m, friendnum, "Group chat instance failed to initialize: Password too long");
        return;
    }

    if (group_add(groupnum, type, password) == -1) {
        printf("Group chat creation by %s failed\n", name);
        send_msg(m, friendnum, "Group chat creation failed");
        tox_del_groupchat(m, groupnum);
        return;
    }

    const char *pw = password ? " (Password protected)" : "";
    printf("Group chat %d created by %s%s\n", groupnum, name, pw);

    char msg[MAX_COMMAND_LENGTH];
    snprintf(msg, sizeof(msg), "Group chat %d created%s", groupnum, pw);
	send_msg(m, friendnum, msg);
}

static void cmd_help(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    char msg[MAX_COMMAND_LENGTH];
    strcpy(msg, " × info\t\t: Print my current status and list active group chats\n");
    strcat(msg, " × id\t\t: Print my Tox ID\n");
    strcat(msg, " × invite\t\t: Request invite to default group chat\n");
    strcat(msg, " × invite <n> <p>\t: Request invite to group chat n (with Password if protected)\n");
    strcat(msg, " × group <t> <p>\t: Creates a new groupchat with Type: text | audio (optional Password)");
    send_msg(m, friendnum, msg);

    if (friend_is_master(m, friendnum)) {
        strcpy(msg, "ToxBot Master Commands:\n");
        strcat(msg, " × default <n>\t\t: Sets default groupchat room to n\n");
        strcat(msg, " × gmessage <n> <msg>\t: Sends msg to groupchat n\n");
        strcat(msg, " × leave <n>\t\t: Leaves groupchat n\n");
        strcat(msg, " × master <id>\t\t: Adds Tox ID to the masterkeys file\n");
        strcat(msg, " × name <name>\t\t: Sets name\n");
        strcat(msg, " × passwd <n> <pass>\t\t: Sets password for groupchat n (leave pass blank for no password)\n");
        strcat(msg, " × purge <n>\t\t: Sets the number of days before an inactive friend is deleted\n");
        strcat(msg, " × status <s>\t\t: Sets status (online, busy or away)\n");
        strcat(msg, " × statusmessage <msg>\t: Sets status message\n");
        strcat(msg, " × title <n> <msg>\t\t: Sets title for groupchat n");
        send_msg(m, friendnum, msg);
    }
}

static void cmd_id(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    char outmsg[TOX_ADDRESS_SIZE * 2 + 1];
    char address[TOX_ADDRESS_SIZE];
    tox_self_get_address(m, (uint8_t *) address);
    int i;

    for (i = 0; i < TOX_ADDRESS_SIZE; ++i) {
        char d[3];
        sprintf(d, "%02X", address[i] & 0xff);
        memcpy(outmsg + i * 2, d, 2);
    }

    outmsg[TOX_ADDRESS_SIZE * 2] = '\0';
    send_msg(m, friendnum, outmsg);
}

static void cmd_info(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    char outmsg[MAX_COMMAND_LENGTH];
    char timestr[64];
    char msg[MAX_COMMAND_LENGTH];

    uint64_t curtime = (uint64_t) time(NULL);
    get_elapsed_time_str(timestr, sizeof(timestr), curtime - Tox_Bot.start_time);
    snprintf(msg, sizeof(msg), "Uptime: %s\n", timestr);
    strcpy(outmsg, msg);
    // send_msg(m, friendnum, outmsg);

    uint32_t numfriends = tox_self_get_friend_list_size(m);
    snprintf(msg, sizeof(msg), "Friends: %d (%d online)\n", numfriends, Tox_Bot.num_online_friends);
    strcat(outmsg, msg);
    // send_msg(m, friendnum, outmsg);

    snprintf(msg, sizeof(msg), "Inactive friends are purged after %"PRIu64" days\n",
                                      Tox_Bot.inactive_limit / SECONDS_IN_DAY);
    strcat(outmsg, msg);
    // send_msg(m, friendnum, outmsg);

    strcat(outmsg, "Tox ID of admin of this bot is: 06F0A900ECAD7402F60E8F17D04AFE0778E1AD4AD254A7DC9E5425123A31686BA9C6F868789A");
    send_msg(m, friendnum, outmsg);

    /* List active group chats and number of peers in each */
    uint32_t numchats = tox_count_chatlist(m);

    if (numchats == 0) {
		send_msg(m, friendnum, "No active groupchats");
        return;
    }

    int32_t *groupchat_list = malloc(numchats * sizeof(int32_t));

    if (groupchat_list == NULL)
        exit(EXIT_FAILURE);

    if (tox_get_chatlist(m, groupchat_list, numchats) == 0) {
        free(groupchat_list);
        return;
    }

    uint32_t i;

    strcpy(outmsg, "");
    for (i = 0; i < numchats; ++i) {
        uint32_t groupnum = groupchat_list[i];
        int num_peers = tox_group_number_peers(m, groupnum);

        if (num_peers != -1) {
            int idx = group_index(groupnum);
            const char *title = Tox_Bot.g_chats[idx].title_len
                              ? Tox_Bot.g_chats[idx].title : "None";
            const char *type = tox_group_get_type(m, groupnum) == TOX_GROUPCHAT_TYPE_TEXT ? "Text" : "Audio";
            snprintf(msg, sizeof(msg), "Group %d | %s | peers: %d | Title: %s\n", groupnum, type, num_peers, title);
            strcat(outmsg, msg);
        }
    }
    send_msg(m, friendnum, outmsg);

    free(groupchat_list);
}

static void cmd_invite(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    int groupnum = Tox_Bot.default_groupnum;

    if (argc >= 1) {
        groupnum = atoi(argv[1]);

        if (groupnum == 0 && strcmp(argv[1], "0")) {
			send_msg(m, friendnum, "Error: Invalid group number");
            return;
        }
    }

    int idx = group_index(groupnum);

    if (idx == -1) {
		send_msg(m, friendnum, "Group doesn't exist.");
        return;
    }

    int has_pass = Tox_Bot.g_chats[idx].has_pass;

    char name[TOX_MAX_NAME_LENGTH];
    tox_friend_get_name(m, friendnum, (uint8_t *) name, NULL);
    size_t len = tox_friend_get_name_size(m, friendnum, NULL);
    name[len] = '\0';

    const char *passwd = NULL;

    if (argc >= 2)
        passwd = argv[2];

    if (has_pass && (!passwd || strcmp(argv[2], Tox_Bot.g_chats[idx].password) != 0)) {
        fprintf(stderr, "Failed to invite %s to group %d (invalid password)\n", name, groupnum);
		send_msg(m, friendnum, "Invalid password");
        return;
    }

    if (tox_invite_friend(m, friendnum, groupnum) == -1) {
        fprintf(stderr, "Failed to invite %s to group %d\n", name, groupnum);
		send_msg(m, friendnum, "Invite failed.");
        return;
    }

    printf("Invited %s to group %d\n", name, groupnum);
}

static void cmd_leave(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
		send_msg(m, friendnum, "Error: Group number required");
        return;
    }

    int groupnum = atoi(argv[1]);

    if (groupnum == 0 && strcmp(argv[1], "0")) {
		send_msg(m, friendnum, "Error: Invalid group number");
        return;
    }

    if (tox_del_groupchat(m, groupnum) == -1) {
		send_msg(m, friendnum, "Error: Invalid group number");
        return;
    }

    char msg[MAX_COMMAND_LENGTH];

    char name[TOX_MAX_NAME_LENGTH];
    tox_friend_get_name(m, friendnum, (uint8_t *) name, NULL);
    size_t len = tox_friend_get_name_size(m, friendnum, NULL);
    name[len] = '\0';

    group_leave(groupnum);

    printf("Left group %d (%s)\n", groupnum, name);
    snprintf(msg, sizeof(msg), "Left group %d", groupnum);
	send_msg(m, friendnum, msg);
}

static void cmd_master(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
		send_msg(m, friendnum, "Error: Tox ID required");
        return;
    }

    const char *id = argv[1];

    if (strlen(id) != TOX_ADDRESS_SIZE * 2) {
		send_msg(m, friendnum, "Error: Invalid Tox ID");
        return;
    }

    FILE *fp = fopen(MASTERLIST_FILE, "a");

    if (fp == NULL) {
		send_msg(m, friendnum, "Error: could not find masterkeys file");
        return;
    }

    fprintf(fp, "%s\n", id);
    fclose(fp);

    char name[TOX_MAX_NAME_LENGTH];
    tox_friend_get_name(m, friendnum, (uint8_t *) name, NULL);
    size_t len = tox_friend_get_name_size(m, friendnum, NULL);
    name[len] = '\0';

    printf("%s added master: %s\n", name, id);
	send_msg(m, friendnum, "ID added to masterkeys list");
}

static void cmd_name(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
		send_msg(m, friendnum, "Error: Name required");
        return;
    }

    char name[TOX_MAX_NAME_LENGTH];
    int len = 0;

    if (argv[1][0] == '\"') {    /* remove opening and closing quotes */
        snprintf(name, sizeof(name), "%s", &argv[1][1]);
        len = strlen(name) - 1;
    } else {
        snprintf(name, sizeof(name), "%s", argv[1]);
        len = strlen(name);
    }

    name[len] = '\0';
    tox_self_set_name(m, (uint8_t *) name, (uint16_t) len, NULL);

    char m_name[TOX_MAX_NAME_LENGTH];
    tox_friend_get_name(m, friendnum, (uint8_t *) m_name, NULL);
    size_t nlen = tox_friend_get_name_size(m, friendnum, NULL);
    m_name[nlen] = '\0';

    printf("%s set name to %s\n", m_name, name);
    save_data(m, DATA_FILE);
}

static void cmd_passwd(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
		send_msg(m, friendnum, "Error: group number required");
        return;
    }

    int groupnum = atoi(argv[1]);

    if (groupnum == 0 && strcmp(argv[1], "0")) {
		send_msg(m, friendnum, "Error: Invalid group number");
        return;
    }

    int idx = group_index(groupnum);

    if (idx == -1) {
		send_msg(m, friendnum, "Error: Invalid group number");
        return;
    }

    char name[TOX_MAX_NAME_LENGTH];
    tox_friend_get_name(m, friendnum, (uint8_t *) name, NULL);
    size_t nlen = tox_friend_get_name_size(m, friendnum, NULL);
    name[nlen] = '\0';


    /* no password */
    if (argc < 2) {
        Tox_Bot.g_chats[idx].has_pass = false;
        memset(Tox_Bot.g_chats[idx].password, 0, MAX_PASSWORD_SIZE);

		send_msg(m, friendnum, "No password set");
        printf("No password set for group %d by %s\n", groupnum, name);
        return;
    }

    if (strlen(argv[2]) >= MAX_PASSWORD_SIZE) {
		send_msg(m, friendnum, "Password too long");
        return;
    }

    Tox_Bot.g_chats[idx].has_pass = true;
    snprintf(Tox_Bot.g_chats[idx].password, sizeof(Tox_Bot.g_chats[idx].password), "%s", argv[2]);

	send_msg(m, friendnum, "Password set");
    printf("Password for group %d set by %s\n", groupnum, name);
}

static void cmd_purge(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
		send_msg(m, friendnum, "Error: number > 0 required");
        return;
    }

    uint64_t days = (uint64_t) atoi(argv[1]);

    if (days <= 0) {
		send_msg(m, friendnum, "Error: number > 0 required");
        return;
    }

    uint64_t seconds = days * SECONDS_IN_DAY;
    Tox_Bot.inactive_limit = seconds;

    char name[TOX_MAX_NAME_LENGTH];
    tox_friend_get_name(m, friendnum, (uint8_t *) name, NULL);
    size_t nlen = tox_friend_get_name_size(m, friendnum, NULL);
    name[nlen] = '\0';

    char msg[MAX_COMMAND_LENGTH];
    snprintf(msg, sizeof(msg), "Purge time set to %"PRIu64" days", days);
	send_msg(m, friendnum, msg);

    printf("Purge time set to %"PRIu64" days by %s\n", days, name);
}

static void cmd_status(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
		send_msg(m, friendnum, "Error: status required");
        return;
    }

    TOX_USER_STATUS type;
    const char *status = argv[1];

    if (strcasecmp(status, "online") == 0)
        type = TOX_USER_STATUS_NONE;
    else if (strcasecmp(status, "away") == 0)
        type = TOX_USER_STATUS_AWAY;
    else if (strcasecmp(status, "busy") == 0)
        type = TOX_USER_STATUS_BUSY;
    else {
		send_msg(m, friendnum, "Invalid status. Valid statuses are: online, busy and away.");
        return;
    }

    tox_self_set_status(m, type);

    char name[TOX_MAX_NAME_LENGTH];
    tox_friend_get_name(m, friendnum, (uint8_t *) name, NULL);
    size_t nlen = tox_friend_get_name_size(m, friendnum, NULL);
    name[nlen] = '\0';

    printf("%s set status to %s\n", name, status);
    save_data(m, DATA_FILE);
}

static void cmd_statusmessage(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
		send_msg(m, friendnum, "Error: message required");
        return;
    }

    if (argv[1][0] != '\"') {
		send_msg(m, friendnum, "Error: message must be enclosed in quotes");
        return;
    }

    /* remove opening and closing quotes */
    char msg[MAX_COMMAND_LENGTH];
    snprintf(msg, sizeof(msg), "%s", &argv[1][1]);
    int len = strlen(msg) - 1;
    msg[len] = '\0';

    tox_self_set_status_message(m, (uint8_t *) msg, len, NULL);

    char name[TOX_MAX_NAME_LENGTH];
    tox_friend_get_name(m, friendnum, (uint8_t *) name, NULL);
    size_t nlen = tox_friend_get_name_size(m, friendnum, NULL);
    name[nlen] = '\0';

    printf("%s set status message to \"%s\"\n", name, msg);
    save_data(m, DATA_FILE);
}

static void cmd_title_set(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 2) {
		send_msg(m, friendnum, "Error: Two arguments are required");
        return;
    }

    if (argv[2][0] != '\"') {
		send_msg(m, friendnum, "Error: title must be enclosed in quotes");
        return;
    }

    int groupnum = atoi(argv[1]);

    if (groupnum == 0 && strcmp(argv[1], "0")) {
		send_msg(m, friendnum, "Error: Invalid group number");
        return;
    }

    /* remove opening and closing quotes */
    char title[MAX_COMMAND_LENGTH];
    snprintf(title, sizeof(title), "%s", &argv[2][1]);
    int len = strlen(title) - 1;
    title[len] = '\0';

    char name[TOX_MAX_NAME_LENGTH];
    tox_friend_get_name(m, friendnum, (uint8_t *) name, NULL);
    size_t nlen = tox_friend_get_name_size(m, friendnum, NULL);
    name[nlen] = '\0';

    if (tox_group_set_title(m, groupnum, (uint8_t *) title, len) != 0) {
		send_msg(m, friendnum, "Failed to set title. This may be caused by an invalid group number or an empty room");
        printf("%s failed to set the title '%s' for group %d\n", name, title, groupnum);
        return;
    }

    int idx = group_index(groupnum);
    memcpy(Tox_Bot.g_chats[idx].title, title, len + 1);
    Tox_Bot.g_chats[idx].title_len = len;

	send_msg(m, friendnum, "Group title set");
    printf("%s set group %d title to %s\n", name, groupnum, title);
}

/* Parses input command and puts args into arg array.
   Returns number of arguments on success, -1 on failure. */
static int parse_command(const char *input, char (*args)[MAX_COMMAND_LENGTH])
{
    char *cmd = strdup(input);

    if (cmd == NULL)
        exit(EXIT_FAILURE);

    int num_args = 0;
    int i = 0;    /* index of last char in an argument */

    /* characters wrapped in double quotes count as one arg */
    while (num_args < MAX_NUM_ARGS) {
        int qt_ofst = 0;    /* set to 1 to offset index for quote char at end of arg */

        if (*cmd == '\"') {
            qt_ofst = 1;
            i = char_find(1, cmd, '\"');

            if (cmd[i] == '\0') {
                free(cmd);
                return -1;
            }
        } else {
            i = char_find(0, cmd, ' ');
        }

        memcpy(args[num_args], cmd, i + qt_ofst);
        args[num_args++][i + qt_ofst] = '\0';

        if (cmd[i] == '\0')    /* no more args */
            break;

        char tmp[MAX_COMMAND_LENGTH];
        snprintf(tmp, sizeof(tmp), "%s", &cmd[i + 1]);
        strcpy(cmd, tmp);    /* tmp will always fit inside cmd */
    }

    free(cmd);
    return num_args;
}

static struct {
    const char *name;
    void (*func)(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH]);
} commands[] = {
    { "default",          cmd_default       },
    { "group",            cmd_group         },
    { "gmessage",         cmd_gmessage      },
    { "help",             cmd_help          },
    { "id",               cmd_id            },
    { "info",             cmd_info          },
    { "invite",           cmd_invite        },
    { "leave",            cmd_leave         },
    { "master",           cmd_master        },
    { "name",             cmd_name          },
    { "passwd",           cmd_passwd        },
    { "purge",            cmd_purge         },
    { "status",           cmd_status        },
    { "statusmessage",    cmd_statusmessage },
    { "title",            cmd_title_set     },
    { NULL,               NULL              },
};

static int do_command(Tox *m, int friendnum, int num_args, char (*args)[MAX_COMMAND_LENGTH])
{
    int i;

    for (i = 0; commands[i].name; ++i) {
        if (strcmp(args[0], commands[i].name) == 0) {
            (commands[i].func)(m, friendnum, num_args - 1, args);
            return 0;
        }
    }

    return -1;
}

int execute(Tox *m, int friendnum, const char *input, int length)
{
    if (length >= MAX_COMMAND_LENGTH)
        return -1;

    char args[MAX_NUM_ARGS][MAX_COMMAND_LENGTH];
    int num_args = parse_command(input, args);

    if (num_args == -1)
        return -1;

    return do_command(m, friendnum, num_args, args);
}
