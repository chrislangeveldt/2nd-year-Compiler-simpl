/**
 * @file    token.h
 * @brief   Data type definitions for the lexical analyser (scanner) of
 *          SIMPL-2021.
 * @author  W.H.K. Bester (whkbester@cs.sun.ac.za)
 * @date    2021-08-23
 */

#ifndef TOKEN_H
#define TOKEN_H

/** the maximum length of an identifier */
#define MAX_ID_LENGTH 32

/** the types of tokens that the scanner recognises */
typedef enum {

	TOK_EOF,       /* end-of-file    */
	TOK_ID,        /* identifier     */
	TOK_NUM,       /* number literal */
	TOK_STR,       /* string literal */

	/* keywords: Note that the boolean operators AND and OR, and the remainder
	 * operator MOD, although written out as string literals, are still
	 * treated as operators
	 */
	TOK_ARRAY,
	TOK_BEGIN,
	TOK_BOOLEAN,
	TOK_CHILL,
	TOK_DEFINE,
	TOK_DO,
	TOK_ELSE,
	TOK_ELSIF,
	TOK_END,
	TOK_EXIT,
	TOK_FALSE,
	TOK_IF,
	TOK_INTEGER,
	TOK_NOT,
	TOK_PROGRAM,
	TOK_READ,
	TOK_THEN,
	TOK_TRUE,
	TOK_WHILE,
	TOK_WRITE,

	/* relational operators: The order of the relational operators is
	 * significant -- it allows us to do range checking in the parser.
	 */
	TOK_EQ,
	TOK_GE,
	TOK_GT,
	TOK_LE,
	TOK_LT,
	TOK_NE,

	/* additive operators */
	TOK_MINUS,
	TOK_OR,
	TOK_PLUS,

	/* multiplicative operators */
	TOK_AND,
	TOK_DIV,
	TOK_MUL,
	TOK_MOD,

	/* other non-alphabetic operators */
	TOK_AMPERSAND,
	TOK_LBRACK,
	TOK_RBRACK,
	TOK_COMMA,
	TOK_GETS,
	TOK_LPAR,
	TOK_RPAR,
	TOK_SEMICOLON,
	TOK_TO

} TokenType;

/** the token data type */
typedef struct {
	TokenType  type;                     /**< type of the token            */
	int        value;                    /**< numeric value (for integers) */
	char       lexeme[MAX_ID_LENGTH+1];  /**< lexeme (for identifiers)     */
	char      *string;                   /**< string (for write)           */
} Token;

/**
 * Returns a string representation of the specified token type.
 *
 * @param[in]   type
 *     the token for which to get the string representation
 * @return      a (constant) string representation of the specified token type
 */
const char *get_token_string(TokenType type);

#endif /* TOKEN_H */
