/* core/common/cmd_parser.h — tipos e interfaz para convertir una línea en un comando */
#ifndef CMD_PARSER_H
#define CMD_PARSER_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/_intsup.h>

#define CMD_MAX_ARGS  4U /* máximo de argumentos que cmd_t puede guardar */

/* identificador interno que representa cada comando reconocido */
typedef enum
{
    CMD_HELP,       // imprime  lista de comandos 
    CMD_INFO,       // muestra uptime y blinky
    CMD_BLINK,      // cambia el periodo del LED heartbeat
    CMD_ECHO,       // devuelve el texto recibido
    CMD_PWM,        // cambia el porcentaje de PWM
    CMD_UNKNOWN     // comando desconocido
}cmd_id_t;

/* resultado del análisis de una línea. Los punteros apuntan dentro de line[] */
typedef struct
{
    cmd_id_t    id;                 // identificador obtenido al buscar el nombre en la tabla
    const char *name;               // comando
    uint8_t     argc;               // cantidad de argumentos que siguen el comando
    const char *argv[CMD_MAX_ARGS]; // punteros a cada argumento
    bool        too_many_args;      // = true si había más argumentos que los que caben en argv
} cmd_t;

/*

* divide line en palabras reemplazando separadores por '\0', asigna el ID del
* comando y guarda sus argumentos en cmd. retorna false si la línea está vacía

*/
bool cmd_parse(char *line, cmd_t *cmd);

#endif
