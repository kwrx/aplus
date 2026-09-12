/*
 * GPL3 License
 *
 * Author(s):
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 *
 * Copyright (c) 2013-2019 Antonino Natale
 *
 * This file is part of aplus.
 *
 * aplus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * aplus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with aplus.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * A small line-oriented IRC client (RFC 1459 / RFC 2812).
 *
 * The whole thing is one select() loop over two descriptors: the terminal and the server
 * socket. Neither side is allowed to block the other -- a client that read() the keyboard
 * while the server had traffic waiting would stall the connection, and one that drained the
 * socket first would ignore the keyboard for as long as the channel stayed busy.
 *
 * The terminal is left in canonical mode, so the tty line discipline does the editing and a
 * line arrives here already assembled. That is the whole reason this stays small: no raw
 * mode, no cursor arithmetic, no redraw. The cost is that a message arriving while a line is
 * half-typed prints straight through it. The text is still in the kernel's line buffer and
 * still sends correctly on Return.
 *
 * Both directions are framed rather than assumed. TCP hands over arbitrary chunks, so an IRC
 * line can arrive split across two recv() calls or three lines can arrive in one; the same
 * goes for the terminal. Each side accumulates bytes and releases whole lines.
 */

#if CONFIG_HAVE_NETWORK

    #include <ctype.h>
    #include <errno.h>
    #include <getopt.h>
    #include <signal.h>
    #include <stdarg.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <strings.h>
    #include <time.h>
    #include <unistd.h>

    #include <netdb.h>
    #include <netinet/in.h>
    #include <sys/select.h>
    #include <sys/socket.h>
    #include <sys/types.h>


    /* An IRC line is at most 512 bytes on the wire, CRLF included (RFC 1459 2.3). Anything
       longer is the server's problem, not ours, but it still has to be absorbed without
       running off the end of a buffer. */
    #define IRC_LINE_MAX  512
    #define IRC_PARAM_MAX 16
    #define IRC_NAME_MAX  128
    #define IRC_TEXT_MAX  IRC_LINE_MAX

    #define IRC_CTCP '\x01'

    #define IRC_VERSION "aplus-irc 1.0"


    /* Colours are emitted only when stdout is a terminal, so redirecting to a file gives
       clean text. col() is the only thing that ever tests the flag. */
    #define C_RESET "\033[0m"
    #define C_TIME  "\033[0;90m"
    #define C_NICK  "\033[1;36m"
    #define C_SELF  "\033[1;32m"
    #define C_CHAN  "\033[1;35m"
    #define C_EVENT "\033[0;33m"
    #define C_ERROR "\033[1;31m"
    #define C_INFO  "\033[0;36m"


struct irc_message {

    char* prefix;  /* servername or nick!user@host, without the leading ':' */
    char* command; /* verb or three-digit numeric */
    char* argv[IRC_PARAM_MAX];
    int argc;
};


static struct {

    int sock;

    char nick[IRC_NAME_MAX];
    char user[IRC_NAME_MAX];
    char real[IRC_NAME_MAX];
    char target[IRC_NAME_MAX];   /* where a bare line of text goes */
    char autojoin[IRC_TEXT_MAX]; /* channels to join once registered */

    int registered;
    int color;
    int quit;

} irc;


static const char* col(const char* code) {
    return irc.color ? code : "";
}


/*
 * strncpy() leaves the destination unterminated exactly when the source was too long, which
 * is the case that matters. Everything here copies through this instead.
 */
static void str_set(char* dst, size_t size, const char* src) {

    if (size == 0)
        return;

    size_t i = 0;

    for (; i < size - 1 && src[i]; i++)
        dst[i] = src[i];

    dst[i] = '\0';
}


/*
 * One output line, stamped with the local time. Everything the user sees goes through here so
 * the stamp and the trailing newline are never forgotten, and so output stays unbuffered --
 * a client whose messages appear a screenful late is worse than useless.
 */
