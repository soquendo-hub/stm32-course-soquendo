/* Core/common/cmd_parser.h — divide una línea de texto en un comando y sus argumentos */
#ifndef CMD_PARSER_H
#define CMD_PARSER_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/_intsup.h>

#define CMD_MAX_ARGS  4U

typedef enum
{
    CMD_HELP,
    CMD_INFO,
    CMD_BLINK,
    CMD_ECHO,
    CMD_UNKNOWN
}cmd_id_t;

typedef struct
{
    cmd_id_t    id;                 //Cuál comando (CMD_UNKNOWN si no está en la tabla)
    const char *name;               //La primera palabra
    uint8_t     argc;               //Número de argumentos después de la primer palabra
    const char *argv[CMD_MAX_ARGS]; //Los argumentos, cada uno termina en '\0'
    bool        too_many_args;
} cmd_t;

/*Divide 'line en el sitio: los separadores pasan a ser '\0'. Devuelve false para una línea vacía*/
bool cmd_parse(char *line, cmd_t *cmd);

#endif