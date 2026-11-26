/*hardware en este archivo: también compila en un PC */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <cmd_parser.h>
#include <sys/_intsup.h>

typedef struct
{
    const char *name;
    cmd_id_t    id;
} cmd_entry_t;

static const cmd_entry_t cmd_table[] =
{
    { "help",  CMD_HELP  },
    { "info",  CMD_INFO  },
    { "blink", CMD_BLINK },
    { "echo",  CMD_ECHO  },
};

#define CMD_TABLE_LEN (sizeof cmd_table / sizeof cmd_table[0])

static bool is_separator(char c);

/*  Divide una línea en palabras, en el mismo lugar: cada separador se vuelve '\0'.
    La primera palabra se busca en cmd_table; el resto pasar a ser los argumentos.
    Retorna false para una línea vacía. */
bool cmd_parse(char *line, cmd_t *cmd)
{
    const char *words[CMD_MAX_ARGS + 1U];
    uint8_t word_count = 0U;
    char *cursor = line;

    cmd->too_many_args = false;

    /*1. Dividir la línea en palabra: cada separador se vuelve '\0' */
    while (*cursor != '\0')
    {
        if (is_separator(*cursor))
        {
            *cursor = '\0';
            cursor++;
        } else if (word_count == (CMD_MAX_ARGS + 1U))
        {
            cmd->too_many_args = true;
            break;
        } else 
        {
            words[word_count] = cursor;
            word_count++;
            while ((*cursor != '\0') && !is_separator((*cursor)))
            {
                cursor++;
            }
        }
    }
    if (word_count == 0U) {
        return false;       //Línea vacía
    }

    /* 2. Mirar la primer palabra en la tabla de argumentos */
    cmd->name = words[0];
    cmd->id = CMD_UNKNOWN;
    for (size_t i = 0U; i < CMD_TABLE_LEN; i++) 
    {
        if (strcmp(words[0], cmd_table[i].name) == 0) {
            cmd->id = cmd_table[i].id;
            break;
        }
    }


    /* 3. Las palabras restantes son los argumentos */
    cmd->argc = (uint8_t)(word_count - 1U);
    for (uint8_t i = 0U; i < cmd->argc; i++) {
        cmd->argv[i] = words[i + 1U];
    }
    return true;
}

/* Verdadero para los caracteres que separan palabras: espacio, tabulador y fines de línea. */
static bool is_separator(char c)
{
    return (c == ' ') || (c == '\t') || (c == '\r') || (c == '\n');
}