static void irc_print(const char* fmt, ...) {

    char stamp[16];

    time_t now = time(NULL);
    struct tm tm;

    if (localtime_r(&now, &tm))
        snprintf(stamp, sizeof(stamp), "%02d:%02d", tm.tm_hour, tm.tm_min);
    else
        str_set(stamp, sizeof(stamp), "--:--");


    printf("%s[%s]%s ", col(C_TIME), stamp, col(C_RESET));

    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);

    printf("%s\n", col(C_RESET));

    fflush(stdout);
}


    #define irc_event(fmt, ...) irc_print("%s-- " fmt, col(C_EVENT), __VA_ARGS__)
    #define irc_error(fmt, ...) irc_print("%s!! " fmt, col(C_ERROR), __VA_ARGS__)
    #define irc_info(fmt, ...)  irc_print("%s** " fmt, col(C_INFO), __VA_ARGS__)


/*
 * send() is allowed to place only part of a buffer, and a short write in the middle of an IRC
 * line would desynchronise the server's parser for the rest of the session.
 */
static int irc_send_all(const char* data, size_t len) {

    size_t off = 0;

    while (off < len) {

        ssize_t n = send(irc.sock, data + off, len - off, 0);

        if (n < 0) {

            if (errno == EINTR)
                continue;

            return -1;
        }

        if (n == 0)
            return -1;

        off += (size_t)n;
    }

    return 0;
}


/*
 * Format one command and put it on the wire with its CRLF. The formatted text is capped two
 * bytes below the line limit so that the terminator always fits: a line that lost its CRLF to
 * truncation would swallow the line after it.
 */
static int irc_sendf(const char* fmt, ...) {

    char line[IRC_LINE_MAX];

    const size_t cap = sizeof(line) - 2;

    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(line, cap, fmt, ap);
    va_end(ap);

    if (n < 0)
        return -1;

    if ((size_t)n >= cap)
        n = (int)cap - 1;

    line[n++] = '\r';
    line[n++] = '\n';

    if (irc_send_all(line, (size_t)n) < 0) {

        irc_error("send: %s", strerror(errno));

        irc.quit = 1;
        return -1;
    }

    return 0;
}


/*
 * Split a line in place. Returns 0 on success, -1 if there was no command to be found.
 *
 * The trailing parameter is the one introduced by ':' and is the only one allowed to contain
 * spaces; it is always last, so the scan stops there.
 */
static int irc_parse(char* line, struct irc_message* m) {

    m->prefix  = NULL;
    m->command = NULL;
    m->argc    = 0;

    char* p = line;

    while (*p == ' ')
        p++;

    if (*p == ':') {

        m->prefix = ++p;

        while (*p && *p != ' ')
            p++;

        if (*p)
            *p++ = '\0';

        while (*p == ' ')
            p++;
    }

    if (!*p)
        return -1;

    m->command = p;

    while (*p && *p != ' ')
        p++;

    if (*p)
        *p++ = '\0';


    while (*p) {

        while (*p == ' ')
            p++;

        if (!*p)
            break;

        if (*p == ':') {

            if (m->argc < IRC_PARAM_MAX)
                m->argv[m->argc++] = p + 1;

            break;
        }

        if (m->argc < IRC_PARAM_MAX)
            m->argv[m->argc++] = p;

        while (*p && *p != ' ')
            p++;

        if (*p)
            *p++ = '\0';
    }

    return 0;
}


/*
 * The nick out of a nick!user@host prefix. Server-sourced messages have no '!' and come back
 * whole, which is what we want to show for them anyway.
 */
static const char* irc_nick_of(const char* prefix) {

    static char nick[IRC_NAME_MAX];

    if (!prefix)
        return "*";

    size_t i = 0;

    for (; i < sizeof(nick) - 1 && prefix[i] && prefix[i] != '!' && prefix[i] != '@'; i++)
        nick[i] = prefix[i];

    nick[i] = '\0';

    return nick;
}


static int irc_is_channel(const char* name) {
    return name[0] == '#' || name[0] == '&' || name[0] == '+' || name[0] == '!';
}


/*
 * Users type "aplus" and mean "#aplus".
 */
static const char* irc_channel_name(const char* name, char* buf, size_t size) {

    if (irc_is_channel(name) || strchr(name, ','))
        str_set(buf, size, name);
    else
        snprintf(buf, size, "#%s", name);

    return buf;
}


