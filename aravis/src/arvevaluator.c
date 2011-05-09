/* Aravis - Digital camera library
 *
 * Copyright © 2009-2010 Emmanuel Pacaud
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Emmanuel Pacaud <emmanuel@gnome.org>
 */

/**
 * SECTION: arvevaluator
 * @short_description: A math expression evaluator with Genicam syntax
 */

#include <arvevaluator.h>
#include <arvtools.h>
#include <arvdebug.h>
#include <math.h>
#include <stdlib.h>

#define ARV_EVALUATOR_STACK_SIZE	128

static GObjectClass *parent_class = NULL;

typedef enum {
	ARV_EVALUATOR_STATUS_SUCCESS,
	ARV_EVALUATOR_STATUS_NOT_PARSED,
	ARV_EVALUATOR_STATUS_EMPTY_EXPRESSION,
	ARV_EVALUATOR_STATUS_PARENTHESES_MISMATCH,
	ARV_EVALUATOR_STATUS_SYNTAX_ERROR,
	ARV_EVALUATOR_STATUS_UNKNOWN_OPERATOR,
	ARV_EVALUATOR_STATUS_UNKNOWN_VARIABLE,
	ARV_EVALUATOR_STATUS_MISSING_ARGUMENTS,
	ARV_EVALUATOR_STATUS_REMAINING_OPERANDS,
	ARV_EVALUATOR_STATUS_DIVISION_BY_ZERO,
	ARV_EVALUATOR_STATUS_STACK_OVERFLOW
} ArvEvaluatorStatus;

static const char *arv_evaluator_status_strings[] = {
	"success",
	"not parsed",
	"empty expression",
	"parentheses mismatch",
	"syntax error",
	"unknown operator",
	"unknown variable",
	"missing arguments",
	"remaining operands",
	"division by zero",
	"stack overflow"
};

struct _ArvEvaluatorPrivate {
	char *expression;
	GSList *rpn_stack;
	ArvEvaluatorStatus parsing_status;
	GHashTable *variables;
};

typedef enum {
	ARV_EVALUATOR_TOKEN_UNKNOWN,
	ARV_EVALUATOR_TOKEN_COMMA,
	ARV_EVALUATOR_TOKEN_TERNARY_QUESTION_MARK,
	ARV_EVALUATOR_TOKEN_TERNARY_COLON,
	ARV_EVALUATOR_TOKEN_LOGICAL_OR,
	ARV_EVALUATOR_TOKEN_LOGICAL_AND,
	ARV_EVALUATOR_TOKEN_BITWISE_NOT,
	ARV_EVALUATOR_TOKEN_BITWISE_OR,
	ARV_EVALUATOR_TOKEN_BITWISE_XOR,
	ARV_EVALUATOR_TOKEN_BITWISE_AND,
	ARV_EVALUATOR_TOKEN_EQUAL,
	ARV_EVALUATOR_TOKEN_NOT_EQUAL,
	ARV_EVALUATOR_TOKEN_LESS_OR_EQUAL,
	ARV_EVALUATOR_TOKEN_GREATER_OR_EQUAL,
	ARV_EVALUATOR_TOKEN_LESS,
	ARV_EVALUATOR_TOKEN_GREATER,
	ARV_EVALUATOR_TOKEN_SHIFT_RIGHT,
	ARV_EVALUATOR_TOKEN_SHIFT_LEFT,
	ARV_EVALUATOR_TOKEN_SUBSTRACTION,
	ARV_EVALUATOR_TOKEN_ADDITION,
	ARV_EVALUATOR_TOKEN_REMAINDER,
	ARV_EVALUATOR_TOKEN_DIVISION,
	ARV_EVALUATOR_TOKEN_MULTIPLICATION,
	ARV_EVALUATOR_TOKEN_POWER,
	ARV_EVALUATOR_TOKEN_MINUS,
	ARV_EVALUATOR_TOKEN_PLUS,
	ARV_EVALUATOR_TOKEN_FUNCTION_SIN,
	ARV_EVALUATOR_TOKEN_FUNCTION_COS,
	ARV_EVALUATOR_TOKEN_FUNCTION_SGN,
	ARV_EVALUATOR_TOKEN_FUNCTION_NEG,
	ARV_EVALUATOR_TOKEN_FUNCTION_ATAN,
	ARV_EVALUATOR_TOKEN_FUNCTION_TAN,
	ARV_EVALUATOR_TOKEN_FUNCTION_ABS,
	ARV_EVALUATOR_TOKEN_FUNCTION_EXP,
	ARV_EVALUATOR_TOKEN_FUNCTION_LN,
	ARV_EVALUATOR_TOKEN_FUNCTION_LG,
	ARV_EVALUATOR_TOKEN_FUNCTION_SQRT,
	ARV_EVALUATOR_TOKEN_FUNCTION_TRUNC,
	ARV_EVALUATOR_TOKEN_FUNCTION_FLOOR,
	ARV_EVALUATOR_TOKEN_FUNCTION_CEIL,
	ARV_EVALUATOR_TOKEN_FUNCTION_ASIN,
	ARV_EVALUATOR_TOKEN_FUNCTION_ACOS,
	ARV_EVALUATOR_TOKEN_RIGHT_PARENTHESIS,
	ARV_EVALUATOR_TOKEN_LEFT_PARENTHESIS,
	ARV_EVALUATOR_TOKEN_CONSTANT_INT64,
	ARV_EVALUATOR_TOKEN_CONSTANT_DOUBLE,
	ARV_EVALUATOR_TOKEN_VARIABLE
} ArvEvaluatorTokenId;

typedef enum {
	ARV_EVALUATOR_TOKEN_ASSOCIATIVITY_LEFT_TO_RIGHT,
	ARV_EVALUATOR_TOKEN_ASSOCIATIVITY_RIGHT_TO_LEFT
} ArvEvaluatorTokenAssociativity;

typedef struct {
	const char *		tag;
	int			precedence;
	int			n_args;
	ArvEvaluatorTokenAssociativity	associativity;
} ArvEvaluatorTokenInfos;

