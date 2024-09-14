#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <math.h>

#define MAX_LINE_LENGTH 0xFF
#define MAX_TOKEN_LENGTH 0xFF

typedef struct symbol_t{
    uint32_t address;
    char name[MAX_TOKEN_LENGTH];
}symbol_t;

static size_t SYMBOL_TABLE_SIZE = 0;
static symbol_t* SYMBOL_TABLE = NULL;

long load_symbols(const char* path){
    FILE *file;
    char line[MAX_LINE_LENGTH];
    char *token;

    // Open the file
    file = fopen(path, "r");
    if (file == NULL) {
        printf("Unable to open file \"%s\".\n", path);
        return -1;
    }

    size_t lines = 0;
    while(!feof(file))
    {
        char ch = fgetc(file);
        if(ch == '\n')
        {
            lines++;
        }
    }
    fseek(file, 0, SEEK_SET);

    SYMBOL_TABLE = (symbol_t*)malloc(lines * sizeof(symbol_t));
    memset(SYMBOL_TABLE, 0, lines * sizeof(symbol_t));

    size_t tk_ptr = 0;
    // Read each line from the file
    while (fgets(line, sizeof(line), file) != NULL) {
        // Split the line into tokens at every space character
        token = strtok(line, " \t");
        size_t ntokens = 0;
        int tk_ok = 0;
        while (token != NULL) {
            size_t tksize = strlen(token);
            if(token[tksize-1] == '\n'){
                token[tksize-1] = '\0';
                tksize -=1;
            }
            if(isdigit(token[0]) && ntokens==0){
                tk_ok = 1;
                sscanf(token, "%x", &SYMBOL_TABLE[tk_ptr].address);
            }
            if(!tk_ok){
                token = strtok(NULL, " \t");
                continue;
            }
            if(ntokens>0 && tk_ok){
                if(token[0] != '<' && token[tksize-3] != '>' && token[tksize-2] != ':'){
                    tk_ok = 0;
                    token = strtok(NULL, " \t");
                    continue;
                }
                memcpy(SYMBOL_TABLE[tk_ptr].name, token, tksize);
                tk_ptr++;
                tk_ok = 0;
            }
            ntokens++;
            // Get the next token
            token = strtok(NULL, " \t");
        }
    }

    SYMBOL_TABLE_SIZE = tk_ptr;
    printf("Loaded %ld Symbols from \"%s\"\n", SYMBOL_TABLE_SIZE, path);
    return SYMBOL_TABLE_SIZE;
}