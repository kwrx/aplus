/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2018 Antonino Natale
 * 
 * 
 * This file is part of aPlus.
 * 
 * aPlus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aPlus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aPlus.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef __MQUEUE_H
#define __MQUEUE_H

#include <stddef.h>

#define _POSIX_REALTIME_SIGNALS
#include <signal.h>

#include <sys/types.h>
#include <sys/fcntl.h>


typedef int mqd_t;

struct mq_attr {
  long mq_flags;    /* message queue flags */
  long mq_maxmsg;   /* maximum number of messages */
  long mq_msgsize;  /* maximum message size */
  long mq_curmsgs;  /* number of messages currently queued */
};

#define MQ_PRIO_MAX 16

mqd_t mq_open (const char *__name, int __oflag, ...);
int mq_close (mqd_t __msgid);
int mq_send (mqd_t __msgid, const char *__msg, size_t __msg_len, unsigned int __msg_prio);
ssize_t mq_receive (mqd_t __msgid, char *__msg, size_t __msg_len, unsigned int *__msg_prio);
int mq_notify (mqd_t __msgid, const struct sigevent *__notification);
int mq_unlink (const char *__name);
int mq_getattr (mqd_t __msgid, struct mq_attr *__mqstat);
int mq_setattr (mqd_t __msgid, const struct mq_attr *__mqstat, struct mq_attr *__omqattr);

#endif /* __MQUEUE_H */