static void irc_join_params(const struct irc_message* m, int from, char* out, size_t size) {

    size_t off = 0;

    out[0] = '\0';

    for (int i = from; i < m->argc && off < size - 1; i++) {

        int n = snprintf(out + off, size - off, "%s%s", off ? " " : "", m->argv[i]);

        if (n < 0)
            break;

        off += (size_t)n < size - off ? (size_t)n : size - off - 1;
    }
}


/* -------------------------------------------------------------------------------------- */
/* Server -> client                                                                        */
/* -------------------------------------------------------------------------------------- */

/*
 * CTCP rides inside PRIVMSG, wrapped in \x01. ACTION is the one everybody sees ("/me"); the
 * rest are queries that are answered with a NOTICE, which by convention is never replied to
 * and so cannot start a loop between two clients.
 */
static void irc_handle_ctcp(const char* from, const char* target, const char* text) {

    char ctcp[IRC_TEXT_MAX];

    str_set(ctcp, sizeof(ctcp), text + 1);

    char* end = strrchr(ctcp, IRC_CTCP);

    if (end)
        *end = '\0';


    char* args = strchr(ctcp, ' ');

    if (args)
        *args++ = '\0';


    if (strcasecmp(ctcp, "ACTION") == 0) {

        irc_print("%s%s %s* %s%s %s", col(C_CHAN), target, col(C_RESET), col(C_NICK), from, args ? args : "");

    } else if (strcasecmp(ctcp, "VERSION") == 0) {

        irc_sendf("NOTICE %s :%cVERSION %s%c", from, IRC_CTCP, IRC_VERSION, IRC_CTCP);
        irc_event("CTCP VERSION from %s", from);

    } else if (strcasecmp(ctcp, "PING") == 0) {

        irc_sendf("NOTICE %s :%cPING %s%c", from, IRC_CTCP, args ? args : "", IRC_CTCP);
        irc_event("CTCP PING from %s", from);

    } else if (strcasecmp(ctcp, "TIME") == 0) {

        char stamp[64];

        time_t now = time(NULL);
        struct tm tm;

        if (localtime_r(&now, &tm) && strftime(stamp, sizeof(stamp), "%a %b %d %H:%M:%S %Y", &tm))
            irc_sendf("NOTICE %s :%cTIME %s%c", from, IRC_CTCP, stamp, IRC_CTCP);

        irc_event("CTCP TIME from %s", from);

    } else {

        irc_event("unhandled CTCP %s from %s", ctcp, from);
    }
}


static void irc_handle_privmsg(const struct irc_message* m, int notice) {

    if (m->argc < 2)
        return;

    const char* from   = irc_nick_of(m->prefix);
    const char* target = m->argv[0];
    const char* text   = m->argv[1];

    /* A message addressed to our own nick is a private one; showing the target verbatim would
       just print our own name back at us. */
    const char* where = irc_is_channel(target) ? target : "PM";

    if (!notice && text[0] == IRC_CTCP) {
        irc_handle_ctcp(from, where, text);
        return;
    }

    if (notice)
        irc_print("%s%s %s-%s%s-%s %s", col(C_CHAN), where, col(C_RESET), col(C_NICK), from, col(C_RESET), text);
    else
        irc_print("%s%s %s<%s%s%s>%s %s", col(C_CHAN), where, col(C_RESET), col(C_NICK), from, col(C_RESET), col(C_RESET), text);
}


/*
 * Numerics 001-005 are the greeting, and 001 is the first moment the nick we asked for is
 * confirmed to be ours. Anything that depends on being registered waits for it.
 */
