/**
 * @file    simplc.c
 *
 * A recursive-descent compiler for the SIMPL-2021 language.
 *
 * All scanning errors are handled in the scanner.  Parser errors MUST be
 * handled by the <code>abort_c</code> function.  System and environment errors,
 * for example, running out of memory, MUST be handled in the unit in which they
 * occur.  Transient errors, for example, non-existent files, MUST be reported
 * where they occur.  There are no warnings, which is to say, all errors are
 * fatal and MUST cause compilation to terminate with an abnormal error code.
 *
 * @author  W.H.K. Bester (whkbester@cs.sun.ac.za)
 * @author  C.H. Langeveldt (23632135@sun.ac.za)
 * @date    2021-08-23
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen.h"
#include "errmsg.h"
#include "error.h"
#include "hashtable.h"
#include "scanner.h"
#include "symboltable.h"
#include "token.h"
#include "valtypes.h"

/* --- debugging ------------------------------------------------------------ */

#ifdef DEBUG_PARSER
	void debug_start(const char *fmt, ...);
	void debug_end(const char *fmt, ...);
	void debug_info(const char *fmt, ...);
	#define DBG_start(...) debug_start(__VA_ARGS__)
	#define DBG_end(...) debug_end(__VA_ARGS__)
	#define DBG_info(...) debug_info(__VA_ARGS__)
#else
	#define DBG_start(...)
	#define DBG_end(...)
	#define DBG_info(...)
#endif /* DEBUG_PARSER */

/* --- type definitions ----------------------------------------------------- */

typedef struct variable_s Variable;
struct variable_s {
	char      *id;     /**< variable identifier                       */
	ValType    type;   /**< variable type                             */
	SourcePos  pos;    /**< variable position in the source           */
	Variable  *next;   /**< pointer to the next variable in the list  */
};

/* --- global variables ----------------------------------------------------- */

Token    token;        /**< the lookahead token.type                  */
FILE    *src_file;     /**< the source code file                      */
ValType  return_type;  /**< the return type of the current subroutine */

/* --- helper macros -------------------------------------------------------- */

#define STARTS_FACTOR(toktype) \
	(toktype == TOK_ID || toktype == TOK_NUM || \
     toktype == TOK_LPAR || toktype == TOK_NOT || \
     toktype == TOK_TRUE || toktype == TOK_FALSE)

#define STARTS_EXPR(toktype) \
	(toktype == TOK_ID || toktype == TOK_NUM || \
     toktype == TOK_LPAR || toktype == TOK_NOT || \
     toktype == TOK_TRUE || toktype == TOK_FALSE || \
     toktype == TOK_MINUS)

#define IS_ADDOP(toktype) \
	(toktype >= TOK_MINUS && toktype <= TOK_PLUS)

#define IS_MULOP(toktype) \
	(toktype >= TOK_AND && toktype <= TOK_MOD) 

#define IS_RELOP(toktype) \
	(toktype >= TOK_EQ && toktype <= TOK_NE)

#define IS_TYPE_TOKEN(toktype) \
	(toktype == TOK_BOOLEAN || toktype == TOK_INTEGER)

#define IS_STATEMENT(toktype) \
	(toktype == TOK_EXIT || toktype == TOK_IF || \
     toktype == TOK_ID || toktype == TOK_READ || \
     toktype == TOK_WHILE || toktype == TOK_WRITE)

/* --- function prototypes: parsing ----------------------------------------- */

void parse_program(void);
void parse_funcdef(void);
void parse_body(void);
void parse_statements(void);
void parse_type(ValType *type);
void parse_vardef(void);
void parse_statement(void);
void parse_exit(void);
void parse_if(void);
void parse_name(void);
void parse_read(void);
void parse_while(void);
void parse_write(void);
void parse_arglist(char *id, SourcePos idpos);
void parse_index(char *id);
void parse_expr(ValType *type);
void parse_simple(ValType *type);
void parse_term(ValType *type);
void parse_factor(ValType *type);
void parse_idf(ValType *type, char *id);
void parse_param(unsigned int *k, char *id);

/* --- function prototypes: helpers ----------------------------------------- */

void check_types(ValType found, ValType expected, SourcePos *pos, ...);
void expect(TokenType type);
void expect_id(char **id);
IDprop *make_idprop(ValType type, unsigned int offset, unsigned int nparams,
       ValType *params);
Variable *make_var(char *id, ValType type, SourcePos pos);

/* --- function prototypes: error reporting --------------------------------- */

void abort_c(Error err, ...);
void abort_cp(SourcePos *posp, Error err, ...);

/* --- main routine --------------------------------------------------------- */