static ArvEvaluatorTokenInfos arv_evaluator_token_infos[] = {
	{"",	0,	1, 0}, /* UNKNOWN */
	{",",	0, 	0, ARV_EVALUATOR_TOKEN_ASSOCIATIVITY_LEFT_TO_RIGHT}, /* COMMA */
	{"?",	5,	3, ARV_EVALUATOR_TOKEN_ASSOCIATIVITY_RIGHT_TO_LEFT}, /* TERNARY_QUESTION_MARK */
	{":",	5,	1, ARV_EVALUATOR_TOKEN_ASSOCIATIVITY_RIGHT_TO_LEFT}, /* TERNARY_COLON */
	{"||",	10,	2, ARV_EVALUATOR_TOKEN_ASSOCIATIVITY_LEFT_TO_RIGHT}, /* LOGICAL_OR */
	{"&&",	20,	2, ARV_EVALUATOR_TOKEN_ASSOCIATIVITY_LEFT_TO_RIGHT}, /* LOGICAL_AND */
	{"~",	30,	1, ARV_EVALUATOR_TOKEN_ASSOCIATIVITY_LEFT_TO_RIGHT}, /* BITWISE_NOT */
	{"|",	40,	2, ARV_EVALUATOR_TOKEN_ASSOCIATIVITY_LEFT_TO_RIGHT}, /* BITWISE_OR */
	{"^",	50,	2, ARV_EVALUATOR_TOKEN_ASSOCIATIVITY_LEFT_TO_RIGHT}, /* BITWISE_XOR */
	{"&",	60,	2, ARV_EVALUATOR_TOKEN_ASSOCIATIVITY_LEFT_TO_RIGHT}, /* BITWISE_AND */
	{"=",	70,	2, ARV_EVALUATOR_TOKEN_ASSOCIATIVITY_LEFT_TO_RIGHT}, /* EQUAL, */
	{"<>",	70,	2, ARV_EVALUATOR_TOKEN_ASSOCIATIVITY_LEFT_TO_RIGHT}, /* NOT_EQUAL */
	{"<=",	80,	2, ARV_EVALUATOR_TOKEN_ASSOCIATIVITY_LEFT_TO_RIGHT}, /* LESS_OR_EQUAL */
	{">=",	80,	2, ARV_EVALUATOR_TOKEN_ASSOCIATIVITY_LEFT_TO_RIGHT}, /* GREATER_OR_EQUAL */
	{"<",	80,	2, ARV_EVALUATOR_TOKEN_ASSOCIATIVITY_LEFT_TO_RIGHT}, /* LESS */
	{">",	80,	2, ARV_EVALUATOR_TOKEN_ASSOCIATIVITY_LEFT_TO_RIGHT}, /* GREATER */
	{">>",	90,	2, ARV_EVALUATOR_TOKEN_ASSOCIATIVITY_LEFT_TO_RIGHT}, /* SHIFT_RIGHT */
	{"<<",	90,	2, ARV_EVALUATOR_TOKEN_ASSOCIATIVITY_LEFT_TO_RIGHT}, /* SHIFT_LEFT */
	{"-",	100, 	2, ARV_EVALUATOR_TOKEN_ASSOCIATIVITY_LEFT_TO_RIGHT}, /* SUBSTRACTION */
	{"+",	100, 	2, ARV_EVALUATOR_TOKEN_ASSOCIATIVITY_LEFT_TO_RIGHT}, /* ADDITION */
	{"%",	110,	2, ARV_EVALUATOR_TOKEN_ASSOCIATIVITY_LEFT_TO_RIGHT}, /* REMAINDER */
	{"/",	110,	2, ARV_EVALUATOR_TOKEN_ASSOCIATIVITY_LEFT_TO_RIGHT}, /* DIVISION */
	{"*",	110, 	2, ARV_EVALUATOR_TOKEN_ASSOCIATIVITY_LEFT_TO_RIGHT}, /* MULTIPLICATION */
	{"**",	120,	2, ARV_EVALUATOR_TOKEN_ASSOCIATIVITY_RIGHT_TO_LEFT}, /* POWER */
	{"minus",130, 	1, ARV_EVALUATOR_TOKEN_ASSOCIATIVITY_RIGHT_TO_LEFT}, /* MINUS */
	{"plus",130, 	1, ARV_EVALUATOR_TOKEN_ASSOCIATIVITY_RIGHT_TO_LEFT}, /* PLUS */
	{"sin",	200,	1, 0}, /* FUNCTION_SIN */
	{"cos",	200,	1, 0}, /* FUNCTION_COS */
	{"sgn",	200,	1, 0}, /* FUNCTION_SGN */
	{"neg",	200,	1, 0}, /* FUNCTION_NEG */
	{"atan",200,	1, 0}, /* FUNCTION_ATAN */
	{"tan" ,200,	1, 0}, /* FUNCTION_TAN */
	{"abs" ,200,	1, 0}, /* FUNCTION_ABS */
	{"exp" ,200,	1, 0}, /* FUNCTION_EXP */
	{"ln",  200,	1, 0}, /* FUNCTION_LN */
	{"lg",  200,	1, 0}, /* FUNCTION_LG */
	{"sqrt",200,	1, 0}, /* FUNCTION_SQRT */
	{"trunc",200,	1, 0}, /* FUNCTION_TRUNC */
	{"floor",200,	1, 0}, /* FUNCTION_FLOOR */
	{"ceil",200, 	1, 0}, /* FUNCTION_CEIL */
	{"asin",200, 	1, 0}, /* FUNCTION_ASIN */
	{"acos",200, 	1, 0}, /* FUNCTION_ACOS */
	{")",	990, 	0, 0}, /* RIGHT_PARENTHESIS */
	{"(",	-1, 	0, 0}, /* LEFT_PARENTHESIS */
	{"int64" ,200,	0, 0}, /* CONSTANT_INT64 */
	{"double",200,	0, 0}, /* CONSTANT_DOUBLE */
	{"var",	200,	0, 0}, /* VARIABLE */
};

typedef struct {
	ArvEvaluatorTokenId	token_id;
	union {
		double		v_double;
		gint64		v_int64;
		char * 		name;
	} data;
} ArvEvaluatorToken;

static ArvEvaluatorToken *
arv_evaluator_token_new (ArvEvaluatorTokenId token_id)
{
	ArvEvaluatorToken *token = g_new0 (ArvEvaluatorToken, 1);
	token->token_id = token_id;

	return token;
}

static ArvEvaluatorToken *
arv_evaluator_token_new_double (double v_double)
{
	ArvEvaluatorToken *token = arv_evaluator_token_new (ARV_EVALUATOR_TOKEN_CONSTANT_DOUBLE);
	token->data.v_double = v_double;

	return token;
}

static ArvEvaluatorToken *
arv_evaluator_token_new_int64 (double v_int64)
{
	ArvEvaluatorToken *token = arv_evaluator_token_new (ARV_EVALUATOR_TOKEN_CONSTANT_INT64);
	token->data.v_int64 = v_int64;

	return token;
}