static void irc_handle_numeric(const struct irc_message* m, int num) {

    char text[IRC_TEXT_MAX];

    switch (num) {

        case 1: /* RPL_WELCOME */

            irc.registered = 1;

            if (m->argc > 0)
                str_set(irc.nick, sizeof(irc.nick), m->argv[0]);

            irc_join_params(m, 1, text, sizeof(text));
            irc_info("%s", text);

            if (irc.autojoin[0]) {

                irc_sendf("JOIN %s", irc.autojoin);

                /* The default target is the first of them; the rest are joined but have to be
                   selected with /target before a bare line goes there. */
                char* comma = strchr(irc.autojoin, ',');

                if (comma)
                    *comma = '\0';

                str_set(irc.target, sizeof(irc.target), irc.autojoin);
            }

            return;

        case 433: /* ERR_NICKNAMEINUSE */
        case 436: /* ERR_NICKCOLLISION */

            /* Before registration there is no session to keep, so take the next nick along and
               try again -- otherwise the connection just sits there never completing. Once
               registered the user is already on with a working nick and a silent rename would
               be worse than the error. */
            if (!irc.registered) {

                size_t len = strlen(irc.nick);

                if (len < sizeof(irc.nick) - 1) {

                    irc.nick[len]     = '_';
                    irc.nick[len + 1] = '\0';

                    irc_event("nick in use, trying %s", irc.nick);
                    irc_sendf("NICK %s", irc.nick);

                    return;
                }

                irc_error("%s", "nick in use and no room left to extend it");

                irc.quit = 1;
                return;
            }

            break;

        case 353: /* RPL_NAMREPLY: <nick> <symbol> <channel> :<names> */

            if (m->argc >= 4) {

                irc_join_params(m, 2, text, sizeof(text));
                irc_info("%s", text);

                return;
            }

            break;

        default:
            break;
    }


    /* The first parameter of a numeric is always the recipient -- us -- so it is dropped. */
    irc_join_params(m, m->argc > 1 ? 1 : 0, text, sizeof(text));

    if (num >= 400)
        irc_error("%s", text);
    else
        irc_info("%s", text);
}


static void irc_handle_message(char* line) {

    struct irc_message m;

    if (irc_parse(line, &m) < 0)
        return;


    const char* from = irc_nick_of(m.prefix);


    /* PING comes before anything else: a client that does not answer it is disconnected for
       being unresponsive, however busy it was. */
    if (strcasecmp(m.command, "PING") == 0) {

        irc_sendf("PONG :%s", m.argc > 0 ? m.argv[0] : "");
        return;
    }

    if (strcasecmp(m.command, "PRIVMSG") == 0) {
        irc_handle_privmsg(&m, 0);
        return;
    }

    if (strcasecmp(m.command, "NOTICE") == 0) {
        irc_handle_privmsg(&m, 1);
        return;
    }

    if (strcasecmp(m.command, "JOIN") == 0) {

        const char* chan = m.argc > 0 ? m.argv[0] : "?";

        /* Our own JOIN is the server confirming the channel, so that is the moment to point
           bare input at it. */
        if (strcmp(from, irc.nick) == 0) {

            str_set(irc.target, sizeof(irc.target), chan);
            irc_event("now talking in %s", chan);

        } else {

            irc_event("%s joined %s", from, chan);
        }

        return;
    }

    if (strcasecmp(m.command, "PART") == 0) {

        const char* chan = m.argc > 0 ? m.argv[0] : "?";

        if (strcmp(from, irc.nick) == 0 && strcmp(chan, irc.target) == 0)
            irc.target[0] = '\0';

        irc_event("%s left %s%s%s", from, chan, m.argc > 1 ? " " : "", m.argc > 1 ? m.argv[1] : "");
        return;
    }

    if (strcasecmp(m.command, "QUIT") == 0) {

        irc_event("%s quit%s%s", from, m.argc > 0 ? ": " : "", m.argc > 0 ? m.argv[0] : "");
        return;
    }

    if (strcasecmp(m.command, "NICK") == 0) {

        const char* to = m.argc > 0 ? m.argv[0] : "?";

        if (strcmp(from, irc.nick) == 0)
            str_set(irc.nick, sizeof(irc.nick), to);

        irc_event("%s is now known as %s", from, to);
        return;
    }

    if (strcasecmp(m.command, "KICK") == 0) {

        const char* chan = m.argc > 0 ? m.argv[0] : "?";
        const char* who  = m.argc > 1 ? m.argv[1] : "?";

        if (strcmp(who, irc.nick) == 0 && strcmp(chan, irc.target) == 0)
            irc.target[0] = '\0';

        irc_event("%s kicked %s from %s%s%s", from, who, chan, m.argc > 2 ? ": " : "", m.argc > 2 ? m.argv[2] : "");
        return;
    }

    if (strcasecmp(m.command, "TOPIC") == 0) {

        irc_event("%s set the topic of %s to: %s", from, m.argc > 0 ? m.argv[0] : "?", m.argc > 1 ? m.argv[1] : "");
        return;
    }

    if (strcasecmp(m.command, "MODE") == 0) {

        char text[IRC_TEXT_MAX];

        irc_join_params(&m, 0, text, sizeof(text));
        irc_event("%s set mode %s", from, text);

        return;
    }

    /* ERROR is the server saying goodbye, usually right before it drops the connection. */
    if (strcasecmp(m.command, "ERROR") == 0) {

        irc_error("%s", m.argc > 0 ? m.argv[0] : "server error");

        irc.quit = 1;
        return;
    }


    if (isdigit((unsigned char)m.command[0])) {
        irc_handle_numeric(&m, atoi(m.command));
        return;
    }


    {
        char text[IRC_TEXT_MAX];

        irc_join_params(&m, 0, text, sizeof(text));
        irc_event("%s %s", m.command, text);
    }
}