int main(int argc, char *argv[])
{
	char *jasmin_path;
	/* TODO: Uncomment the previous definition for code generation. */

	/* set up global variables */
	setprogname(argv[0]);

	/* check command-line arguments and environment */
	if (argc != 2) {
		eprintf("usage: %s <filename>", getprogname());
	}

	if ((jasmin_path = getenv("JASMIN_JAR")) == NULL) {
		eprintf("JASMIN_JAR environment variable not set");
	}


	/* open the source file, and report an error if it could not be opened. */
	if ((src_file = fopen(argv[1], "r")) == NULL) {
		eprintf("file '%s' could not be opened:", argv[1]);
	}
	setsrcname(argv[1]);

	/* initialise all compiler units */
	init_scanner(src_file);
	init_symbol_table();
	init_code_generation();

	/* compile */
	get_token(&token);
	parse_program();

	/* produce the object code, and assemble */
	make_code_file();
	assemble(jasmin_path);

	/* release allocated resources */
	fclose(src_file);
	freeprogname();
	freesrcname();
	release_symbol_table();
	release_code_generation();

#ifdef DEBUG_PARSER
	printf("SUCCESS!\n");
#endif

	return EXIT_SUCCESS;
}

/* --- parser routines ------------------------------------------------------ */

/* <program> = "program" <id> { <funcdef> } <body> .
 */
void parse_program(void)
{
	char *class_name;

	DBG_start("<program>");

	/* For code generation, set the class name inside this function, and
	 * also handle initialising and closing the "main" function.  But from the
	 * perspective of simple parsing, this function is complete.
	 */

	expect(TOK_PROGRAM);
	expect_id(&class_name);
	/* Set the class name here during code generation. */
	set_class_name(class_name);

	while (token.type == TOK_DEFINE) {
		parse_funcdef();
	}
	init_subroutine_codegen("main", NULL);
	parse_body();
	gen_1(JVM_RETURN);
	close_subroutine_codegen(get_variables_width());

	/* Memory leak strategy? -chris */
	free(class_name); 

	DBG_end("</program>");
}

/* <funcdef> = "define" <id> "(" [<type> <id> { "," <type> <id> }] ")" 
               ["->" <type>] <body> .
 */
void parse_funcdef(void)
{
	DBG_start("<funcdef>");

	char *funcid, *id;
	SourcePos funcpos, pos;
	ValType t1, *params;
	Variable *head, *temp, *newvar;
	unsigned int count, i;
	IDprop *prop;

	funcpos = position;
	count = 0;
	head = NULL;
	t1 = 0;
	id = NULL;
	return_type = TYPE_NONE;
	
	expect(TOK_DEFINE);
	expect_id(&funcid);
	expect(TOK_LPAR);
	if (IS_TYPE_TOKEN(token.type)) {
		parse_type(&t1);
		pos = position;
		expect_id(&id);
		head = make_var(id, t1, pos);
		head->next = NULL;
		count = 1;
		temp = head;
		while (token.type == TOK_COMMA) {
			get_token(&token);
			t1 = 0;
			parse_type(&t1);
			pos = position;
			expect_id(&id);
			newvar = make_var(id, t1, pos); 
			newvar->next = NULL;
			temp->next = newvar;
			temp = temp->next;
			count++;
		}
	}
	expect(TOK_RPAR);
	params = emalloc(count * sizeof(Variable)); 
	temp = head;
	for (i = 0; i < count; i++) {
		params[i] = temp->type;
		temp = temp->next;
	}
	t1 = TYPE_CALLABLE;
	if (token.type == TOK_TO) {
		get_token(&token);
		parse_type(&t1);
	}
	return_type = t1;
	prop = make_idprop(t1, get_variables_width(), count, params);
	if (open_subroutine(funcid, prop)) {
		while (head != NULL) {
			temp = head;
			prop = NULL;
			if (find_name(temp->id, &prop)) {
				position = temp->pos;
				abort_c(ERR_MULTIPLE_DEFINITION, temp->id);
			}
			prop = NULL;
			prop = make_idprop(temp->type, get_variables_width(), 0, NULL);
			if (!insert_name(temp->id, prop)) {
				position = temp->pos;
				abort_c(ERR_MULTIPLE_DEFINITION, temp->id);
			}
			head = head->next;
			free(temp);
			temp = NULL;
		}
		init_subroutine_codegen(funcid, prop);
		parse_body();
		close_subroutine_codegen(get_variables_width());
		close_subroutine();
		return_type = TYPE_NONE;
	} else {
		position = funcpos;
		abort_c(ERR_MULTIPLE_DEFINITION, funcid);
	}

	DBG_end("</funcdef>");
}

/* <body> = "begin" { <vardef> } <statements> "end" .
 */
void parse_body(void)
{
	DBG_start("<body>");

	expect(TOK_BEGIN);
	while (IS_TYPE_TOKEN(token.type)) {
		parse_vardef();
	}
	parse_statements();
	expect(TOK_END);

	DBG_end("</body>");
}

/* <statements> = "chill" | <statement> { ";" <statement> } .
 */