static ArvEvaluatorToken *
arv_evaluator_token_new_variable (const char *name)
{
	ArvEvaluatorToken *token = arv_evaluator_token_new (ARV_EVALUATOR_TOKEN_VARIABLE);
	token->data.name = g_strdup (name);

	return token;
}

static void
arv_evaluator_token_free (ArvEvaluatorToken *token)
{
	if (token == NULL)
		return;

	if (token->token_id == ARV_EVALUATOR_TOKEN_VARIABLE)
		g_free (token->data.name);
	g_free (token);
}

void
arv_evaluator_token_debug (ArvEvaluatorToken *token, GHashTable *variables)
{
	ArvValue *value;

	g_return_if_fail (token != NULL);

	switch (token->token_id) {
		case ARV_EVALUATOR_TOKEN_VARIABLE:
			value = g_hash_table_lookup (variables, token->data.name);
			arv_debug ("evaluator", "(var) %s = %g%s", token->data.name,
				   value != NULL ? arv_value_get_double (value) : 0,
				   value != NULL ? "" : " not found");
			break;
		case ARV_EVALUATOR_TOKEN_CONSTANT_INT64:
			arv_debug ("evaluator", "(int64) %Ld", token->data.v_int64);
			break;
		case ARV_EVALUATOR_TOKEN_CONSTANT_DOUBLE:
			arv_debug ("evaluator", "(double) %g", token->data.v_double);
			break;
		default:
			arv_debug ("evaluator", "(operator) %s", arv_evaluator_token_infos[token->token_id].tag);
	}
}

static gboolean
arv_evaluator_token_is_operand (ArvEvaluatorToken *token)
{
	return (token != NULL &&
		token->token_id > ARV_EVALUATOR_TOKEN_LEFT_PARENTHESIS);
}

static gboolean
arv_evaluator_token_is_operator (ArvEvaluatorToken *token)
{
	return (token != NULL &&
		token->token_id > ARV_EVALUATOR_TOKEN_UNKNOWN &&
		token->token_id < ARV_EVALUATOR_TOKEN_RIGHT_PARENTHESIS);
}

static gboolean
arv_evaluator_token_is_comma (ArvEvaluatorToken *token)
{
	return (token != NULL &&
		token->token_id == ARV_EVALUATOR_TOKEN_COMMA);
}

static gboolean
arv_evaluator_token_is_left_parenthesis (ArvEvaluatorToken *token)
{
	return (token != NULL &&
		token->token_id == ARV_EVALUATOR_TOKEN_LEFT_PARENTHESIS);
}

static gboolean
arv_evaluator_token_is_right_parenthesis (ArvEvaluatorToken *token)
{
	return (token != NULL &&
		token->token_id == ARV_EVALUATOR_TOKEN_RIGHT_PARENTHESIS);
}

gboolean
arv_evaluator_token_compare_precedence (ArvEvaluatorToken *a, ArvEvaluatorToken *b)
{
	gint a_precedence;
	gint b_precedence;
	ArvEvaluatorTokenAssociativity a_associativity;
	ArvEvaluatorTokenAssociativity b_associativity;

	if (a == NULL || b == NULL ||
	    a->token_id >= G_N_ELEMENTS (arv_evaluator_token_infos) ||
	    b->token_id >= G_N_ELEMENTS (arv_evaluator_token_infos))
		return FALSE;

	a_precedence = arv_evaluator_token_infos[a->token_id].precedence;
	b_precedence = arv_evaluator_token_infos[b->token_id].precedence;
	a_associativity = arv_evaluator_token_infos[a->token_id].associativity;
	b_associativity = arv_evaluator_token_infos[b->token_id].associativity;

	return (((a_precedence <= b_precedence) &&
		 a_associativity == ARV_EVALUATOR_TOKEN_ASSOCIATIVITY_LEFT_TO_RIGHT) ||
		((a_precedence < b_precedence) &&
		 a_associativity == ARV_EVALUATOR_TOKEN_ASSOCIATIVITY_RIGHT_TO_LEFT));
}