/* -------------------------------------------------------------------------------------- */
/* Client -> server                                                                        */
/* -------------------------------------------------------------------------------------- */

static void irc_say(const char* target, const char* text) {

    irc_sendf("PRIVMSG %s :%s", target, text);

    /* The server does not echo a message back to its sender, so the local copy is the only
       one the user will ever see. */
    irc_print("%s%s %s<%s%s%s>%s %s", col(C_CHAN), target, col(C_RESET), col(C_SELF), irc.nick, col(C_RESET), col(C_RESET), text);
}


static void irc_help(void) {

    static const char* lines[] = {
        "/join <channel>            join a channel and talk there",
        "/part [channel] [reason]   leave a channel",
        "/target <name>             send plain lines to someone else",
        "/msg <target> <text>       one message elsewhere",
        "/notice <target> <text>    send a notice",
        "/me <text>                 describe an action",
        "/nick <nick>               change nick",
        "/topic [text]              show or set the channel topic",
        "/names [channel]           who is in the channel",
        "/whois <nick>              look someone up",
        "/list                      list channels (can be long)",
        "/away [reason]             mark yourself away, or back with no reason",
        "/raw <line>                send a raw protocol line",
        "/quit [reason]             disconnect and exit",
        "/help                      this list",
        "",
        "A line that does not start with '/' is sent to the current target.",
        "Start a line with '//' to send text that begins with a slash.",
    };

    for (size_t i = 0; i < sizeof(lines) / sizeof(lines[0]); i++)
        irc_print("%s%s", col(C_INFO), lines[i]);
}


/*
 * One line typed by the user. The buffer is writable and is carved up in place.
 */