void parse_statements(void)
{
	DBG_start("<statements>");

	if (token.type == TOK_CHILL) {
		get_token(&token);
	} else if (IS_STATEMENT(token.type)) {
		parse_statement();
		while (token.type == TOK_SEMICOLON) {
			get_token(&token);
			parse_statement();
		}
	} else {
		abort_c(ERR_STATEMENT_EXPECTED, token.type); 
	}

	DBG_end("</statements>");
}

/* <type> = ("boolean" | "integer") ["array"] .
 */
void parse_type(ValType *t0)
{
	DBG_start("<type>");

	if (token.type == TOK_BOOLEAN) {
		*t0 |= TYPE_BOOLEAN;
	} else if (token.type == TOK_INTEGER) {
		*t0 |= TYPE_INTEGER;
	} else {
		abort_c(ERR_TYPE_EXPECTED, token.type);
	}
	get_token(&token);
	if (token.type == TOK_ARRAY) {
		get_token(&token);
		*t0 |= TYPE_ARRAY;
	}

	DBG_end("</type>");
}

/* <vardef> = <type> <id> { "," <id> } ";" .
 */
void parse_vardef(void)
{
	char *vname;
	ValType t1;
	IDprop *prop;
	SourcePos pos;

	DBG_start("<vardef>");
	t1 = 0;
	parse_type(&t1);
	pos = position;
	expect_id(&vname);
	if (find_name(vname, &prop)) {
		position = pos;
		abort_c(ERR_MULTIPLE_DEFINITION, vname);
	}
	prop = make_idprop(t1, get_variables_width(), 0, NULL);
	if (!insert_name(vname, prop)) {
		position = pos;
		abort_c(ERR_MULTIPLE_DEFINITION, vname);
	}
	while (token.type == TOK_COMMA) {
		get_token(&token);
		pos = position;
		expect_id(&vname);
		if (find_name(vname, &prop)) {
			position = pos;
			abort_c(ERR_MULTIPLE_DEFINITION, vname);
		}
		prop = make_idprop(t1, get_variables_width(), 0, NULL);
		if (!insert_name(vname, prop)) {
			position = pos;
			abort_c(ERR_MULTIPLE_DEFINITION, vname);
		}
	}
	expect(TOK_SEMICOLON);

	DBG_end("</vardef>");
}

/* <statement> = <exit> | <if> | <name> | <read> | <while> | <write> .
 */
void parse_statement(void)
{
	DBG_start("<statement>");

	switch (token.type) {
		case TOK_EXIT:  parse_exit();   break;
		case TOK_IF:    parse_if();     break;
		case TOK_ID:    parse_name();   break;
		case TOK_READ:  parse_read();   break;
		case TOK_WHILE: parse_while();  break;
		case TOK_WRITE: parse_write();  break;
		default:
			abort_c(ERR_STATEMENT_EXPECTED, token.type);
			break;
	}

	DBG_end("</statement>");
}

/* <exit> = "exit" [<expr>] .
 */
void parse_exit(void)
{
	ValType t1, t2;
	SourcePos pos;

	DBG_start("<exit>");
	t1 = 0;
	pos = position;
	expect(TOK_EXIT);
	if (STARTS_EXPR(token.type)) {
		if (IS_PROCEDURE(return_type)) {
			abort_c(ERR_EXIT_EXPRESSION_NOT_ALLOWED_FOR_PROCEDURE);
		} else if (IS_FUNCTION(return_type)) {
			pos = position;
			parse_expr(&t1);
			if (IS_ARRAY_TYPE(return_type)) {
				gen_1(JVM_ARETURN);
			} else {
				gen_1(JVM_IRETURN);
			}
			t2 = return_type;
			SET_RETURN_TYPE(t2);
			check_types(t1, t2, &pos, "for 'exit' statement");
		}
	} else if (IS_FUNCTION(return_type)) {
		position = pos;
		abort_c(ERR_MISSING_EXIT_EXPRESSION_FOR_FUNCTION);
	} else {
		gen_1(JVM_RETURN);
	}

	DBG_end("</exit>");
}

/* <if> = "if" <expr> "then" <statements> {"elsif" <expr> "then" <statements>} 
          ["else" <statements>] "end" . 
 */
void parse_if(void)
{
	ValType t1;
	SourcePos pos;
	int l1, l2, l3;

	DBG_start("<if>");
	
	l1 = get_label();
	l2 = get_label();
	l3 = get_label();
	
	expect(TOK_IF);
	pos = position;
	parse_expr(&t1);
	gen_2_label(JVM_IFEQ, l1); //false, go to l1
	check_types(t1, TYPE_BOOLEAN, &pos, "for 'if' guard");
	expect(TOK_THEN);
	parse_statements();
	gen_2_label(JVM_GOTO, l3);

	gen_label(l1);
	while (token.type == TOK_ELSIF) {
		gen_label(l2);
		get_token(&token);
		pos = position;
		parse_expr(&t1);
		gen_2_label(JVM_IFEQ, l2);
		check_types(t1, TYPE_BOOLEAN, &pos, "for 'elsif' guard");
		expect(TOK_THEN);
		parse_statements();
		gen_2_label(JVM_GOTO, l3);
	}
	if (token.type == TOK_ELSE) {
		get_token(&token);
		parse_statements();
	}
	gen_label(l3);
	expect(TOK_END);

	DBG_end("</if>");
}