ArvEvaluatorToken *
arv_get_next_token (char **expression, ArvEvaluatorToken *previous_token)
{
	ArvEvaluatorToken *token = NULL;
	ArvEvaluatorTokenId token_id = ARV_EVALUATOR_TOKEN_UNKNOWN;

	g_return_val_if_fail (expression != NULL && *expression != NULL, NULL);
	arv_str_skip_spaces (expression);

	if (**expression == '\0')
		return NULL;

	if (g_ascii_isdigit (**expression)) {
		char *end;
		gint64 v_int64;
		double v_double;
		ptrdiff_t length_int64;
		ptrdiff_t length_double;

		v_int64 = g_ascii_strtoll (*expression, &end, 0);
		length_int64 = end - *expression;

		end = *expression;
		arv_str_parse_double (&end, &v_double);
		length_double = end - *expression;

		if (length_double > 0 || length_int64 > 0) {
			if (length_double > length_int64) {
				token = arv_evaluator_token_new_double (v_double);
				*expression += length_double;
			} else {
				token = arv_evaluator_token_new_int64 (v_int64);
				*expression += length_int64;
			}
		}
	} else if (g_ascii_isalpha (**expression) || **expression=='_') {
		char *end = *expression;
		ptrdiff_t token_length;

		while (g_ascii_isalnum (*end) || *end == '_')
			end++;

		token_length = end - *expression;

		if (token_length == 2) {
			if (g_ascii_strncasecmp ("ln", *expression, token_length) == 0)
				token_id = ARV_EVALUATOR_TOKEN_FUNCTION_LN;
			else if (g_ascii_strncasecmp ("lg", *expression, token_length) == 0)
				token_id = ARV_EVALUATOR_TOKEN_FUNCTION_LG;
		} else if (token_length == 3) {
			if (g_ascii_strncasecmp ("sin", *expression, token_length) == 0)
				token_id = ARV_EVALUATOR_TOKEN_FUNCTION_SIN;
			else if (g_ascii_strncasecmp ("cos", *expression, token_length) == 0)
				token_id = ARV_EVALUATOR_TOKEN_FUNCTION_COS;
			else if (g_ascii_strncasecmp ("sgn", *expression, token_length) == 0)
				token_id = ARV_EVALUATOR_TOKEN_FUNCTION_SGN;
			else if (g_ascii_strncasecmp ("neg", *expression, token_length) == 0)
				token_id = ARV_EVALUATOR_TOKEN_FUNCTION_NEG;
			else if (g_ascii_strncasecmp ("tan", *expression, token_length) == 0)
				token_id = ARV_EVALUATOR_TOKEN_FUNCTION_TAN;
			else if (g_ascii_strncasecmp ("abs", *expression, token_length) == 0)
				token_id = ARV_EVALUATOR_TOKEN_FUNCTION_ABS;
			else if (g_ascii_strncasecmp ("exp", *expression, token_length) == 0)
				token_id = ARV_EVALUATOR_TOKEN_FUNCTION_EXP;
		} else if (token_length == 4) {
			if (g_ascii_strncasecmp ("atan", *expression, token_length) == 0)
				token_id = ARV_EVALUATOR_TOKEN_FUNCTION_ATAN;
			else if (g_ascii_strncasecmp ("sqrt", *expression, token_length) == 0)
				token_id = ARV_EVALUATOR_TOKEN_FUNCTION_SQRT;
			else if (g_ascii_strncasecmp ("ceil", *expression, token_length) == 0)
				token_id = ARV_EVALUATOR_TOKEN_FUNCTION_CEIL;
			else if (g_ascii_strncasecmp ("asin", *expression, token_length) == 0)
				token_id = ARV_EVALUATOR_TOKEN_FUNCTION_ASIN;
			else if (g_ascii_strncasecmp ("acos", *expression, token_length) == 0)
				token_id = ARV_EVALUATOR_TOKEN_FUNCTION_ACOS;
		} else if (token_length == 5) {
			if (g_ascii_strncasecmp ("trunc", *expression, token_length) == 0)
				token_id = ARV_EVALUATOR_TOKEN_FUNCTION_TRUNC;
			else if (g_ascii_strncasecmp ("floor", *expression, token_length) == 0)
				token_id = ARV_EVALUATOR_TOKEN_FUNCTION_FLOOR;
		}

		if (token_id != ARV_EVALUATOR_TOKEN_UNKNOWN)
			token = arv_evaluator_token_new (token_id);
		else {
			char *name = g_strndup (*expression, token_length);
			token = arv_evaluator_token_new_variable (name);
			g_free (name);
		}

		*expression = end;
	} else {
		switch (**expression) {
			case '(': token_id = ARV_EVALUATOR_TOKEN_LEFT_PARENTHESIS; break;
			case ')': token_id = ARV_EVALUATOR_TOKEN_RIGHT_PARENTHESIS; break;
			case ',': token_id = ARV_EVALUATOR_TOKEN_COMMA; break;
			case '?': token_id = ARV_EVALUATOR_TOKEN_TERNARY_QUESTION_MARK; break;
			case ':': token_id = ARV_EVALUATOR_TOKEN_TERNARY_COLON; break;
			case '+': if (previous_token != NULL &&
				      (arv_evaluator_token_is_operand (previous_token) ||
				       arv_evaluator_token_is_right_parenthesis (previous_token)))
					  token_id = ARV_EVALUATOR_TOKEN_ADDITION;
				  else
					  token_id = ARV_EVALUATOR_TOKEN_PLUS;
				  break;
			case '-': if (previous_token != NULL &&
				      (arv_evaluator_token_is_operand (previous_token) ||
				       arv_evaluator_token_is_right_parenthesis (previous_token)))
					  token_id = ARV_EVALUATOR_TOKEN_SUBSTRACTION;
				  else
					  token_id = ARV_EVALUATOR_TOKEN_MINUS;
				  break;
			case '*': if ((*expression)[1] == '*') {
					  (*expression)++;
					  token_id = ARV_EVALUATOR_TOKEN_POWER;
				  } else
					  token_id = ARV_EVALUATOR_TOKEN_MULTIPLICATION;
				  break;
			case '/': token_id = ARV_EVALUATOR_TOKEN_DIVISION; break;
			case '%': token_id = ARV_EVALUATOR_TOKEN_REMAINDER; break;
			case '&': if ((*expression)[1] == '&') {
					  (*expression)++;
					  token_id = ARV_EVALUATOR_TOKEN_LOGICAL_AND;
				  } else
					  token_id = ARV_EVALUATOR_TOKEN_BITWISE_AND;
				  break;
			case '|': if ((*expression)[1] == '|') {
					  (*expression)++;
					  token_id = ARV_EVALUATOR_TOKEN_LOGICAL_OR;
				  } else
					  token_id = ARV_EVALUATOR_TOKEN_BITWISE_OR;
				  break;
			case '^': token_id = ARV_EVALUATOR_TOKEN_BITWISE_XOR; break;
			case '~': token_id = ARV_EVALUATOR_TOKEN_BITWISE_NOT; break;
			case '<': if ((*expression)[1] == '>') {
					  (*expression)++;
					  token_id = ARV_EVALUATOR_TOKEN_NOT_EQUAL;
				  } else if ((*expression)[1] == '<') {
					  (*expression)++;
					  token_id = ARV_EVALUATOR_TOKEN_SHIFT_LEFT;
				  } else if ((*expression)[1] == '=') {
					  (*expression)++;
					  token_id = ARV_EVALUATOR_TOKEN_LESS_OR_EQUAL;
				  } else
					  token_id = ARV_EVALUATOR_TOKEN_LESS;
				  break;
			case '>': if ((*expression)[1] == '>') {
					  (*expression)++;
					  token_id = ARV_EVALUATOR_TOKEN_SHIFT_RIGHT;
				  } else if ((*expression)[1] == '=') {
					  (*expression)++;
					  token_id = ARV_EVALUATOR_TOKEN_GREATER_OR_EQUAL;
				  } else
					  token_id = ARV_EVALUATOR_TOKEN_GREATER;
				  break;
			case '=': token_id = ARV_EVALUATOR_TOKEN_EQUAL; break;
		}

		if (token_id != ARV_EVALUATOR_TOKEN_UNKNOWN) {
			(*expression)++;
			token = arv_evaluator_token_new (token_id);
		}
	}

	return token;
}