static void irc_handle_input(char* line) {

    if (!line[0])
        return;


    if (line[0] != '/' || line[1] == '/') {

        /* "//..." is the escape for a message that really does start with a slash. */
        const char* text = line[0] == '/' ? line + 1 : line;

        if (!irc.target[0]) {

            irc_error("%s", "no target: use /join <channel> or /target <nick> first");
            return;
        }

        irc_say(irc.target, text);
        return;
    }


    char empty[1] = "";

    char* cmd  = line + 1;
    char* args = strchr(cmd, ' ');

    if (args) {

        *args++ = '\0';

        while (*args == ' ')
            args++;

    } else {

        args = empty;
    }


    if (strcasecmp(cmd, "join") == 0 || strcasecmp(cmd, "j") == 0) {

        if (!args[0]) {
            irc_error("%s", "usage: /join <channel>");
            return;
        }

        char chan[IRC_NAME_MAX];

        irc_channel_name(args, chan, sizeof(chan));

        irc_sendf("JOIN %s", chan);

        /* Point bare input at the channel now rather than waiting for the server to echo the
           JOIN back. The round trip is short but not instant, and a line typed inside it would
           otherwise be refused for having no target. The echo confirms this a moment later,
           and a join that is actually refused reports itself as a numeric. */
        char* comma = strchr(chan, ',');

        if (comma)
            *comma = '\0';

        str_set(irc.target, sizeof(irc.target), chan);

    } else if (strcasecmp(cmd, "part") == 0 || strcasecmp(cmd, "leave") == 0) {

        /* With no argument, or with a reason only, the channel is the current one. */
        if (!args[0]) {

            if (!irc.target[0]) {
                irc_error("%s", "usage: /part [channel] [reason]");
                return;
            }

            irc_sendf("PART %s", irc.target);

        } else if (irc_is_channel(args)) {

            char* reason = strchr(args, ' ');

            if (reason) {

                *reason++ = '\0';
                irc_sendf("PART %s :%s", args, reason);

            } else {

                irc_sendf("PART %s", args);
            }

        } else {

            if (!irc.target[0]) {
                irc_error("%s", "usage: /part [channel] [reason]");
                return;
            }

            irc_sendf("PART %s :%s", irc.target, args);
        }

    } else if (strcasecmp(cmd, "target") == 0 || strcasecmp(cmd, "t") == 0) {

        if (!args[0]) {

            irc_info("current target: %s", irc.target[0] ? irc.target : "(none)");
            return;
        }

        str_set(irc.target, sizeof(irc.target), args);
        irc_info("now talking to %s", irc.target);

    } else if (strcasecmp(cmd, "msg") == 0 || strcasecmp(cmd, "privmsg") == 0) {

        char* text = strchr(args, ' ');

        if (!args[0] || !text) {
            irc_error("%s", "usage: /msg <target> <text>");
            return;
        }

        *text++ = '\0';

        irc_say(args, text);

    } else if (strcasecmp(cmd, "notice") == 0) {

        char* text = strchr(args, ' ');

        if (!args[0] || !text) {
            irc_error("%s", "usage: /notice <target> <text>");
            return;
        }

        *text++ = '\0';

        irc_sendf("NOTICE %s :%s", args, text);
        irc_print("%s%s %s-%s%s-%s %s", col(C_CHAN), args, col(C_RESET), col(C_SELF), irc.nick, col(C_RESET), text);

    } else if (strcasecmp(cmd, "me") == 0) {

        if (!irc.target[0]) {
            irc_error("%s", "no target: use /join <channel> or /target <nick> first");
            return;
        }

        irc_sendf("PRIVMSG %s :%cACTION %s%c", irc.target, IRC_CTCP, args, IRC_CTCP);
        irc_print("%s%s %s* %s%s %s", col(C_CHAN), irc.target, col(C_RESET), col(C_SELF), irc.nick, args);

    } else if (strcasecmp(cmd, "nick") == 0) {

        if (!args[0]) {
            irc_error("%s", "usage: /nick <nick>");
            return;
        }

        /* The rename is not recorded here; it becomes real when the server sends back the
           NICK message confirming it, and until then the old one is still ours. */
        irc_sendf("NICK %s", args);

    } else if (strcasecmp(cmd, "topic") == 0) {

        if (!irc.target[0]) {
            irc_error("%s", "no channel selected");
            return;
        }

        if (args[0])
            irc_sendf("TOPIC %s :%s", irc.target, args);
        else
            irc_sendf("TOPIC %s", irc.target);

    } else if (strcasecmp(cmd, "names") == 0) {

        const char* chan = args[0] ? args : irc.target;

        if (!chan[0]) {
            irc_error("%s", "no channel selected");
            return;
        }

        irc_sendf("NAMES %s", chan);

    } else if (strcasecmp(cmd, "whois") == 0) {

        if (!args[0]) {
            irc_error("%s", "usage: /whois <nick>");
            return;
        }

        irc_sendf("WHOIS %s", args);

    } else if (strcasecmp(cmd, "who") == 0) {

        irc_sendf("WHO %s", args[0] ? args : irc.target);

    } else if (strcasecmp(cmd, "list") == 0) {

        irc_sendf("LIST %s", args);

    } else if (strcasecmp(cmd, "away") == 0) {

        if (args[0])
            irc_sendf("AWAY :%s", args);
        else
            irc_sendf("AWAY");

    } else if (strcasecmp(cmd, "raw") == 0 || strcasecmp(cmd, "quote") == 0) {

        if (!args[0]) {
            irc_error("%s", "usage: /raw <line>");
            return;
        }

        irc_sendf("%s", args);

    } else if (strcasecmp(cmd, "quit") == 0 || strcasecmp(cmd, "exit") == 0) {

        irc_sendf("QUIT :%s", args[0] ? args : "leaving");

        irc.quit = 1;

    } else if (strcasecmp(cmd, "help") == 0 || strcasecmp(cmd, "h") == 0) {

        irc_help();

    } else {

        irc_error("unknown command: /%s (try /help)", cmd);
    }
}