/* <name> = <id> (<arglist> | [<index>] "<-" (<expr> | "array" <simple>)) .
 */
void parse_name(void)
{
	char *id;
	ValType t1, proptype;
	IDprop *prop;
	Boolean is_array, is_indexed;
	SourcePos idpos, pos;
	
	DBG_start("<name>");

	is_indexed = FALSE;
	idpos = position;
	expect_id(&id);
	if (!find_name(id, &prop)) {
		position = idpos;
		abort_c(ERR_UNKNOWN_IDENTIFIER, id);
	}
	proptype = prop->type;
	if (token.type == TOK_LPAR) {
		if (!IS_PROCEDURE(prop->type)) {
			position = idpos;
			abort_c(ERR_NOT_A_PROCEDURE, id);
		}
		parse_arglist(id, idpos);
		gen_call(id, prop);
	} else if (token.type == TOK_LBRACK || token.type == TOK_GETS) {
		if (IS_CALLABLE_TYPE(prop->type)) {
			position = idpos;
			abort_c(ERR_NOT_A_VARIABLE, id);
		}
		if (token.type == TOK_LBRACK) {
			if (!IS_ARRAY(prop->type)){
				position = idpos;
				abort_c(ERR_NOT_AN_ARRAY, id);
			}
			proptype ^= TYPE_ARRAY;
			is_array = FALSE;
			is_indexed = TRUE;
			parse_index(id);
		} else if (IS_ARRAY(prop->type)) {
			is_array = TRUE;
		} else {
			is_array = FALSE;
		}
		expect(TOK_GETS);
		pos = position;
		if (STARTS_EXPR(token.type)) {
			parse_expr(&t1);
			if (!IS_VARIABLE(proptype)) {
				position = idpos;
				abort_c(ERR_NOT_A_VARIABLE, id);
			}
			if (is_array) {
				check_types(t1, proptype, &pos, 
				"for assignment to '%s'", id);
				gen_2(JVM_ASTORE, prop->offset);
			} else {
				if (IS_ARRAY(t1)) {
					if (is_indexed) {
						check_types(t1, proptype, &pos, 
						"for allocation to indexed array '%s'", id);
					} else {
						position = idpos;
						abort_c(ERR_NOT_AN_ARRAY, id);
					}
				} else {
					check_types(t1, proptype, &pos,
					"for assignment to '%s'", id);
				}
			}
			if (!is_indexed && !is_array) {
				gen_2(JVM_ISTORE, prop->offset);
			}
			if (is_indexed) {
				gen_1(JVM_IASTORE);
			}
		} else if (token.type == TOK_ARRAY) {
			if (is_indexed) {
				check_types(prop->type, proptype, &position, 
				"for allocation to indexed array '%s'", id); 
			}
			if (!IS_ARRAY(prop->type)) {
				position = idpos;
				abort_c(ERR_NOT_AN_ARRAY, id);
			}
			get_token(&token);
			pos = position;
			parse_simple(&t1);
			check_types(t1, TYPE_INTEGER, &pos, "for array size of '%s'", id);
			gen_newarray(T_INT);
			gen_2(JVM_ASTORE, prop->offset);
		} else {
			abort_c(ERR_ARRAY_ALLOCATION_OR_EXPRESSION_EXPECTED, token.type);
		}
	} else {
		abort_c(ERR_ARGUMENT_LIST_OR_VARIABLE_ASSIGNMENT_EXPECTED, token.type);
	}
			
	DBG_end("</name>");
}

/* <read> = "read" <id> [<index>] .
 */
void parse_read(void)
{
	char *vname; 
	IDprop *prop;
	SourcePos pos;

	DBG_start("<read>");

	expect(TOK_READ);
	pos = position;
	expect_id(&vname);
	if (!find_name(vname, &prop)) {
		position = pos;
		abort_c(ERR_UNKNOWN_IDENTIFIER, vname);
	}
	if (token.type == TOK_LBRACK) {
		if (!IS_ARRAY(prop->type)) {
			position = pos;
			abort_c(ERR_NOT_AN_ARRAY, vname);
		}
		parse_index(vname);
	} else if (IS_ARRAY(prop->type)) {
		position = pos;
		abort_c(ERR_SCALAR_VARIABLE_EXPECTED, vname);
	}
	if (IS_INTEGER_TYPE(prop->type)) {
		gen_read(TYPE_INTEGER);
	} else {
		gen_read(TYPE_BOOLEAN);
	}

	if (IS_ARRAY_TYPE(prop->type)) {
		gen_1(JVM_IASTORE);
	} else {
		gen_2(JVM_ISTORE, prop->offset);
	}	

	DBG_end("</read>");
}

