/* analiza comandos de texto sin depender del hardware; puede compilarse en un PC */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <cmd_parser.h>
#include <sys/_intsup.h>

/* relaciona la palabra que se escribe con el ID que usa commands.c */
typedef struct
{
    const char *name; // nombre del comando
    cmd_id_t    id;   // identificador 
} cmd_entry_t;

/* aquí se agregan las palabras que el analizador debe reconocer */
static const cmd_entry_t cmd_table[] =
{
    { "help",  CMD_HELP  },
    { "info",  CMD_INFO  },
    { "blink", CMD_BLINK },
    { "echo",  CMD_ECHO  },
    { "pwm",   CMD_PWM   },
    { "R",     CMD_RED   },
    { "G",     CMD_GREEN },
    { "B",     CMD_BLUE  },
};

#define CMD_TABLE_LEN (sizeof cmd_table / sizeof cmd_table[0]) /* número de entradas sin fijarlo a mano */

static bool is_separator(char c);

/*
divide la línea directamente en su buffer: cada separador se reemplaza por '\0'
la primera palabra busca el comando; las siguientes se guardan como argumentos
devuelve false cuando no había ninguna palabra que analizar
*/
bool cmd_parse(char *line, cmd_t *cmd)
{
    const char *words[CMD_MAX_ARGS + 1U]; // una palabra para el nombre más los argumentos
    uint8_t word_count = 0U;              // cantidad de palabras encontradas hasta ahora
    char *cursor = line;                  // avanza por la línea y marca dónde inicia cada palabra

    cmd->too_many_args = false;           // empezar suponiendo que todos los argumentos caben

    /* 1. buscar palabras y poner '\0' donde estaban los separadores */
    while (*cursor != '\0')
    {
        if (is_separator(*cursor))
        {
            *cursor = '\0';              // termina la palabra actual (o ignora espacios repetidos)
            cursor++;                      // continuar después del separador
        } else if (word_count == (CMD_MAX_ARGS + 1U))
        {
            cmd->too_many_args = true;    // ya había nombre + máximo de argumentos; hay más texto
            break;
        } else 
        {
            words[word_count] = cursor;   // guardar el inicio de esta palabra
            word_count++;                 // contarla antes de avanzar hasta su final
            /* saltar todos los caracteres de la palabra actual */
            while ((*cursor != '\0') && !is_separator((*cursor)))
            {
                cursor++;
            }
        }
    }
    if (word_count == 0U) {
        return false;                     // no hay comando; no se llama a commands_execute()
    }

    /* 2. guardar el nombre y buscar su ID en la tabla */
    cmd->name = words[0];                 // la palabra inicial siempre es el nombre del comando
    cmd->id = CMD_UNKNOWN;                // si no hay coincidencia, se conservará como desconocido
    for (size_t i = 0U; i < CMD_TABLE_LEN; i++) 
    {
        if (strcmp(words[0], cmd_table[i].name) == 0) {
            cmd->id = cmd_table[i].id;    // guardar el ID que corresponde a esa palabra
            break;
        }
    }


    /* 3. las palabras posteriores al nombre son los argumentos del comando */
    cmd->argc = (uint8_t)(word_count - 1U); // restar la primera palabra, que es el nombre
    for (uint8_t i = 0U; i < cmd->argc; i++) {
        cmd->argv[i] = words[i + 1U];     // copiar cada puntero, no el texto de la palabra
    }
    return true;                          // se encontró al menos el nombre de un comando
}

/* indica qué caracteres separan palabras dentro de una línea de comandos */
static bool is_separator(char c)
{
    return (c == ' ') || (c == '\t') || (c == '\r') || (c == '\n');
}
