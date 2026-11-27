/* core/common/commands.c — lo que hace cada comando */
#include <stdio.h>
#include <stdlib.h>
#include "cmd_parser.h"
#include "stm32f4xx_hal.h"
#include "commands.h"
#include "heartbeat.h"
#include "pwm.h"

static void cmd_blink(const cmd_t *cmd, char *reply, size_t size);
static void cmd_echo(const cmd_t *cmd, char *reply, size_t size);
static void cmd_pwm(const cmd_t *cmd, char *reply, size_t size);

/*
recibe el comando ya separado por cmd_parser y prepara una respuesta de una línea
el switch selecciona la acción según el ID; aquí todavía no se envía nada por UART
*/
void commands_execute(const cmd_t *cmd, char *reply, size_t size)
{
    if (cmd->too_many_args) {
        (void)snprintf(reply, size, "error: too many arguments"); // rechazar antes de usar argv
        return;
    }
    /* cada case delega el trabajo específico a su manejador o arma una respuesta simple */
    switch (cmd->id) {
    case CMD_HELP:
        /* responder con los nombres y formatos de uso incluidos en esta ayuda */
        (void)snprintf(reply, size, "commands: help, info, blink <ms>, echo <text>");
        break;
    case CMD_INFO:
        /* HAL_GetTick cuenta milisegundos desde el inicio; heartbeat entrega el periodo actual */
        (void)snprintf(reply, size, "uptime %lu ms, blink %lu ms",
                       (unsigned long)HAL_GetTick(), (unsigned long)heartbeat_get_period());
        break;
    case CMD_BLINK:
        /* pasar la validación y el cambio del heartbeat al manejador blink */
        cmd_blink(cmd, reply, size);
        break;
    case CMD_ECHO:
        /* reconstruir los argumentos como texto y devolverlos al usuario */
        cmd_echo(cmd, reply, size);
        break;
    case CMD_PWM:
        /* validar el porcentaje y pedir al módulo PWM que ajuste TIM3 */
        cmd_pwm(cmd, reply, size);
        break;
    case CMD_UNKNOWN:
    default:
        /* un ID no reconocido produce una respuesta con el nombre que se escribió */
        (void)snprintf(reply, size, "error: unknown command '%s' (type help)", cmd->name);
        break;
    }
}

/* "blink <ms>": valida el periodo y pide al módulo heartbeat que cambie el parpadeo */
static void cmd_blink(const cmd_t *cmd, char *reply, size_t size)
{
    char *end = NULL;                       // strtol deja aquí el primer carácter que no convirtió
    long period_ms;                         // valor temporal para detectar también números negativos

    if (cmd->argc != 1U) {
        (void)snprintf(reply, size, "usage: blink <ms>"); // se requiere exactamente un argumento
        return;
    }
    period_ms = strtol(cmd->argv[0], &end, 10); // convertir el argumento decimal a entero
    if ((end == cmd->argv[0]) || (*end != '\0')) {
        (void)snprintf(reply, size, "error: '%s' is not a number", cmd->argv[0]); // no convirtió toda la palabra
    } else if ((period_ms < 0L) || !heartbeat_set_period((uint32_t)period_ms)) {
        /* rechazar negativos o valores fuera del rango permitido por heartbeat */
        (void)snprintf(reply, size, "error: blink accepts %u to %u ms",
                       HEARTBEAT_MIN_MS, HEARTBEAT_MAX_MS);
    } else {
        (void)snprintf(reply, size, "ok: blink %ld ms", period_ms); // confirmar el cambio aplicado
    }
}

/*
"echo <text>": copia los argumentos de vuelta, separados por un espacio
se detiene antes si reply se llena; el texto siempre termina en '\0'
*/
static void cmd_echo(const cmd_t *cmd, char *reply, size_t size)
{
    size_t used = 0U;                     

    reply[0] = '\0';                      
    for (uint8_t i = 0U; i < cmd->argc; i++) {
        /* en el primer lugar libre, agregar la palabra y un espacio si no es la primera */
        int written = snprintf(&reply[used], size - used, "%s%s",
                               (i == 0U) ? "" : " ", cmd->argv[i]);
        /*
snprintf retorna cuántos caracteres manejó, sin el '\0'
negativo = error; igual o mayor que el espacio que queda = cortado: parar aquí
*/
        if ((written < 0) || ((size_t)written >= (size - used))) {
            break;
        }
        used += (size_t)written;           
    }
}

/* "pwm <percent>": valida el porcentaje y pide al módulo pwm que ajuste TIM3 */
static void cmd_pwm(const cmd_t *cmd, char *reply, size_t size)
{
    char *end = NULL;                       
    long percent;                         

    if (cmd->argc != 1U) {
        (void)snprintf(reply, size, "usage: pwm <0..100>");
        return;
    }
    percent = strtol(cmd->argv[0], &end, 10); // leer el porcentaje escrito en base decimal
    if ((end == cmd->argv[0]) || (*end != '\0')) {
        (void)snprintf(reply, size, "error: '%s' is not a number", cmd->argv[0]);
    } else if ((percent < 0L) || (percent > 100L)) { // el ciclo de trabajo solo admite 0 a 100 %
        (void)snprintf(reply, size, "error: pwm accepts 0 to 100 %%");
    } else if (!pwm_set_duty((uint32_t)percent)) { // el módulo también comprueba que pwm_init() se llamó
        (void)snprintf(reply, size, "error: pwm is not started in this application");
    } else {
        (void)snprintf(reply, size, "ok: pwm %ld %%", percent);
    }
}