/* <while> = "while" <expr> "do" <statements> "end" .
 */
void parse_while(void)
{
	ValType t1;
	SourcePos pos;
	int l1, l2;

	DBG_start("<while>");

	l1 = get_label();
	l2 = get_label();

	expect(TOK_WHILE);
	pos = position;
	gen_label(l1);
	parse_expr(&t1);
	gen_2_label(JVM_IFEQ, l2);
	check_types(t1, TYPE_BOOLEAN, &pos, "for 'while' guard");
	expect(TOK_DO);
	parse_statements();
	expect(TOK_END);
	gen_2_label(JVM_GOTO, l1);

	gen_label(l2);

	DBG_end("</while>");
}

/* <write> = "write" (<string> | <expr>) {"&" (<string> | <expr>)} .
 */
void parse_write(void)
{
	ValType t1;
	SourcePos pos;

	DBG_start("<write>");
	
	pos = position;
	expect(TOK_WRITE);
	if (token.type == TOK_STR) {
		gen_print_string(token.string);
		get_token(&token);
	} else if (STARTS_EXPR(token.type)) {
		parse_expr(&t1);
		gen_print(t1);
		if (IS_ARRAY(t1)) {
			position = pos;
			abort_c(ERR_ILLEGAL_ARRAY_OPERATION, "write");
		}
	} else {
		abort_c(ERR_EXPRESSION_OR_STRING_EXPECTED, token.type);
	}
	while (token.type == TOK_AMPERSAND) {
		pos = position;
		get_token(&token);
		if (token.type == TOK_STR) {
			gen_print_string(token.string);
			get_token(&token);
		} else if (STARTS_EXPR(token.type)) {
			parse_expr(&t1);
			gen_print(t1);
			if (IS_ARRAY(t1)) {
				position = pos;
				abort_c(ERR_ILLEGAL_ARRAY_OPERATION, "&");
			}
		} else {
			abort_c(ERR_EXPRESSION_OR_STRING_EXPECTED, token.type);
		}
	}

	DBG_end("</write>");
}

/* <arglist> = "(" [<expr> {"," <expr>}] ")" .
 */
void parse_arglist(char *id, SourcePos idpos)
{
	ValType t1;
	unsigned int i;
	IDprop *prop;
	char *routine;
	SourcePos pos;

	DBG_start("<arglist>");

	if (!find_name(id, &prop)) {
		abort_c(ERR_UNKNOWN_IDENTIFIER, id);
	}
	if (IS_FUNCTION(prop->type)) {
		routine = "function";
	} else {
		routine = "procedure";
	}
	i = 0;
	
	expect(TOK_LPAR);
	if (STARTS_EXPR(token.type)) {
		if (prop->nparams == 0) {
			position = idpos;
			abort_c(ERR_TAKES_NO_ARGUMENTS, id, routine);
		}
		pos = position;
		parse_expr(&t1);
		check_types(t1, prop->params[i], &pos, 
		"for parameter %d of call to '%s'", i + 1, id);
		i++;
		while (token.type == TOK_COMMA) {
			if (i >= prop->nparams) {
				abort_c(ERR_TOO_MANY_ARGUMENTS, id);
			}
			get_token(&token);
			pos = position;
			parse_expr(&t1);
			check_types(t1, prop->params[i], &pos, 
			"for parameter %d of call to '%s'", i + 1, id);
			i++;
		}
		if (i < prop->nparams) {
			abort_c(ERR_TOO_FEW_ARGUMENTS, id);
		}
	}
	expect(TOK_RPAR);

	DBG_end("</arglist>");
}

/* <index> = "[" <simple> "]" .
 */
void parse_index(char *id)
{
	ValType t1;
	SourcePos pos;
	IDprop *prop;

	DBG_start("<index>");
	find_name(id, &prop);
	expect(TOK_LBRACK);
	pos = position;
	if (token.type == TOK_NUM) {
		gen_2(JVM_ALOAD, prop->offset);
	}
	parse_simple(&t1);
	check_types(t1, TYPE_INTEGER, &pos, "for array index of '%s'", id); 
	expect(TOK_RBRACK);
	
	DBG_end("</index>");
}

/* <expr> = <simple> [<relop> <simple>] .
 */
