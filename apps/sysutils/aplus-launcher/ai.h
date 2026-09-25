/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2019 Antonino Natale
 *
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

#ifndef _APLUS_LAUNCHER_AI_H
#define _APLUS_LAUNCHER_AI_H

#include <stddef.h>


/**
 * @brief How much of an answer, of a command and of a complaint is kept.
 */
#define LAUNCHER_AI_ANSWER_MAX  2048
#define LAUNCHER_AI_COMMAND_MAX 512
#define LAUNCHER_AI_ERROR_MAX   256


/**
 * @brief What the model said, as the two things the launcher knows how to show.
 *
 * A model that was asked something no command answers leaves the command empty, which is
 * the ordinary case rather than a failure.
 */

typedef struct {

    char answer[LAUNCHER_AI_ANSWER_MAX];
    char command[LAUNCHER_AI_COMMAND_MAX];

    char error[LAUNCHER_AI_ERROR_MAX];

} launcher_ai_reply_t;


/**
 * @brief Asks the model a question, blocking until it answers, fails or runs out of time.
 *
 * @param prompt What to ask.
 * @param out Receives the answer, or a message for the user in its error when this fails.
 * @return 0 when the model answered, -1 otherwise.
 */
int launcher_ai_ask(const char* prompt, launcher_ai_reply_t* out);

#endif
