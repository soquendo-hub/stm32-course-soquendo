/* Core/common/commands.c — lo que hace cada comando*/
#include <stdio.h>
#include <stdlib.h>
#include "stm32f4xx_hal.h"
#include "commands.h"
#include "heartbeat.h"

static void cmd_blink(const cmd_t *cmd, char *reply, size_t size);
static void cmd_echo(const cmd_t *cmd, char *reply, size_t size);

/* Ejecuta un comando ya analizado y escribe su respuesta en reply (una línea).
   El enum + switch hace que cada llamada sea directa: sin punteros a función. */
void commands_execute(const cmd_t *cmd, char *reply, size_t size)
{
    if (cmd->too_many_args) {
        (void)snprintf(reply, size, "error: too many arguments");
        return;
    }
    switch (cmd->id) {
    case CMD_HELP:
        (void)snprintf(reply, size, "commands: help, info, blink <ms>, echo <text>");
        break;
    case CMD_INFO:
        (void)snprintf(reply, size, "uptime %lu ms, blink %lu ms",
                       (unsigned long)HAL_GetTick(), (unsigned long)heartbeat_get_period());
        break;
    case CMD_BLINK:
        cmd_blink(cmd, reply, size);
        break;
    case CMD_ECHO:
        cmd_echo(cmd, reply, size);
        break;
    case CMD_UNKNOWN:
    default:
        (void)snprintf(reply, size, "error: unknown command '%s' (type help)", cmd->name);
        break;
    }
}

/* "blink <ms>": verifica que el argumento sea un número entero dentro del
   rango permitido, y luego cambia el periodo del heartbeat. */
static void cmd_blink(const cmd_t *cmd, char *reply, size_t size)
{
    char *end = NULL;
    long period_ms;

    if (cmd->argc != 1U) {
        (void)snprintf(reply, size, "usage: blink <ms>");
        return;
    }
    period_ms = strtol(cmd->argv[0], &end, 10);
    if ((end == cmd->argv[0]) || (*end != '\0')) {
        (void)snprintf(reply, size, "error: '%s' is not a number", cmd->argv[0]);
    } else if ((period_ms < 0L) || !heartbeat_set_period((uint32_t)period_ms)) {
        (void)snprintf(reply, size, "error: blink accepts %u to %u ms",
                       HEARTBEAT_MIN_MS, HEARTBEAT_MAX_MS);
    } else {
        (void)snprintf(reply, size, "ok: blink %ld ms", period_ms);
    }
}

/* "echo <text>": copia los argumentos de vuelta, separados por un espacio.
   Se detiene antes si reply se llena; el texto siempre termina en '\0'. */
static void cmd_echo(const cmd_t *cmd, char *reply, size_t size)
{
    size_t used = 0U;                      /* caracteres que ya están en reply */

    reply[0] = '\0';                       /* sin argumentos: una respuesta vacía */
    for (uint8_t i = 0U; i < cmd->argc; i++) {
        /* en el primer lugar libre: un espacio (excepto antes de la palabra 1), luego la palabra */
        int written = snprintf(&reply[used], size - used, "%s%s",
                               (i == 0U) ? "" : " ", cmd->argv[i]);
        /* snprintf retorna cuántos caracteres manejó, sin el '\0'.
           Negativo = error; igual o mayor que el espacio que queda = cortado: parar aquí */
        if ((written < 0) || ((size_t)written >= (size - used))) {
            break;
        }
        used += (size_t)written;           /* la siguiente palabra va justo después de esta */
    }
}