void parse_expr(ValType *t0)
{
	ValType t1, t2;
	TokenType op;
	SourcePos pos;

	DBG_start("<expr>");

	parse_simple(&t1);
	if (IS_RELOP(token.type)) {
		op = token.type;
		if (IS_ARRAY(t1)) {
			abort_c(ERR_ILLEGAL_ARRAY_OPERATION,
			get_token_string(op));
		}
		pos = position;
		get_token(&token);
		parse_simple(&t2);
		if (IS_ARRAY(t2)) {
			position = pos;
			abort_c(ERR_ILLEGAL_ARRAY_OPERATION,
			get_token_string(op));
		}	
		if (op == TOK_EQ || op == TOK_NE) {
			check_types(t2, t1, &pos, "for operator %s", get_token_string(op));
			if (op == TOK_EQ) {
				gen_cmp(JVM_IF_ICMPEQ);
			} else if (op == TOK_NE) {
				gen_cmp(JVM_IF_ICMPNE);
			}
		} else {
			check_types(t1, TYPE_INTEGER, &pos, "for operator %s",
			get_token_string(op));
			check_types(t2, TYPE_INTEGER, &pos, "for operator %s",
			get_token_string(op));
			if (op == TOK_GE) {
				gen_cmp(JVM_IF_ICMPGE); 
			} else if (op == TOK_GT) {
				gen_cmp(JVM_IF_ICMPGT);
			} else if (op == TOK_LE) {
				gen_cmp(JVM_IF_ICMPLE);
			} else if (op == TOK_LT) {
				gen_cmp(JVM_IF_ICMPLT);
			}
		}
		*t0 = TYPE_BOOLEAN;
	} else {
		*t0 = t1;
	}

	DBG_end("</expr>");
}

/* <simple> = ["-"] <term> {<addop> <term>} .
 */
void parse_simple(ValType *t0)
{
	ValType t1;
	SourcePos pos, posterm;
	TokenType op;

	DBG_start("<simple>");

	t1 = 0;
	if (token.type == TOK_MINUS) {
		pos = position;
		get_token(&token);
		posterm = position;
		parse_term(t0);
		gen_1(JVM_INEG);
		if (IS_ARRAY(*t0)) {
			position = pos;
			abort_c(ERR_ILLEGAL_ARRAY_OPERATION, "unary minus");
		}
		check_types(*t0, TYPE_INTEGER, &posterm, "for unary minus");
	} else {
		parse_term(t0);
		if (IS_ADDOP(token.type) && IS_ARRAY(*t0)) {
			abort_c(ERR_ILLEGAL_ARRAY_OPERATION, get_token_string(token.type));
		}
		while (IS_ADDOP(token.type)) {
			op = token.type;
			pos = position;
			get_token(&token);
			parse_term(&t1);
			if (IS_ARRAY(t1)) {
				position = pos;
				abort_c(ERR_ILLEGAL_ARRAY_OPERATION, get_token_string(op));
			}
			if (op == TOK_OR) {
				check_types(*t0, TYPE_BOOLEAN, &pos, "for operator %s",
				get_token_string(op));
				check_types(t1, TYPE_BOOLEAN, &pos, "for operator %s",
				get_token_string(op));
				gen_1(JVM_IOR);
			} else {
				check_types(*t0, TYPE_INTEGER, &pos, "for operator %s",
				get_token_string(op));
				check_types(t1, TYPE_INTEGER, &pos, "for operator %s",
				get_token_string(op));
				if (op == TOK_PLUS) {
					gen_1(JVM_IADD);
				} else if (op == TOK_MINUS) {
					gen_1(JVM_ISUB);
				}
			}
		}
	}

	DBG_end("</simple>");
}

/* <term> = <factor> {<mulop> <factor>} .
 */
void parse_term(ValType *t0)
{
	ValType t1;
	SourcePos pos;
	TokenType op;

	DBG_start("<term>");

	parse_factor(t0);
	if (IS_MULOP(token.type) && IS_ARRAY(*t0)) {
		abort_c(ERR_ILLEGAL_ARRAY_OPERATION, get_token_string(token.type));
	}
	while (IS_MULOP(token.type)) {
		op = token.type;
		pos = position;
		get_token(&token);
		parse_factor(&t1);
		if (IS_ARRAY(t1)) {
			position = pos;
			abort_c(ERR_ILLEGAL_ARRAY_OPERATION, get_token_string(op));
		}
		if (op == TOK_AND) {
			check_types(*t0, TYPE_BOOLEAN, &pos, "for operator %s", 
			get_token_string(op));
			check_types(t1, TYPE_BOOLEAN, &pos, "for operator %s",
			get_token_string(op));
			gen_1(JVM_IAND);
		} else {
			check_types(*t0, TYPE_INTEGER, &pos, "for operator %s",
			get_token_string(op));
			check_types(t1, TYPE_INTEGER, &pos, "for operator %s",
			get_token_string(op));
			if (op == TOK_MUL) {
				gen_1(JVM_IMUL);
			} else if (op == TOK_DIV) {
				gen_1(JVM_IDIV);
			} else if (op == TOK_MOD) { 
				gen_1(JVM_IREM);
			}
		}
	}
	
	DBG_end("</term>");
}

/* <factor> = <id> [<index> | <arglist>] | <num> | "not" <factor> | "true" |
              "false" | "(" <expr> ")" .
 */
