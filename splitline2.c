/* splitline.c - commmand reading and parsing functions for smsh
 *    
 *    char *next_cmd(char *prompt, FILE *fp) - get next command
 *    char **splitline(char *str);           - parse a string

 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	"smsh.h"


/**
 **	splitline ( parse a line into an array of strings )
 **/
#define	is_delim(x) ((x)==' '||(x)=='\t')

char ** splitline2(char *line, char * delimiter)
/*
 * purpose: split a line into array of tokens separated by a delimiter
 * returns: a NULL-terminated array of pointers to copies of the tokens
 *          or NULL if line if no tokens on the line
 *  action: traverse the line, locate strings, make copies
 */
{
    char *newstr();
    char **args;
    int argnum = 0;         /* slots used        */
    char *cp = line;        /* pos in string     */
    char *start;
    int len;

    if (line == NULL)       /* handle special case    */
        return NULL;

    args = emalloc(BUFSIZ); /* initialize array       */

    while (*cp != '\0') {
        while (*cp == *delimiter)   /* skip leading delimiters  */
            cp++;
        if (*cp == '\0')            /* quit at end-o-string    */
            break;

        /* mark start, then find end of word */
        start = cp;
        len = 1;
        while (*++cp != '\0' && *cp != *delimiter)
            len++;
        args[argnum++] = newstr(start, len);
    }
    args[argnum] = NULL;
    return args;
}