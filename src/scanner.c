/**
 * @file    scanner.c
 * @brief   The scanner for SIMPL-2021.
 * @author  C.H. Langeveldt (23632135@sun.ac.za)
 * @author  W.H.K. Bester (whkbester@cs.sun.ac.za)
 * @date    2021-08-23
 */

#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "boolean.h"
#include "error.h"
#include "scanner.h"
#include "token.h"

/* --- type definitions and constants --------------------------------------- */

typedef struct {
	char       *word;                  /* the reserved word, i.e., the lexeme */
	TokenType  type;                   /* the associated token type           */
} ReservedWord;

/* -------------------------------------------------------------------------- */

static FILE *src_file;                 /* the source file pointer             */
static int   ch;                       /* the next source character           */
static int   column_number;            /* the current column number           */

static ReservedWord reserved[] = {     /* reserved words                      */
	{"and", TOK_AND},
	{"array", TOK_ARRAY},
	{"begin", TOK_BEGIN},
	{"boolean", TOK_BOOLEAN},
	{"chill", TOK_CHILL},
	{"define", TOK_DEFINE},
	{"do", TOK_DO},
	{"else", TOK_ELSE},
	{"elsif", TOK_ELSIF},
	{"end", TOK_END},
	{"exit", TOK_EXIT},
	{"false", TOK_FALSE},
	{"if", TOK_IF},
	{"integer", TOK_INTEGER},
	{"mod", TOK_MOD},
	{"not", TOK_NOT},
	{"or", TOK_OR},
	{"program", TOK_PROGRAM},
	{"read", TOK_READ},
	{"then", TOK_THEN},
	{"true", TOK_TRUE},
	{"while", TOK_WHILE},
	{"write", TOK_WRITE}
};

#define NUM_RESERVED_WORDS (sizeof(reserved) / sizeof(ReservedWord))
#define MAX_INITIAL_STRLEN (1024)

/* --- function prototypes -------------------------------------------------- */

static void next_char(void);
static void process_number(Token *token);
static void process_string(Token *token);
static void process_word(Token *token);
static void skip_comment(void);

/* --- scanner interface ---------------------------------------------------- */

void init_scanner(FILE *in_file)
{
	src_file = in_file;
	position.line = 1;
	position.col = column_number = 0;
	next_char();
}

void get_token(Token *token)
{
	int ascii;
	/* remove whitespace */
	while (isspace(ch)) {
		next_char();
	}
	/* remember token start */
	position.col = column_number;

	/* get next token */
	if (ch != EOF) {
		if (isalpha(ch) || ch == '_') {

			/* process a word */
			process_word(token);

		} else if (isdigit(ch)) {

			/* process a number */
			process_number(token);

		} else switch (ch) {

			/* process a string */
			case '"':
				position.col = column_number;
				next_char();
				process_string(token);
				break;

			/* process the other tokens, and trigger comment skipping. */
			case '=':
				token->type = TOK_EQ;
				next_char();
				break;

			case '>':
				next_char();
				if (ch == '=') {
					token->type = TOK_GE;
					next_char();
				} else {
					token->type = TOK_GT;
				}
				break;

			case '<':
				next_char();
				if (ch == '=') {
					token->type = TOK_LE;
					next_char();
				} else if (ch == '-') {
					token->type = TOK_GETS;
					next_char();
				} else {
					token->type = TOK_LT;
				}
				break;

			case '#':
				token->type = TOK_NE;
				next_char();
				break;
			
			case '-':
				next_char();
				if (ch == '>') {
					token->type = TOK_TO;
					next_char();
				} else {
					token->type = TOK_MINUS;
				}
				break;

			case '+':
				token->type = TOK_PLUS;
				next_char();
				break;

			case '/':
				token->type = TOK_DIV;
				next_char();
				break;

			case '*':
				token->type = TOK_MUL;
				next_char();
				break;

			case '&':
				token->type = TOK_AMPERSAND;
				next_char();
				break;

			case '[':
				token->type = TOK_LBRACK;
				next_char();
				break;

			case ']':
				token->type = TOK_RBRACK;
				next_char();
				break;

			case ',':
				token->type = TOK_COMMA;
				next_char();
				break;

			case '(':
				next_char();
				if (ch == '*') { /* trigger comment skipping */
					next_char();
					skip_comment();
					get_token(token);
				} else {
					token->type = TOK_LPAR;
				}
				break;

			case ')':
				token->type = TOK_RPAR;
				next_char();
				break;

			case ';':
				token->type = TOK_SEMICOLON;
				next_char();
				break;

			default:
				ascii = (int) ch;
				leprintf("illegal character '%c' (ASCII #%d)", ch, ascii);
		}

	} else {
		token->type = TOK_EOF;
	}
}