void parse_factor(ValType *t0)
{
	char *vname;
	IDprop *prop;
	SourcePos pos;

	DBG_start("<factor>");
	vname = NULL;
	switch (token.type) {
		case TOK_ID:
			pos = position;
			expect_id(&vname);
			if (!find_name(vname, &prop)) { 
				position = pos;
				abort_c(ERR_UNKNOWN_IDENTIFIER, vname);
			} else if (token.type == TOK_LBRACK) {
				if (!IS_ARRAY(prop->type)) {
					position = pos;
					abort_c(ERR_NOT_AN_ARRAY, vname);
				}
				*t0 = prop->type & 6;
				parse_index(vname);
				gen_1(JVM_IALOAD);
			} else if (token.type == TOK_LPAR) {
				if (!IS_FUNCTION(prop->type)) {
					position = pos;
					abort_c(ERR_NOT_A_FUNCTION, vname);
				}
				*t0 = (prop->type) ^ TYPE_CALLABLE;
				parse_arglist(vname, pos);
				gen_call(vname, prop);
			} else if (IS_FUNCTION(prop->type)) {
				position = pos;
				abort_c(ERR_MISSING_FUNCTION_ARGUMENT_LIST, vname);
				//abort_c(ERR_NOT_A_VARIABLE, vname);
			} else {
				*t0 = prop->type;
				if (IS_ARRAY_TYPE(*t0)) {
					gen_2(JVM_ALOAD, prop->offset);
				} else {
					gen_2(JVM_ILOAD, prop->offset);
				}
			}
			break;

		case TOK_NUM:
			gen_2(JVM_LDC, token.value);
			*t0 = TYPE_INTEGER;
			get_token(&token);
			break;

		case TOK_NOT:
			get_token(&token);
			pos = position;
			parse_factor(t0);
			check_types(*t0, TYPE_BOOLEAN, &pos, "for 'not'");
			gen_2(JVM_LDC, 1);
			gen_1(JVM_IXOR);
			break;

		case TOK_TRUE:
			gen_2(JVM_LDC, 1);
			*t0 = TYPE_BOOLEAN;
			get_token(&token);
			break;

		case TOK_FALSE:
			gen_2(JVM_LDC, 0);
			*t0 = TYPE_BOOLEAN;
			get_token(&token);
			break;

		case TOK_LPAR:
			get_token(&token);
			parse_expr(t0);
			expect(TOK_RPAR);
			break;

		default:
			abort_c(ERR_FACTOR_EXPECTED, token.type);
			break;
	}

	DBG_end("</factor>");
}

/* --- helper routines ------------------------------------------------------ */

#define MAX_MESSAGE_LENGTH 256

void check_types(ValType found, ValType expected, SourcePos *pos, ...)
{
	char buf[MAX_MESSAGE_LENGTH], *s;
	va_list ap;

	if (found != expected) {
		buf[0] = '\0';
		va_start(ap, pos);
		s = va_arg(ap, char *);
		vsnprintf(buf, MAX_MESSAGE_LENGTH, s, ap);
		va_end(ap);
		if (pos != NULL) {
			position = *pos;
		}
		leprintf("incompatible types (expected %s, found %s) %s",
			get_valtype_string(expected), get_valtype_string(found), buf);
	}
}

void expect(TokenType type)
{
	if (token.type == type) {
		get_token(&token);
	} else {
		abort_c(ERR_EXPECT, type);
	}
}

void expect_id(char **id)
{
	if (token.type == TOK_ID) {
		*id = strdup(token.lexeme);
		get_token(&token);
	} else {
		abort_c(ERR_EXPECT, TOK_ID);
	}
}

IDprop *make_idprop(ValType type, unsigned int offset, unsigned int nparams,
       ValType *params)
{
	IDprop *ip;

	ip = emalloc(sizeof(IDprop));
	ip->type = type;
	ip->offset = offset;
	ip->nparams = nparams;
	ip->params = params;

	return ip;
}

Variable *make_var(char *id, ValType type, SourcePos pos)
{
	Variable *vp;

	vp = emalloc(sizeof(Variable));
	vp->id = id;
	vp->type = type;
	vp->pos = pos;
	vp->next = NULL;

	return vp;
}

/* --- error reporting routines --------------------------------------------- */

void _abort_compile(SourcePos *posp, Error err, va_list args);

void abort_c(Error err, ...)
{
	va_list args;

	va_start(args, err);
	_abort_compile(NULL, err, args);
	va_end(args);
}

void abort_cp(SourcePos *posp, Error err, ...)
{
	va_list args;

	va_start(args, err);
	_abort_compile(posp, err, args);
	va_end(args);
}