ArvEvaluatorStatus
evaluate (GSList *token_stack, GHashTable *variables, gint64 *v_int64, double *v_double)
{
	ArvEvaluatorToken *token;
	ArvEvaluatorStatus status;
	GSList *iter;
	ArvValue stack[ARV_EVALUATOR_STACK_SIZE];
	ArvValue *value;
	int index = -1;

	for (iter = token_stack; iter != NULL; iter = iter->next) {
		token = iter->data;

		if (index < (arv_evaluator_token_infos[token->token_id].n_args - 1)) {
			status = ARV_EVALUATOR_STATUS_MISSING_ARGUMENTS;
			goto CLEANUP;
		}

		arv_evaluator_token_debug (token, variables);

		switch (token->token_id) {
			case ARV_EVALUATOR_TOKEN_LOGICAL_AND:
				arv_value_set_int64 (&stack[index-1],
						      arv_value_get_int64 (&stack[index-1]) &&
						      arv_value_get_int64 (&stack[index]));
				break;
			case ARV_EVALUATOR_TOKEN_LOGICAL_OR:
				arv_value_set_int64 (&stack[index-1],
						      arv_value_get_int64 (&stack[index-1]) ||
						      arv_value_get_int64 (&stack[index]));
				break;
			case ARV_EVALUATOR_TOKEN_BITWISE_NOT:
				arv_value_set_int64 (&stack[index],
						      ~arv_value_get_int64 (&stack[index]));
				break;
			case ARV_EVALUATOR_TOKEN_BITWISE_AND:
				arv_value_set_int64 (&stack[index-1],
						      arv_value_get_int64 (&stack[index-1]) &
						      arv_value_get_int64 (&stack[index]));
				break;
			case ARV_EVALUATOR_TOKEN_BITWISE_OR:
				arv_value_set_int64 (&stack[index-1],
						      arv_value_get_int64 (&stack[index-1]) |
						      arv_value_get_int64 (&stack[index]));
				break;
			case ARV_EVALUATOR_TOKEN_BITWISE_XOR:
				arv_value_set_int64 (&stack[index-1],
						      arv_value_get_int64 (&stack[index-1]) ^
						      arv_value_get_int64 (&stack[index]));
				break;
			case ARV_EVALUATOR_TOKEN_EQUAL:
				if (arv_value_holds_int64 (&stack[index-1]) ||
				    arv_value_holds_int64 (&stack[index]))
					arv_value_set_int64 (&stack[index-1],
							      arv_value_get_int64 (&stack[index-1]) ==
							      arv_value_get_int64 (&stack[index]));
				else
					arv_value_set_int64 (&stack[index - 1],
							      arv_value_get_double (&stack[index-1]) ==
							      arv_value_get_double (&stack[index]));
				break;
			case ARV_EVALUATOR_TOKEN_NOT_EQUAL:
				if (arv_value_holds_int64 (&stack[index-1]) ||
				    arv_value_holds_int64 (&stack[index]))
					arv_value_set_int64 (&stack[index-1],
							      arv_value_get_int64 (&stack[index-1]) !=
							      arv_value_get_int64 (&stack[index]));
				else
					arv_value_set_int64 (&stack[index - 1],
							      arv_value_get_double (&stack[index-1]) !=
							      arv_value_get_double (&stack[index]));
				break;
			case ARV_EVALUATOR_TOKEN_LESS_OR_EQUAL:
				if (arv_value_holds_int64 (&stack[index-1]) ||
				    arv_value_holds_int64 (&stack[index]))
					arv_value_set_int64 (&stack[index-1],
							      arv_value_get_int64 (&stack[index-1]) <=
							      arv_value_get_int64 (&stack[index]));
				else
					arv_value_set_int64 (&stack[index - 1],
							      arv_value_get_double (&stack[index-1]) <=
							      arv_value_get_double (&stack[index]));
				break;
			case ARV_EVALUATOR_TOKEN_GREATER_OR_EQUAL:
				if (arv_value_holds_int64 (&stack[index-1]) ||
				    arv_value_holds_int64 (&stack[index]))
					arv_value_set_int64 (&stack[index-1],
							      arv_value_get_int64 (&stack[index-1]) >=
							      arv_value_get_int64 (&stack[index]));
				else
					arv_value_set_int64 (&stack[index - 1],
							      arv_value_get_double (&stack[index-1]) >=
							      arv_value_get_double (&stack[index]));
				break;
			case ARV_EVALUATOR_TOKEN_LESS:
				if (arv_value_holds_int64 (&stack[index-1]) ||
				    arv_value_holds_int64 (&stack[index]))
					arv_value_set_int64 (&stack[index-1],
							      arv_value_get_int64 (&stack[index-1]) <
							      arv_value_get_int64 (&stack[index]));
				else
					arv_value_set_int64 (&stack[index - 1],
							      arv_value_get_double (&stack[index-1]) <
							      arv_value_get_double (&stack[index]));
				break;
			case ARV_EVALUATOR_TOKEN_GREATER:
				if (arv_value_holds_int64 (&stack[index-1]) ||
				    arv_value_holds_int64 (&stack[index]))
					arv_value_set_int64 (&stack[index-1],
							      arv_value_get_int64 (&stack[index-1]) >
							      arv_value_get_int64 (&stack[index]));
				else
					arv_value_set_int64 (&stack[index - 1],
							      arv_value_get_double (&stack[index-1]) >
							      arv_value_get_double (&stack[index]));
				break;
			case ARV_EVALUATOR_TOKEN_SHIFT_RIGHT:
				arv_value_set_int64 (&stack[index-1],
						      arv_value_get_int64 (&stack[index-1]) >>
						      arv_value_get_int64 (&stack[index]));
				break;
			case ARV_EVALUATOR_TOKEN_SHIFT_LEFT:
				arv_value_set_int64 (&stack[index-1],
						      arv_value_get_int64 (&stack[index-1]) <<
						      arv_value_get_int64 (&stack[index]));
				break;
			case ARV_EVALUATOR_TOKEN_SUBSTRACTION:
				if (arv_value_holds_double (&stack[index-1]) ||
				    arv_value_holds_double (&stack[index]))
					arv_value_set_double (&stack[index-1],
							       arv_value_get_double (&stack[index-1]) -
							       arv_value_get_double (&stack[index]));
				else
					arv_value_set_int64 (&stack[index-1],
							      arv_value_get_int64 (&stack[index-1]) -
							      arv_value_get_int64 (&stack[index]));
				break;
			case ARV_EVALUATOR_TOKEN_ADDITION:
				if (arv_value_holds_double (&stack[index-1]) ||
				    arv_value_holds_double (&stack[index]))
					arv_value_set_double (&stack[index-1],
							       arv_value_get_double (&stack[index-1]) +
							       arv_value_get_double (&stack[index]));
				else
					arv_value_set_int64 (&stack[index-1],
							      arv_value_get_int64 (&stack[index-1]) +
							      arv_value_get_int64 (&stack[index]));
				break;
			case ARV_EVALUATOR_TOKEN_REMAINDER:
				if (arv_value_get_int64 (&stack[index]) == 0) {
					status = ARV_EVALUATOR_STATUS_DIVISION_BY_ZERO;
					goto CLEANUP;
				}
				arv_value_set_int64 (&stack[index-1],
						      arv_value_get_int64 (&stack[index-1]) %
						      arv_value_get_int64 (&stack[index]));
				break;
			case ARV_EVALUATOR_TOKEN_DIVISION:
				if (arv_value_holds_double (&stack[index-1]) ||
				    arv_value_holds_double (&stack[index]) ||
				    /* Do float division if asked for a float value, even
				     * with integer operands. */
				    v_double != NULL) {
					if (arv_value_get_double (&stack[index]) == 0.0) {
						status = ARV_EVALUATOR_STATUS_DIVISION_BY_ZERO;
						goto CLEANUP;
					}
					arv_value_set_double (&stack[index-1],
							       arv_value_get_double (&stack[index-1]) /
							       arv_value_get_double (&stack[index]));
				} else {
					if (arv_value_get_int64 (&stack[index]) == 0) {
						status = ARV_EVALUATOR_STATUS_DIVISION_BY_ZERO;
						goto CLEANUP;
					}
					arv_value_set_int64 (&stack[index-1],
							      arv_value_get_int64 (&stack[index-1]) /
							      arv_value_get_int64 (&stack[index]));
				}
				break;
			case ARV_EVALUATOR_TOKEN_MULTIPLICATION:
				if (arv_value_holds_double (&stack[index-1]) ||
				    arv_value_holds_double (&stack[index]))
					arv_value_set_double (&stack[index-1],
							       arv_value_get_double (&stack[index-1]) *
							       arv_value_get_double (&stack[index]));
				else
					arv_value_set_int64 (&stack[index-1],
							      arv_value_get_int64 (&stack[index-1]) *
							      arv_value_get_int64 (&stack[index]));
				break;
			case ARV_EVALUATOR_TOKEN_POWER:
				arv_value_set_double (&stack[index-1],
						       pow (arv_value_get_double(&stack[index-1]),
							    arv_value_get_double(&stack[index])));
				break;
			case ARV_EVALUATOR_TOKEN_MINUS:
				if (arv_value_holds_double (&stack[index]))
					arv_value_set_double (&stack[index],
							       -arv_value_get_double (&stack[index]));
				else
					arv_value_set_int64 (&stack[index],
							      -arv_value_get_int64 (&stack[index]));
				break;
			case ARV_EVALUATOR_TOKEN_PLUS:
				break;
			case ARV_EVALUATOR_TOKEN_FUNCTION_SIN:
				arv_value_set_double (&stack[index], sin (arv_value_get_double (&stack[index])));
				break;
			case ARV_EVALUATOR_TOKEN_FUNCTION_COS:
				arv_value_set_double (&stack[index], cos (arv_value_get_double (&stack[index])));
				break;
			case ARV_EVALUATOR_TOKEN_FUNCTION_SGN:
				if (arv_value_holds_double (&stack[index])) {
					double value = arv_value_get_double (&stack[index]);
					if (value < 0.0)
						arv_value_set_int64 (&stack[index], -1);
					else if (value > 0.0)
						arv_value_set_int64 (&stack[index], 1);
					else
						arv_value_set_int64 (&stack[index], 0);
				} else {
					gint64 value = arv_value_get_int64 (&stack[index]);
					if (value < 0)
						arv_value_set_int64 (&stack[index], -1);
					else if (value > 0)
						arv_value_set_int64 (&stack[index], 1);
					else
						arv_value_set_int64 (&stack[index], 0);
				}
				break;
			case ARV_EVALUATOR_TOKEN_FUNCTION_NEG:
				if (arv_value_holds_double (&stack[index]))
					arv_value_set_double (&stack[index],
							       -arv_value_get_double (&stack[index-1]));
				else
					arv_value_set_int64 (&stack[index],
							      -arv_value_get_int64 (&stack[index]));
				break;
			case ARV_EVALUATOR_TOKEN_FUNCTION_ATAN:
				arv_value_set_double (&stack[index], atan (arv_value_get_double (&stack[index])));
				break;
			case ARV_EVALUATOR_TOKEN_FUNCTION_TAN:
				arv_value_set_double (&stack[index], tan (arv_value_get_double (&stack[index])));
				break;
			case ARV_EVALUATOR_TOKEN_FUNCTION_ABS:
				if (arv_value_holds_double (&stack[index]))
					arv_value_set_double (&stack[index],
							       fabs (arv_value_get_double (&stack[index-1])));
				else
					arv_value_set_int64 (&stack[index],
							      abs (arv_value_get_int64 (&stack[index])));
				break;
			case ARV_EVALUATOR_TOKEN_FUNCTION_EXP:
				arv_value_set_double (&stack[index], exp (arv_value_get_double (&stack[index])));
				break;
			case ARV_EVALUATOR_TOKEN_FUNCTION_LN:
				arv_value_set_double (&stack[index], log (arv_value_get_double (&stack[index])));
				break;
			case ARV_EVALUATOR_TOKEN_FUNCTION_LG:
				arv_value_set_double (&stack[index], log10 (arv_value_get_double (&stack[index])));
				break;
			case ARV_EVALUATOR_TOKEN_FUNCTION_SQRT:
				arv_value_set_double (&stack[index], sqrt (arv_value_get_double (&stack[index])));
				break;
			case ARV_EVALUATOR_TOKEN_FUNCTION_TRUNC:
				if (arv_value_get_double (&stack[index]) > 0.0)
					arv_value_set_double (&stack[index],
							       floor (arv_value_get_double (&stack[index])));
				else
					arv_value_set_double (&stack[index],
							       ceil (arv_value_get_double (&stack[index])));
				break;
			case ARV_EVALUATOR_TOKEN_FUNCTION_FLOOR:
				arv_value_set_double (&stack[index], floor (arv_value_get_double (&stack[index])));
				break;
			case ARV_EVALUATOR_TOKEN_FUNCTION_CEIL:
				arv_value_set_double (&stack[index], ceil (arv_value_get_double (&stack[index])));
				break;
			case ARV_EVALUATOR_TOKEN_FUNCTION_ASIN:
				arv_value_set_double (&stack[index], asin (arv_value_get_double (&stack[index])));
				break;
			case ARV_EVALUATOR_TOKEN_FUNCTION_ACOS:
				arv_value_set_double (&stack[index], acos (arv_value_get_double (&stack[index])));
				break;
			case ARV_EVALUATOR_TOKEN_CONSTANT_INT64:
				arv_value_set_int64 (&stack[index+1], token->data.v_int64);
				break;
			case ARV_EVALUATOR_TOKEN_CONSTANT_DOUBLE:
				arv_value_set_double (&stack[index+1], token->data.v_double);
				break;
			case ARV_EVALUATOR_TOKEN_VARIABLE:
				value = g_hash_table_lookup (variables, token->data.name);
				if (value != NULL)
					arv_value_copy (&stack[index+1], value);
				else
					arv_value_set_int64 (&stack[index+1], 0);
				break;
			case ARV_EVALUATOR_TOKEN_TERNARY_COLON:
				break;
			case ARV_EVALUATOR_TOKEN_TERNARY_QUESTION_MARK:
				if (arv_value_get_int64 (&stack[index-2]) != 0)
					arv_value_copy (&stack[index-2], &stack[index-1]);
				else
					arv_value_copy (&stack[index-2], &stack[index]);
				break;
			default:
				status = ARV_EVALUATOR_STATUS_UNKNOWN_OPERATOR;
				goto CLEANUP;
				break;
		}

		index = index - arv_evaluator_token_infos[token->token_id].n_args + 1;

		if (index >= ARV_EVALUATOR_STACK_SIZE) {
			status = ARV_EVALUATOR_STATUS_STACK_OVERFLOW;
			goto CLEANUP;
		}
	}

	if (index != 0) {
		status = ARV_EVALUATOR_STATUS_REMAINING_OPERANDS;
		goto CLEANUP;
	}

	if (v_double != NULL)
		*v_double = arv_value_get_double (stack);

	if (v_int64 != NULL)
		*v_int64 = arv_value_get_int64 (stack);

	if (arv_value_holds_int64 (stack))
		arv_debug ("evaluator", "[Evaluator::evaluate] Result = (int64) %Ld", arv_value_get_int64 (stack));
	else
		arv_debug ("evaluator", "[Evaluator::evaluate] Result = (double) %g", arv_value_get_double (stack));

	return ARV_EVALUATOR_STATUS_SUCCESS;
CLEANUP:
	if (v_double != NULL)
		*v_double = 0.0;

	if (v_int64 != NULL)
		*v_int64 = 0.0;

	return status;
}

