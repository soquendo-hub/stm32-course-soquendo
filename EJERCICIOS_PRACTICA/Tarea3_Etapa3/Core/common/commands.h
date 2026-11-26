/* Core/common/commands.h — lo que hace cada comando */
#ifndef COMMANDS_H
#define COMMANDS_H

#include <stddef.h>
#include "cmd_parser.h"

/* Ejecuta 'cmd' y escribe la respuesta (una línea, sin final de línea) en 'reply'. */
void commands_execute(const cmd_t *cmd, char *reply, size_t reply_size);

#endif