/* -------------------------------------------------------------------------------------- */
/* Framing                                                                                 */
/* -------------------------------------------------------------------------------------- */

/*
 * Turn a stream of bytes into whole lines.
 *
 * A line longer than the buffer is dropped rather than truncated and delivered: half an IRC
 * message parsed as a whole one is worse than no message. The discard runs to the next
 * newline so the stream stays in sync.
 */
struct irc_framer {

    char buf[IRC_LINE_MAX * 2];
    size_t len;
    int overflow;
};


static void irc_frame(struct irc_framer* f, const char* data, size_t n, void (*on_line)(char*)) {

    for (size_t i = 0; i < n; i++) {

        if (data[i] == '\n') {

            if (!f->overflow) {

                /* The wire terminator is CRLF, but a bare LF is common enough from test
                   servers and scripts to be worth accepting. */
                while (f->len > 0 && f->buf[f->len - 1] == '\r')
                    f->len--;

                f->buf[f->len] = '\0';

                on_line(f->buf);

            } else {

                irc_error("%s", "over-long line discarded");
            }

            f->len      = 0;
            f->overflow = 0;

            continue;
        }

        if (f->len < sizeof(f->buf) - 1)
            f->buf[f->len++] = data[i];
        else
            f->overflow = 1;
    }
}


static struct irc_framer rx_server;
static struct irc_framer rx_input;


static void irc_on_server_readable(void) {

    char buf[2048];

    ssize_t n = recv(irc.sock, buf, sizeof(buf), 0);

    if (n < 0) {

        if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
            return;

        irc_error("recv: %s", strerror(errno));

        irc.quit = 1;
        return;
    }

    if (n == 0) {

        irc_error("%s", "connection closed by server");

        irc.quit = 1;
        return;
    }

    irc_frame(&rx_server, buf, (size_t)n, irc_handle_message);
}


static void irc_on_input_readable(void) {

    char buf[1024];

    ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));

    if (n < 0) {

        if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
            return;

        irc_error("read: %s", strerror(errno));

        irc.quit = 1;
        return;
    }

    /* End of input -- ^D, or a script that ran out of commands. Leave properly rather than
       spinning on a descriptor that is readable and empty forever. */
    if (n == 0) {

        irc_sendf("QUIT :leaving");

        irc.quit = 1;
        return;
    }

    irc_frame(&rx_input, buf, (size_t)n, irc_handle_input);
}


/* -------------------------------------------------------------------------------------- */
/* Connection                                                                              */
/* -------------------------------------------------------------------------------------- */

/*
 * Resolve and connect, trying every address the resolver offers. A name can map to several
 * addresses and to both families, and the first one is not always the one that answers.
 *
 * getaddrinfo() also handles a bare address literal, so a host given as "10.0.2.2" works
 * without a nameserver configured.
 */
static int irc_connect(const char* host, const char* port) {

    struct addrinfo hints;

    memset(&hints, 0, sizeof(hints));

    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* res = NULL;

    int e = getaddrinfo(host, port, &hints, &res);

    if (e != 0) {

        fprintf(stderr, "irc: cannot resolve %s: %s\n", host, gai_strerror(e));
        return -1;
    }


    int sock = -1;

    for (struct addrinfo* ai = res; ai; ai = ai->ai_next) {

        sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);

        if (sock < 0)
            continue;

        if (connect(sock, ai->ai_addr, ai->ai_addrlen) == 0)
            break;

        close(sock);
        sock = -1;
    }

    freeaddrinfo(res);

    if (sock < 0) {

        fprintf(stderr, "irc: cannot connect to %s:%s: %s\n", host, port, strerror(errno));
        return -1;
    }

    return sock;
}


