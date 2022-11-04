/**
 * @file    symboltable.c
 * @brief   A symbol table for SIMPL-2021.
 * @author  W.H.K. Bester (whkbester@cs.sun.ac.za)
 * @date    2021-08-23
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "boolean.h"
#include "error.h"
#include "hashtable.h"
#include "symboltable.h"
#include "token.h"

/* --- global static variables ---------------------------------------------- */

static HashTab *table, *saved_table;
/* Nothing here, but note that the next variable keeps a running count of
 * the number of variables in the current symbol table.  It will be necessary
 * during code generation to compute the size of the local variable array of a
 * method frame in the Java virtual machine.
 */
static unsigned int curr_offset;

/* --- function prototypes -------------------------------------------------- */

static void valstr(void *key, void *p, char *str);
static void freeprop(void *p);
static unsigned int shift_hash(void *key, unsigned int size);
static int key_strcmp(void *val1, void *val2);

/* --- symbol table interface ----------------------------------------------- */

void init_symbol_table(void)
{
	saved_table = NULL;

	if ((table = ht_init(0.75f, shift_hash, key_strcmp)) == NULL) { 
		eprintf("Symbol table could not be initialised");
	}
	curr_offset = 1;
}

Boolean open_subroutine(char *id, IDprop *prop)
{
	/* - Insert the subroutine name into the global symbol table; return TRUE or
	 *   FALSE, depending on whether or not the insertion succeeded.
	 * - Save the global symbol table to saved_table, initialise a new hash
	 *   table for the subroutine, and reset the current offset.
	 */
	if (ht_insert(table, id, prop) != EXIT_SUCCESS) {
		return FALSE;
	}
	saved_table = table;

	if ((table = ht_init(0.75f, shift_hash, key_strcmp)) == NULL) {
		table = saved_table;
		eprintf("Symbol table could not be initialised"); 
	}
	curr_offset = 1;
	return TRUE;
}

void close_subroutine(void)
{
	/* Release the subroutine table, and reactivate the global table. */
	ht_free(table, free, freeprop);
	table = saved_table;
	//saved_table = NULL;
}

Boolean insert_name(char *id, IDprop *prop)
{
	/* Insert the properties of the identifier into the hash table, and
	 * remember to increment the current offset pointer if the identifier is a
	 * variable.
	 *
	 * VERY IMPORTANT: Remember to read the documentation of this function in
	 * the header file.
	*/
	IDprop *p;
	if (find_name(id, &p)) {
		return FALSE;
	}
	if(ht_insert(table, id, prop) != EXIT_SUCCESS) {
		return FALSE;
	}
	if (IS_VARIABLE(prop->type)) {
		prop->offset = curr_offset;
		curr_offset++;
	}
	return TRUE;
}

Boolean find_name(char *id, IDprop **prop)
{
	Boolean found;

	/*Nothing, unless you want to.*/
	found = ht_search(table, id, (void **) prop);
	if (!found && saved_table) {
		found = ht_search(saved_table, id, (void **) prop);
		if (found && !IS_CALLABLE_TYPE((*prop)->type)) {
			found = FALSE;
		}
	}

	return found;
}

int get_variables_width(void)
{
	return curr_offset;
}

void release_symbol_table(void)
{
	/* Free the underlying structures of the symbol table. */
	ht_free(table, free, freeprop);
	table = NULL;
}

void print_symbol_table(void)
{
	ht_print(table, valstr);
}

/* --- utility functions ---------------------------------------------------- */

static void valstr(void *key, void *p, char *str)
{
	char *keystr = (char *) key;
	IDprop *idpp = (IDprop *) p;

	/* Nothing, but this shoud give you an idea of how to look at the
	 * contents of the symbol table.
	 */

	sprintf(str, "%s.%d", keystr, idpp->offset);
	sprintf(str, "%s@%d[%s]", keystr, idpp->offset,
			get_valtype_string(idpp->type));
}

/* Here you should add your own utility functions, in particular, for
 * deallocation, hashing, and key comparison.  For hashing, you MUST NOT use the
 * simply strategy of summing the integer values of characters.  I suggest you
 * use some kind of cyclic bit shift hash.
 */

static void freeprop(void *p)
{
	IDprop *prop = (IDprop *) p;
	if (IS_CALLABLE_TYPE(prop->type) && prop->nparams > 0) {
		//free(prop->params);
	}
	//free(prop);

}

static unsigned int shift_hash(void *key, unsigned int size)
{
	char *keystr = (char *) key;
	unsigned int i, hash, length;

	hash = 0;
	length = strlen(keystr);
	for (i = 0; i < length; i++) {
		hash = (hash << 5) | (hash >> 27);
		hash += keystr[i];
	}
	return (hash % size);
}

static int key_strcmp(void *v1, void *v2)
{
	return strcmp((char *) v1, (char *) v2);
}
