/* core/common/commands.h — lo que hace cada comando */
#ifndef COMMANDS_H
#define COMMANDS_H

#include <stddef.h>
#include "cmd_parser.h"

/*

* ejecuta el comando ya analizado y escribe una respuesta sin CR/LF en reply
* el transporte (UART/DMA) es responsabilidad de app.c, no de este módulo

*/
void commands_execute(const cmd_t *cmd, char *reply, size_t reply_size);

#endif