void _abort_compile(SourcePos *posp, Error err, va_list args)
{
	char expstr[MAX_MESSAGE_LENGTH], *s, *t; 
	int tok;

	if (posp) {
		position = *posp;
	}

	snprintf(expstr, MAX_MESSAGE_LENGTH, "expected %%s, but found %s",
		get_token_string(token.type));

	switch (err) {
		case ERR_ARGUMENT_LIST_OR_VARIABLE_ASSIGNMENT_EXPECTED:
		case ERR_ARRAY_ALLOCATION_OR_EXPRESSION_EXPECTED:
		case ERR_EXIT_EXPRESSION_NOT_ALLOWED_FOR_PROCEDURE:
		case ERR_EXPECT:
		case ERR_EXPRESSION_OR_STRING_EXPECTED:
		case ERR_FACTOR_EXPECTED:
		case ERR_MISSING_EXIT_EXPRESSION_FOR_FUNCTION:
		case ERR_STATEMENT_EXPECTED:
		case ERR_TYPE_EXPECTED:
			break;
		case ERR_TAKES_NO_ARGUMENTS:
			s = va_arg(args, char *);
			t = va_arg(args, char *);
			break;	
		default:
			s = va_arg(args, char *);
			break;
	}

	switch (err) {

		/* Add additional cases here as is necessary, referring to
		 * errmsg.h for all possible errors.  Some errors only become possible
		 * to recognise once we add type checking.  Until you get to type
		 * checking, you can handle such errors by adding the default case.
		 * However, your final submission *must* handle all cases explicitly.
		 */
		case ERR_ARGUMENT_LIST_OR_VARIABLE_ASSIGNMENT_EXPECTED:
			leprintf(expstr, "argument list or variable assignment");
			break;

		case ERR_ARRAY_ALLOCATION_OR_EXPRESSION_EXPECTED:
			leprintf(expstr, "array allocation or expression");
			break;
		
		case ERR_EXIT_EXPRESSION_NOT_ALLOWED_FOR_PROCEDURE:
			leprintf("an exit expression is not allowed for a procedure");
			break;

		case ERR_EXPECT:
			tok = va_arg(args, int);
			leprintf(expstr, get_token_string(tok));
			break;

		case ERR_EXPRESSION_OR_STRING_EXPECTED:
			leprintf(expstr, "expression or string");
			break;

		case ERR_FACTOR_EXPECTED:
			leprintf(expstr, "factor");
			break;

		case ERR_ILLEGAL_ARRAY_OPERATION:
			leprintf("%s is an illegal array operation", s);
			break;
		
		case ERR_MISSING_EXIT_EXPRESSION_FOR_FUNCTION:
			leprintf("missing exit expression for a function");
			break;

		case ERR_MISSING_FUNCTION_ARGUMENT_LIST:
			leprintf("missing argument list for function '%s'", s);
			break;

		case ERR_MULTIPLE_DEFINITION:
			leprintf("multiple definition of '%s'", s);
			break;

		case ERR_NOT_A_FUNCTION:
			leprintf("'%s' is not a function", s);
			break;

		case ERR_NOT_A_PROCEDURE:
			leprintf("'%s' is not a procedure", s);
			break;
		
		case ERR_NOT_A_VARIABLE:
			leprintf("'%s' is not a variable", s);
			break;

		case ERR_NOT_AN_ARRAY:
			leprintf("'%s' is not an array", s);
			break;

		case ERR_SCALAR_VARIABLE_EXPECTED:
			leprintf("expected scalar variable instead of '%s'", s);
			break;

		case ERR_STATEMENT_EXPECTED:
			leprintf(expstr, "statement");
			break;
		
		case ERR_TAKES_NO_ARGUMENTS:
			leprintf("%s '%s' takes no arguments", s, t); 
			break;

		case ERR_TOO_FEW_ARGUMENTS:
			leprintf("too few arguments for call to '%s'", s);
			break;

		case ERR_TOO_MANY_ARGUMENTS:
			leprintf("too many arguments for call to '%s'", s);
			break;

		case ERR_TYPE_EXPECTED:
			leprintf(expstr, "type");
			break;
		
		case ERR_UNKNOWN_IDENTIFIER:
			leprintf("unknown identifier '%s'", s);
			break;

		case ERR_UNREACHABLE:
			leprintf("unreachable: %s", s);
			break;

		default:
			leprintf("Error not yet implemented");
			break;
		
	}
}

/* --- debugging output routines -------------------------------------------- */

#ifdef DEBUG_PARSER

static int indent = 0;

void debug_start(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	debug_info(fmt, ap);
	va_end(ap);
	indent += 2;
}

void debug_end(const char *fmt, ...)
{
	va_list ap;

	indent -= 2;
	va_start(ap, fmt);
	debug_info(fmt, ap);
	va_end(ap);
}

void debug_info(const char *fmt, ...)
{
	int i;
	char buf[MAX_MESSAGE_LENGTH], *buf_ptr;
	va_list ap;

	buf_ptr = buf;

	va_start(ap, fmt);

	for (i = 0; i < indent; i++) {
		*buf_ptr++ = ' ';
	}
	vsprintf(buf_ptr, fmt, ap);

	buf_ptr += strlen(buf_ptr);
	snprintf(buf_ptr, MAX_MESSAGE_LENGTH, " in line %d.\n", position.line);
	fflush(stdout);
	fputs(buf, stdout);
	fflush(NULL);

	va_end(ap);
}

#endif /* DEBUG_PARSER */