static ArvEvaluatorStatus
parse_expression (char *expression, GSList **rpn_stack)
{
	ArvEvaluatorToken *token;
	ArvEvaluatorToken *previous_token = NULL;
	ArvEvaluatorStatus status;
	GSList *token_stack = NULL;
	GSList *operator_stack = NULL;
	GSList *iter;

	arv_debug ("evaluator", expression);

	/* Dijkstra's "shunting yard" algorithm */
	/* http://en.wikipedia.org/wiki/Shunting-yard_algorithm */

	do {
		token = arv_get_next_token (&expression, previous_token);
		previous_token = token;
		if (token != NULL) {
			if (arv_evaluator_token_is_operand (token)) {
				token_stack = g_slist_prepend (token_stack, token);
			} else if (arv_evaluator_token_is_comma (token)) {
				while (operator_stack != NULL &&
				       !arv_evaluator_token_is_left_parenthesis (operator_stack->data)) {
					token_stack = g_slist_prepend (token_stack, operator_stack->data);
					operator_stack = g_slist_delete_link (operator_stack, operator_stack);
				}
				if (operator_stack == NULL ||
				    !arv_evaluator_token_is_left_parenthesis (operator_stack->data)) {
					status = ARV_EVALUATOR_STATUS_PARENTHESES_MISMATCH;
					goto CLEANUP;
				}
				arv_evaluator_token_free (token);
			} else if (arv_evaluator_token_is_operator (token)) {
				while (operator_stack != NULL &&
				       arv_evaluator_token_compare_precedence (token, operator_stack->data)) {
					token_stack = g_slist_prepend (token_stack, operator_stack->data);
					operator_stack = g_slist_delete_link (operator_stack, operator_stack);
				}
				operator_stack = g_slist_prepend (operator_stack, token);
			} else if (arv_evaluator_token_is_left_parenthesis (token)) {
				operator_stack = g_slist_prepend (operator_stack, token);
			} else if (arv_evaluator_token_is_right_parenthesis (token)) {
				while (operator_stack != NULL &&
				       !arv_evaluator_token_is_left_parenthesis (operator_stack->data)) {
					token_stack = g_slist_prepend (token_stack, operator_stack->data);
					operator_stack = g_slist_delete_link (operator_stack, operator_stack);
				}
				if (operator_stack == NULL) {
					status = ARV_EVALUATOR_STATUS_PARENTHESES_MISMATCH;
					goto CLEANUP;
				}
				arv_evaluator_token_free (token);
				arv_evaluator_token_free (operator_stack->data);
				operator_stack = g_slist_delete_link (operator_stack, operator_stack);
			} else {
				status = ARV_EVALUATOR_STATUS_SYNTAX_ERROR;
				goto CLEANUP;
			}
		} else if (*expression != '\0') {
			status = ARV_EVALUATOR_STATUS_SYNTAX_ERROR;
			goto CLEANUP;
		}
	} while (token != NULL);

	while (operator_stack != NULL) {
		if (arv_evaluator_token_is_left_parenthesis (operator_stack->data)) {
			status = ARV_EVALUATOR_STATUS_PARENTHESES_MISMATCH;
			goto CLEANUP;
		}

		token_stack = g_slist_prepend (token_stack, operator_stack->data);
		operator_stack = g_slist_delete_link (operator_stack, operator_stack);
	}

	*rpn_stack = g_slist_reverse (token_stack);

	return *rpn_stack == NULL ? ARV_EVALUATOR_STATUS_EMPTY_EXPRESSION : ARV_EVALUATOR_STATUS_SUCCESS;

CLEANUP:

	if (token != NULL)
		arv_evaluator_token_free (token);
	for (iter = token_stack; iter != NULL; iter = iter->next)
		arv_evaluator_token_free (iter->data);
	g_slist_free (token_stack);
	for (iter = operator_stack; iter != NULL; iter = iter->next)
		arv_evaluator_token_free (iter->data);
	g_slist_free (operator_stack);

	return status;
}