static void usage(const char* argv0) {

    fprintf(stderr,
            "usage: %s [-n nick] [-u user] [-r realname] [-w pass] [-j channels] [-p port] [-C] <server> [port]\n"
            "\n"
            "  -n nick       nickname to use (default: $USER, or \"aplus\")\n"
            "  -u user       username sent at registration (default: the nick)\n"
            "  -r realname   real name sent at registration (default: the nick)\n"
            "  -w pass       server password, sent before registering\n"
            "  -j channels   channels to join once connected, comma separated\n"
            "  -p port       server port (default: 6667)\n"
            "  -C            never colour the output\n"
            "  -h            this message\n",
            argv0);
}


int main(int argc, char** argv) {

    /* A write to a socket the peer has closed raises SIGPIPE, whose default action is to kill
       the process. Ignored, the same write returns EPIPE and the error is reported instead. */
    signal(SIGPIPE, SIG_IGN);


    const char* nick = getenv("USER");
    const char* user = NULL;
    const char* real = NULL;
    const char* pass = NULL;
    const char* port = "6667";

    int color = isatty(STDOUT_FILENO);

    if (!nick || !nick[0])
        nick = "aplus";


    int opt;

    while ((opt = getopt(argc, argv, "n:u:r:w:j:p:Ch")) != -1) {

        switch (opt) {

            case 'n':
                nick = optarg;
                break;
            case 'u':
                user = optarg;
                break;
            case 'r':
                real = optarg;
                break;
            case 'w':
                pass = optarg;
                break;
            case 'p':
                port = optarg;
                break;
            case 'C':
                color = 0;
                break;
            case 'j':
                str_set(irc.autojoin, sizeof(irc.autojoin), optarg);
                break;

            case 'h':
                usage(argv[0]);
                return 0;

            default:
                usage(argv[0]);
                return 1;
        }
    }


    if (optind >= argc) {

        usage(argv[0]);
        return 1;
    }

    const char* host = argv[optind++];

    if (optind < argc)
        port = argv[optind];


    irc.color = color;

    str_set(irc.nick, sizeof(irc.nick), nick);
    str_set(irc.user, sizeof(irc.user), user ? user : nick);
    str_set(irc.real, sizeof(irc.real), real ? real : nick);


    irc_info("connecting to %s:%s as %s...", host, port, irc.nick);

    if ((irc.sock = irc_connect(host, port)) < 0)
        return 1;

    /* select() addresses descriptors by bit position, and a socket here is numbered above the
       file-descriptor range. It fits comfortably, but not checking would turn a configuration
       change into a silent write past the end of an fd_set. */
    if (irc.sock >= FD_SETSIZE) {

        fprintf(stderr, "irc: socket descriptor %d is beyond FD_SETSIZE (%d)\n", irc.sock, FD_SETSIZE);

        close(irc.sock);
        return 1;
    }

    irc_info("%s", "connected, registering");


    /* PASS has to come before NICK and USER if it is used at all (RFC 2812 3.1.1). */
    if (pass)
        irc_sendf("PASS %s", pass);

    irc_sendf("NICK %s", irc.nick);
    irc_sendf("USER %s 0 * :%s", irc.user, irc.real);

    irc_info("%s", "type /help for the list of commands");


    int nfds = (irc.sock > STDIN_FILENO ? irc.sock : STDIN_FILENO) + 1;

    while (!irc.quit) {

        fd_set rfds;

        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);
        FD_SET(irc.sock, &rfds);

        if (select(nfds, &rfds, NULL, NULL, NULL) < 0) {

            if (errno == EINTR)
                continue;

            irc_error("select: %s", strerror(errno));
            break;
        }

        /* The server first: it is the side that can time us out. */
        if (FD_ISSET(irc.sock, &rfds))
            irc_on_server_readable();

        if (!irc.quit && FD_ISSET(STDIN_FILENO, &rfds))
            irc_on_input_readable();
    }


    close(irc.sock);

    irc_info("%s", "disconnected");

    return 0;
}

#else

    #include <stdio.h>

int main(int argc, char** argv) {

    fprintf(stderr, "irc: network support is not enabled in this build\n");
    return 1;
}

#endif