/* --- utility functions ---------------------------------------------------- */

void next_char(void)
{  
	static char last_read = '\0';

	ch = fgetc(src_file);
	if (ch == EOF) {
		return;
	}
	if (last_read == '\n') {
		position.line++;
		column_number = 0;
	}

	column_number++;
	last_read = ch;
}

void process_number(Token *token)
{
	int digit, num;
	position.col = column_number;
	digit = 0;
	for (num = 0; isdigit(ch); next_char()) {
		digit = ch - '0';
		if (num > (INT_MAX - digit) / 10) {
			leprintf("number too large");
		} else {
			num = 10 * num + digit;
		}
	}

	token->value = num;
	token->type = TOK_NUM;
}

void process_string(Token *token)
{
	size_t i, nstring = MAX_INITIAL_STRLEN;
	int ascii;
	SourcePos start_pos;

	char *str = malloc(sizeof(char) * nstring);
	start_pos.col = column_number - 1;
	start_pos.line = position.line;
	 
	for (i = 0; ; i++) {
		if (i == nstring) { /* double size */
			nstring *= 2;
			str = realloc(str, nstring);
		}
		if (ch == EOF) { /* end of file reached - string not closed */
			position = start_pos;
			leprintf("string not closed");
		} else if (ch == '"') { /* end of string */
			next_char();
			break; 
		} else if (ch == '\\') { /* check legibility of escape sequence */
			next_char();
			if (ch == 'n' || ch == 't' || ch == '"' || ch == '\\') {
				str[i] = '\\';
				str[++i] = ch;
				next_char();
			} else {
				position.col = column_number - 1;
				leprintf("illegal escape code '\\%c' in string", ch);
			}
		} else if (isascii(ch) && isprint(ch)) { 
			str[i] = ch;
			next_char();
		} else {
			ascii = (int) ch;
			position.col = column_number;
			leprintf("non-printable character (ASCII #%d) in string", ascii);
			break;
		}
	}

	token->type = TOK_STR;
	token->string = malloc(sizeof(char) * nstring);
	token->string = str;
}

void process_word(Token *token)
{
	char lexeme[MAX_ID_LENGTH+1];
	int i, cmp, low, mid, high;

	position.col = column_number;
	i = 0;

	/* check that the id length is less than the maximum */
	while (isdigit(ch) || isalpha(ch) || ch == '_') { 
		if (i == MAX_ID_LENGTH) {
			leprintf("identifier too long");
		}
		lexeme[i] = ch;
		next_char();
		i++;
	}

	lexeme[i] = '\0';

	/* do a binary search through the array of reserved words */
	low = 0;
	high = NUM_RESERVED_WORDS;
	mid = (low + high) / 2;
	while (low <= high) {
		cmp = strcmp(lexeme, reserved[mid].word);
		if (cmp > 0) {
			low = mid + 1;
		} else if (cmp < 0) {
			high = mid - 1;
		} else {
			break;
		}
		mid = (low + high) / 2;
	}

	/* if id was not recognised as a reserved word, it is an identifier */
	if (low > high) {
		token->type = TOK_ID;
		strcpy(token->lexeme, lexeme);
	} else {
		token->type = reserved[mid].type;
	}
}

void skip_comment(void)
{
	SourcePos start_pos;
	start_pos.line = position.line;
	start_pos.col = column_number - 2;

	while (ch != EOF) {
		if (ch == '*') {
			next_char();
			if (ch == ')') {
				next_char();
				return;
			} 
			continue;
		} else if (ch == '(') {
			next_char();
			if (ch == '*') {
				next_char();
				skip_comment();
				continue;
			}
		}
		next_char();
	}

	/* force the line number of error reporting */
	position = start_pos;
	leprintf("comment not closed");
}