static void
arv_evaluator_set_error (GError **error, ArvEvaluatorStatus status)
{
	g_set_error (error,
		     g_quark_from_string ("Aravis"),
		     status,
		     "Parsing error: %s",
		     arv_evaluator_status_strings [MIN (status,
							G_N_ELEMENTS (arv_evaluator_status_strings)-1)]);

	arv_debug ("evaluator", "[Evaluator::evaluate] Error '%s'",
		   arv_evaluator_status_strings [MIN (status,
						      G_N_ELEMENTS (arv_evaluator_status_strings)-1)]);
}

double
arv_evaluator_evaluate_as_double (ArvEvaluator *evaluator, GError **error)
{
	ArvEvaluatorStatus status;
	double value;

	g_return_val_if_fail (ARV_IS_EVALUATOR (evaluator), 0.0);

	arv_debug ("evaluator", "[Evaluator::evaluate_as_double] Expression = '%s'", 
		   evaluator->priv->expression);

	if (evaluator->priv->parsing_status == ARV_EVALUATOR_STATUS_NOT_PARSED) {
		evaluator->priv->parsing_status = parse_expression (evaluator->priv->expression,
								    &evaluator->priv->rpn_stack);
		arv_debug ("evaluator", "[Evaluator::evaluate_as_double] Parsing status = %d",
			   evaluator->priv->parsing_status);
	}

	if (evaluator->priv->parsing_status != ARV_EVALUATOR_STATUS_SUCCESS) {
		arv_evaluator_set_error (error, evaluator->priv->parsing_status);
		return 0.0;
	}

	status = evaluate (evaluator->priv->rpn_stack, evaluator->priv->variables, NULL, &value);
	if (status != ARV_EVALUATOR_STATUS_SUCCESS) {
		arv_evaluator_set_error (error, status);
		return 0.0;
	}

	return value;
}

gint64
arv_evaluator_evaluate_as_int64 (ArvEvaluator *evaluator, GError **error)
{
	ArvEvaluatorStatus status;
	gint64 value;

	g_return_val_if_fail (ARV_IS_EVALUATOR (evaluator), 0.0);

	arv_debug ("evaluator", "[Evaluator::evaluate_as_int64] Expression = '%s'", 
		   evaluator->priv->expression);

	if (evaluator->priv->parsing_status == ARV_EVALUATOR_STATUS_NOT_PARSED) {
		evaluator->priv->parsing_status = parse_expression (evaluator->priv->expression,
								    &evaluator->priv->rpn_stack);
		arv_debug ("evaluator", "[Evaluator::evaluate_as_int64] Parsing status = %d",
			   evaluator->priv->parsing_status);
	}

	if (evaluator->priv->parsing_status != ARV_EVALUATOR_STATUS_SUCCESS) {
		arv_evaluator_set_error (error, evaluator->priv->parsing_status);
		return 0.0;
	}

	status = evaluate (evaluator->priv->rpn_stack, evaluator->priv->variables, &value, NULL);
	if (status != ARV_EVALUATOR_STATUS_SUCCESS) {
		arv_evaluator_set_error (error, status);
		return 0.0;
	}

	return value;
}

void
arv_evaluator_set_expression (ArvEvaluator *evaluator, const char *expression)
{
	GSList *iter;

	g_return_if_fail (ARV_IS_EVALUATOR (evaluator));

	if (g_strcmp0 (expression, evaluator->priv->expression) == 0)
		return;

	g_free (evaluator->priv->expression);
	evaluator->priv->expression = NULL;
	for (iter = evaluator->priv->rpn_stack; iter != NULL; iter = iter->next)
		arv_evaluator_token_free (iter->data);
	g_slist_free (evaluator->priv->rpn_stack);
	evaluator->priv->rpn_stack = NULL;

	if (expression == NULL) {
		evaluator->priv->parsing_status = ARV_EVALUATOR_STATUS_EMPTY_EXPRESSION;
		return;
	}

	evaluator->priv->parsing_status = ARV_EVALUATOR_STATUS_NOT_PARSED;
	evaluator->priv->expression = g_strdup (expression);
}

const char *
arv_evaluator_get_expression (ArvEvaluator *evaluator)
{
	g_return_val_if_fail (ARV_IS_EVALUATOR (evaluator), NULL);

	return evaluator->priv->expression;
}

void
arv_evaluator_set_double_variable (ArvEvaluator *evaluator, const char *name, double v_double)
{
	ArvValue *old_value;

	g_return_if_fail (ARV_IS_EVALUATOR (evaluator));
	g_return_if_fail (name != NULL);

	old_value = g_hash_table_lookup (evaluator->priv->variables, name);
	if (old_value != NULL && (arv_value_get_double (old_value) == v_double))
		return;

	g_hash_table_insert (evaluator->priv->variables,
			     g_strdup (name),
			     arv_value_new_double (v_double));

	arv_debug ("evaluator", "[Evaluator::set_double_variable] %s = %g",
		   name, v_double);
}

void
arv_evaluator_set_int64_variable (ArvEvaluator *evaluator, const char *name, gint64 v_int64)
{
	ArvValue *old_value;

	g_return_if_fail (ARV_IS_EVALUATOR (evaluator));
	g_return_if_fail (name != NULL);

	old_value = g_hash_table_lookup (evaluator->priv->variables, name);
	if (old_value != NULL && (arv_value_get_int64 (old_value) == v_int64))
		return;

	g_hash_table_insert (evaluator->priv->variables,
			     g_strdup (name),
			     arv_value_new_int64 (v_int64));

	arv_debug ("evaluator", "[Evaluator::set_int64_variable] %s = %Ld",
		   name, v_int64);
}

/** 
 * arv_evaluator_new:
 * @expression: (allow-none): an evaluator expression
 * Return value: a new #ArvEvaluator object.
 *
 * Creates a new #ArvEvaluator object. The syntax is described in the genicam standard specification.
 */

ArvEvaluator *
arv_evaluator_new (const char *expression)
{
	ArvEvaluator *evaluator;

	evaluator = g_object_new (ARV_TYPE_EVALUATOR, NULL);

	arv_evaluator_set_expression (evaluator, expression);

	return evaluator;
}

static void
arv_evaluator_init (ArvEvaluator *evaluator)
{
	evaluator->priv = G_TYPE_INSTANCE_GET_PRIVATE (evaluator, ARV_TYPE_EVALUATOR, ArvEvaluatorPrivate);

	evaluator->priv->expression = NULL;
	evaluator->priv->rpn_stack = NULL;
	evaluator->priv->variables = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) arv_value_free);

	arv_evaluator_set_double_variable (evaluator, "PI", M_PI);
	arv_evaluator_set_double_variable (evaluator, "E", M_E);
}

static void
arv_evaluator_finalize (GObject *object)
{
	ArvEvaluator *evaluator = ARV_EVALUATOR (object);

	arv_evaluator_set_expression (evaluator, NULL);
	g_hash_table_unref (evaluator->priv->variables);

	parent_class->finalize (object);
}

static void
arv_evaluator_class_init (ArvEvaluatorClass *evaluator_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (evaluator_class);

	g_type_class_add_private (evaluator_class, sizeof (ArvEvaluatorPrivate));

	parent_class = g_type_class_peek_parent (evaluator_class);

	object_class->finalize = arv_evaluator_finalize;
}

G_DEFINE_TYPE (ArvEvaluator, arv_evaluator, G_TYPE_OBJECT)
