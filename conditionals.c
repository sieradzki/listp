#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "mpc.h"

#ifdef _WIN32
#include <string.h>

static char buffer[2048];

char *readline(char *prompt)
{
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);
  char *cpy = malloc(strlen(buffer) + 1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy) - 1] = '\n';
  return cpy;
}

void add_history(char *unused) {}

#else
#include <editline.h>
#endif

/* Assert definitions */

#define LASSERT(args, cond, fmt, ...)         \
  if (!(cond))                                \
  {                                           \
    lval *err = lval_err(fmt, ##__VA_ARGS__); \
    lval_del(args);                           \
    return err;                               \
  }

#define LASSERT_TYPE(args, func, index, required)                                      \
  LASSERT(args, args->cell[index]->type == required,                                   \
          "Function '%s' passed incorrect type for argument %i. Got %s, Expected %s.", \
          func, index, ltype_name(args->cell[index]->type), ltype_name(required));

#define LASSERT_COUNT(args, func, required)                                          \
  LASSERT(args, args->count == required,                                             \
          "Function '%s' passed incorrect number of arguments. Got %i, Expected %i", \
          func, args->count, required);

#define LASSERT_EMPTY(args, func, index)       \
  LASSERT(args, args->cell[index]->count != 0, \
          "Function '%s' passed {}!");

struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

/* Create Enumeration of Possible lval Types */
enum
{
  LVAL_NUM,
  LVAL_ERR,
  LVAL_SYM,
  LVAL_FUN,
  LVAL_SEXPR,
  LVAL_QEXPR
};

typedef lval *(*lbuiltin)(lenv *, lval *);

/* Declare New lval Struct */
struct lval
{
  int type;

  /* Basic */
  double num;
  char *err;
  char *sym;

  /* Function */
  lbuiltin builtin;
  lenv *env;
  lval *formals;
  lval *body;

  /* Expression */
  int count;
  struct lval **cell;
};

/* Declare New lenv Struct */
struct lenv
{
  lenv *par;
  int count;
  char **syms;
  lval **vals;
};

/* Create lenv structure */
lenv *lenv_new(void)
{
  lenv *e = malloc(sizeof(lenv));
  e->par = NULL;
  e->count = 0;
  e->syms = NULL;
  e->vals = NULL;
  return e;
}

void lval_del(lval *v);

/* Delete lenv structure */
void lenv_del(lenv *e)
{
  for (int i = 0; i < e->count; i++)
  {
    free(e->syms[i]);
    lval_del(e->vals[i]);
  }

  free(e->syms);
  free(e->vals);
  free(e);
}

lval *lval_err(char *fmt, ...);
lval *lval_copy(lval *v);

lval *lenv_get(lenv *e, lval *k)
{
  for (int i = 0; i < e->count; i++)
  {
    if (strcmp(e->syms[i], k->sym) == 0)
    {
      return lval_copy(e->vals[i]);
    }
  }

  /* If no symbol check in parent otherwise error */
  if (e->par)
  {
    return lenv_get(e->par, k);
  }
  else
  {
    return lval_err("Unbound Symbol '%s'", k->sym);
  }
}

lenv *lenv_copy(lenv *e)
{
  lenv *n = malloc(sizeof(lenv));
  n->par = e->par;
  n->count = e->count;
  n->syms = malloc(sizeof(char *) * n->count);
  n->vals = malloc(sizeof(lval *) * n->count);
  for (int i = 0; i < e->count; i++)
  {
    n->syms[i] = malloc(strlen(e->syms[i]) + 1);
    strcpy(n->syms[i], e->syms[i]);
    n->vals[i] = lval_copy(e->vals[i]);
  }
  return n;
}

/* Add new variable to the environment */
void lenv_put(lenv *e, lval *k, lval *v)
{
  /* Iterate over all items in environment */
  /* This is to see if variable already exits */
  for (int i = 0; i < e->count; i++)
  {
    /* If variable is found delete item at that position */
    /* And replace with variable supplied by user */
    if (strcmp(e->syms[i], k->sym) == 0)
    {
      lval_del(e->vals[i]);
      e->vals[i] = lval_copy(v);
      return;
    }
  }

  /* If no existing entry found allocate space for new entry */
  e->count++;
  e->vals = realloc(e->vals, sizeof(lval *) * e->count);
  e->syms = realloc(e->syms, sizeof(char *) * e->count);

  /* Copy contents of lval and symbol string into new location */
  e->vals[e->count - 1] = lval_copy(v);
  e->syms[e->count - 1] = malloc(strlen(k->sym) + 1);
  strcpy(e->syms[e->count - 1], k->sym);
}

void lenv_def(lenv *e, lval *k, lval *v)
{
  /* Iterate till e has no parent */
  while (e->par)
    e = e->par;
  /* Put value in e */
  lenv_put(e, k, v);
}

char *ltype_name(int t)
{
  switch (t)
  {
  case LVAL_FUN:
    return "Function";
  case LVAL_NUM:
    return "Number";
  case LVAL_ERR:
    return "Error";
  case LVAL_SYM:
    return "Symbol";
  case LVAL_SEXPR:
    return "S-Expression";
  case LVAL_QEXPR:
    return "Q-Expression";
  default:
    return "Unkown";
  }
}

lval *lval_lambda(lval *formals, lval *body)
{
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_FUN;

  /* Set Builtin to Null */
  v->builtin = NULL;

  /* Build new environment */
  v->env = lenv_new();

  /* Set Formals and Body */
  v->formals = formals;
  v->body = body;
  return v;
}

/* Create a pointer to a new number lval */
lval *lval_num(double x)
{
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = x;
  return v;
}

/* Create a pointer to a new error lval */
lval *lval_err(char *fmt, ...)
{
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_ERR;

  /* Create a va list and initialize it */
  va_list va;
  va_start(va, fmt);

  /* Allocate 512 bytes of space */
  v->err = malloc(512);

  /* printf the error string with a maximum of 511 characters */
  vsnprintf(v->err, 511, fmt, va);

  /* Reallocate to number of bytes actually used */
  v->err = realloc(v->err, strlen(v->err) + 1);

  /* Cleanup our va list */
  va_end(va);

  return v;
}

/* Create a pointer to a new symbol lval */
lval *lval_sym(char *s)
{
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_SYM;
  v->sym = malloc(strlen(s) + 1);
  strcpy(v->sym, s);
  return v;
}

/* A pointer to a new empty sexpr lval */
lval *lval_sexpr(void)
{
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

/* A pointer to a new empty Qexpr lval */
lval *lval_qexpr(void)
{
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_QEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

/* A pointer to a new empty lval_fun type */
lval *lval_fun(lbuiltin func)
{
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_FUN;
  v->builtin = func;
  return v;
}

/* Destructor for lval types */
void lval_del(lval *v)
{
  switch (v->type)
  {
  case LVAL_NUM:
    break;

  case LVAL_ERR:
    free(v->err);
    break;
  case LVAL_SYM:
    free(v->sym);
    break;

  case LVAL_QEXPR:
  case LVAL_SEXPR:
    for (int i = 0; i < v->count; i++)
    {
      lval_del(v->cell[i]);
    }
    free(v->cell);
    break;

  case LVAL_FUN:
    if (!v->builtin)
    {
      lenv_del(v->env);
      lval_del(v->formals);
      lval_del(v->body);
    }
    break;
  }

  free(v);
}

lval *lval_copy(lval *v)
{
  lval *x = malloc(sizeof(lval));
  x->type = v->type;

  switch (v->type)
  {
  /* Copy Functions and Numbers Directly */
  case LVAL_FUN:
    if (v->builtin)
    {
      x->builtin = v->builtin;
    }
    else
    {
      x->builtin = NULL;
      x->env = lenv_copy(v->env);
      x->formals = lval_copy(v->formals);
      x->body = lval_copy(v->body);
    }
    break;
  case LVAL_NUM:
    x->num = v->num;
    break;

  /* Copy Strings using malloc and strcpy */
  case LVAL_ERR:
    x->err = malloc(strlen(v->err) + 1);
    strcpy(x->err, v->err);
    break;

  case LVAL_SYM:
    x->sym = malloc(strlen(v->sym) + 1);
    strcpy(x->sym, v->sym);
    break;

  /* Copy Lists by copying each sub-expression */
  case LVAL_SEXPR:
  case LVAL_QEXPR:
    x->count = v->count;
    x->cell = malloc(sizeof(lval *) * x->count);
    for (int i = 0; i < x->count; i++)
    {
      x->cell[i] = lval_copy(v->cell[i]);
    }
    break;
  }

  return x;
}

/* Read lval number */
lval *lval_read_num(mpc_ast_t *t)
{
  errno = 0;
  double x = strtof(t->contents, NULL);
  return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}

lval *lval_add(lval *v, lval *x)
{
  v->count++;
  v->cell = realloc(v->cell, sizeof(lval *) * v->count);
  v->cell[v->count - 1] = x;
  return v;
}

lval *lval_read(mpc_ast_t *t)
{
  /* If Symbol or Number return conversion to that type */
  if (strstr(t->tag, "number"))
  {
    return lval_read_num(t);
  }
  if (strstr(t->tag, "symbol"))
  {
    return lval_sym(t->contents);
  }

  /* If root (>) or sexpr then create empty list */
  lval *x = NULL;
  if (strcmp(t->tag, ">") == 0)
  {
    x = lval_sexpr();
  }
  if (strstr(t->tag, "sexpr"))
  {
    x = lval_sexpr();
  }
  if (strstr(t->tag, "qexpr"))
  {
    x = lval_qexpr();
  }

  /* Fill this list with any valid expression contained within */
  for (int i = 0; i < t->children_num; i++)
  {
    if (strcmp(t->children[i]->contents, "(") == 0)
    {
      continue;
    }
    if (strcmp(t->children[i]->contents, ")") == 0)
    {
      continue;
    }
    if (strcmp(t->children[i]->contents, "}") == 0)
    {
      continue;
    }
    if (strcmp(t->children[i]->contents, "{") == 0)
    {
      continue;
    }
    if (strcmp(t->children[i]->tag, "regex") == 0)
    {
      continue;
    }
    x = lval_add(x, lval_read(t->children[i]));
  }

  return x;
}

/* Forward declaration of lval_print function */
void lval_print(lval *v);

/* Print S-Expression */
void lval_expr_print(lval *v, char open, char close)
{
  putchar(open);
  for (int i = 0; i < v->count; i++)
  {
    lval_print(v->cell[i]);
    if (i != (v->count - 1))
    {
      putchar(' ');
    }
  }
  putchar(close);
}

/* Print an "lval" */
void lval_print(lval *v)
{
  switch (v->type)
  {
  case LVAL_NUM:
    printf("%f", v->num);
    break;
  case LVAL_ERR:
    printf("Error: %s", v->err);
    break;
  case LVAL_SYM:
    printf("%s", v->sym);
    break;
  case LVAL_SEXPR:
    lval_expr_print(v, '(', ')');
    break;
  case LVAL_QEXPR:
    lval_expr_print(v, '{', '}');
    break;
  case LVAL_FUN:
    if (v->builtin)
    {
      printf("<function>");
    }
    else
    {
      printf("(\\ ");
      lval_print(v->formals);
      putchar(' ');
      lval_print(v->body);
      putchar(')');
    }
    break;
  }
}

/* Print an "lval" followed by a newline */
void lval_println(lval *v)
{
  lval_print(v);
  putchar('\n');
}

lval *lval_pop(lval *v, int i)
{
  /* Find the item at "i" */
  lval *x = v->cell[i];

  /* Shift memory after the item at "i" over the top */
  memmove(&v->cell[i], &v->cell[i + 1], sizeof(lval *) * (v->count - i - 1));

  /* Decrease the ccount of items in the list */
  v->count--;

  /* Reallocate the memory used */
  v->cell = realloc(v->cell, sizeof(lval *) * v->count);

  return x;
}

lval *lval_take(lval *v, int i)
{
  lval *x = lval_pop(v, i);
  lval_del(v);
  return x;
}

lval *builtin_lambda(lenv *e, lval *a)
{
  /* Check Two arguments, each of which are Q-Expressions */
  LASSERT_COUNT(a, "\\", 2);
  LASSERT_TYPE(a, "\\", 0, LVAL_QEXPR);
  LASSERT_TYPE(a, "\\", 1, LVAL_QEXPR);

  /* Check first Q-Expression contains only Symbols */
  for (int i = 0; i < a->cell[0]->count; i++)
  {
    LASSERT(a, (a->cell[0]->cell[i]->type == LVAL_SYM),
            "Cannot define non-symbol. Got %s, Expected %s.",
            ltype_name(a->cell[0]->cell[i]->type), ltype_name(LVAL_SYM));
  }

  /* Pop first two arguments and pass them to lval_lambda */
  lval *formals = lval_pop(a, 0);
  lval *body = lval_pop(a, 0);
  lval_del(a);

  return lval_lambda(formals, body);
}

lval *builtin_head(lenv *e, lval *a)
{
  /* Check Error Conditions */
  LASSERT(a, a->count == 1,
          "Function 'head' passed too many arguments. Got %i, Expected %i",
          a->count, 1);
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
          "Function 'head' passed incorrect type for argument 0. Got %s, Expected %s.",
          ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));
  LASSERT(a, a->cell[0]->count != 0, "Function 'head' passed {}!");

  /* Otherwise take first argument */
  lval *v = lval_take(a, 0);

  /* Delete all elements that are not head and return */
  while (v->count > 1)
  {
    lval_del(lval_pop(v, 1));
  }
  return v;
}

/* Takes a value and a Q-Expression and appends it to the front */
lval *builtin_cons(lenv *e, double x, lval *v)
{
  /* Check Error Conditions */
  LASSERT(v, v->cell[0]->type == LVAL_QEXPR,
          "Function 'cons' passed incorrect type for argument 0. Got %s, Expected %s.",
          ltype_name(v->cell[0]->type), ltype_name(LVAL_QEXPR));

  /* I am not sure if it is exactly what the author meant but maybe? */
  v = lval_add(v, lval_num(x));
  return v;
}

/* Return number of elements in a Q-Expression */
int builtin_len(lenv *e, lval *v)
{
  return v->count;
}

/* Return all of a Q-Expression except the final element */
lval *builtin_init(lenv *e, lval *v)
{
  /* Check Error Conditions */
  LASSERT(v, v->cell[0]->type == LVAL_QEXPR,
          "Function 'init' passed incorrect type for argument 0. Got %s, Expected %s.",
          ltype_name(v->cell[0]->type), ltype_name(LVAL_QEXPR));
  LASSERT(v, v->count > 0, "Function 'init' passed {}!");

  lval_pop(v, v->count - 1);

  return v;
}

/* Forward declaration of lval_eval */
lval *lval_eval(lenv *e, lval *v);

lval *builtin_tail(lenv *e, lval *a)
{
  /* Check Error Conditions */
  LASSERT(a, a->count == 1,
          "Function 'tail' passed too many arguments. Got %i, Expected %i",
          a->count, 1);
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
          "Function 'tail' passed incorrect type for argument 0. Got %s, Expected %s.",
          ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));

  LASSERT(a, a->cell[0]->count != 0, "Function 'tail' passed {}!");

  /* Take first argument */
  lval *v = lval_take(a, 0);

  /* Delete first element and return */
  lval_del(lval_pop(v, 0));
  return v;
}

lval *builtin_list(lenv *e, lval *a)
{
  a->type = LVAL_QEXPR;
  return a;
}

lval *builtin_eval(lenv *e, lval *a)
{
  LASSERT(a, a->count == 1,
          "Function 'eval' passed too many arguments. Got %i, Expected %i",
          a->count, 1);
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
          "Function 'eval' passed incorrect type for argument 0. Got %s, Expected %s.",
          ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));

  lval *x = lval_take(a, 0);
  x->type = LVAL_SEXPR;
  return lval_eval(e, x);
}

lval* builtin_ord(lenv *e, lval *a, char *op)
{
  LASSERT_COUNT(a, op, 2);
  LASSERT_TYPE(a, op, 0, LVAL_NUM);
  LASSERT_TYPE(a, op, 1, LVAL_NUM);

  int r;
  if (strcmp(op, ">") == 0)
  {
    r = (a->cell[0]->num > a->cell[1]->num);
  }
  if (strcmp(op, "<") == 0)
  {
    r = (a->cell[0]->num < a->cell[1]->num);
  }
  if (strcmp(op, ">=") == 0)
  {
    r = (a->cell[0]->num >= a->cell[1]->num);
  }
   if (strcmp(op, "<=") == 0)
  {
    r = (a->cell[0]->num <= a->cell[1]->num);
  }
  lval_del(a);
  return lval_num(r);
}

int lval_eq(lval *x, lval *y)
{
  /* Different Types are always unequal */
  if (x->type != y->type) { return 0; }

  /* Compare Based upon type */
  switch(x->type)
  {
    /* Compare Number Value */
    case LVAL_NUM: return(x->num == y->num);

    /* Compare String Values */
    case LVAL_ERR: return(strcmp(x->err, y->err) == 0);
    case LVAL_SYM: return(strcmp(x->sym, y->sym) == 0);

    /* If builtin compare, otherwise compare formals and body */
    case LVAL_FUN:
                   if(x->builtin || y->builtin) {
                     return x->builtin == y->builtin;
                   } else {
                     return lval_eq(x->formals, y->formals)
                       && lval_eq(x->body, y->body);
                   }

    /* If list compare every indivdual element */
    case LVAL_QEXPR:
    case LVAL_SEXPR:
                   if (x->count != y->count) { return 0; }
                   for(int i = 0; i < x->count; i++)
                   {
                     /* If any element not equal then whole list not equal */
                     if (!lval_eq(x->cell[i], y->cell[i])) { return 0; }
                   }
                   /* Otherwise lists must be equal */
                   return 1;
                   break;
  }
  return 0;
}

lval* builtin_cmp(lenv *e, lval *a, char *op)
{
  LASSERT_COUNT(a, op, 2);
  int r;
  if (strcmp(op, "==") == 0)
  {
    r = lval_eq(a->cell[0], a->cell[1]);
  }
  if (strcmp(op, "!=") == 0)
  {
    r = !lval_eq(a->cell[0], a->cell[1]);
  }
  lval_del(a);
  return lval_num(r);
}

lval* builtin_eq(lenv *e, lval *a)
{
  return builtin_cmp(e, a, "==");
}

lval *builtin_ne(lenv *e, lval *a)
{
  return builtin_cmp(e, a, "!=");
}

lval* builtin_gt(lenv *e, lval *a)
{
  return builtin_ord(e, a, ">");
}

lval* builtin_lt(lenv *e, lval *a)
{
  return builtin_ord(e, a, "<");
}

lval* builtin_ge(lenv *e, lval *a)
{
  return builtin_ord(e, a, ">=");
}

lval* builtin_le(lenv *e, lval *a)
{
  return builtin_ord(e, a, "<=");
}

lval* builtin_if(lenv *e, lval *a)
{
  LASSERT_COUNT(a, "if", 3);
  LASSERT_TYPE(a, "if", 0, LVAL_NUM);
  LASSERT_TYPE(a, "if", 1, LVAL_QEXPR);
  LASSERT_TYPE(a, "if", 2, LVAL_QEXPR);

  /* Mark Both Expressions as evaluable */
  lval *x;
  a->cell[1]->type = LVAL_SEXPR;
  a->cell[2]->type = LVAL_SEXPR;

  if(a->cell[0]->num)
  {
    /* If condition is true evaluate first expression */
    x = lval_eval(e, lval_pop(a, 1));
  } else {
    /* Otherwise evaluate second expression */
    x = lval_eval(e, lval_pop(a, 2));
  }

  /* Delete argument list and return */
  lval_del(a);
  return x;
}

lval *lval_join(lval *x, lval *y)
{
  /* For each cell in 'y' add it to 'x' */
  while (y->count)
  {
    x = lval_add(x, lval_pop(y, 0));
  }

  /* Delete the empty 'y' and return 'x' */
  lval_del(y);
  return x;
}

lval* lval_call(lenv *e, lval* f, lval *a)
{
  /* If builtin then simply call that */
  if (f->builtin) { return f->builtin(e, a); }

  /* Record Argument Counts */
  int given = a->count;
  int total = f->formals->count;

  /* While arguments still remain to be processed */
  while (a->count)
  {
    /* If we've ran out of formal arguments to bind */
    if (f->formals->count == 0)
    {
      lval_del(a);
      return lval_err("Function pssed too many arguments. Got %i, Expected %i",
          given, total);
    }

    /* Pop the first symbol from the formals */
    lval* sym = lval_pop(f->formals, 0);

    /* Special Case to deal with '&' */
    if(strcmp(sym->sym, "&") == 0)
    {
      /* Ensure '&' is followed by another symbol */
      if(f->formals->count != 1)
      {
        lval_del(a);
        return lval_err(
            "Function format invalid. Symbol '&' not followed by single symbol.");
      }

      /* Next formal should be bound to remaining arguments */
      lval *nsym = lval_pop(f->formals, 0);
      lenv_put(f->env, nsym, builtin_list(e, a));
      lval_del(sym);
      lval_del(nsym);
      break;
    }

    /* Pop the next argument from the list */
    lval* val = lval_pop(a, 0);

    /* Bind a copy into the function's environment */
    lenv_put(f->env, sym, val);

    /* Delete symbol and value */
    lval_del(sym);
    lval_del(val);
  }

  /* Argument list is now bound so can be cleaned up */
  lval_del(a);

  /* If '&' remains in formal list bind to empty list */
  if(f->formals->count > 0 && strcmp(f->formals->cell[0]->sym, "&") == 0)
  {
    /* Check to ensure that & is not passed invalidly */
    if (f->formals->count != 2)
    {
      return lval_err(
          "Function format invalid. Symbol '&' not followed by single symbol.");
    }

    /* Pop and delete '&' symbol */
    lval_del(lval_pop(f->formals, 0));

    /* Pop next symbol and create empty list */
    lval *sym = lval_pop(f->formals, 0);
    lval *val = lval_qexpr();

    /* Bind to environment and delete */
    lenv_put(f->env, sym, val);
    lval_del(sym);
    lval_del(val);
  }

  if(f->formals->count == 0)
  {
    /* Set the parent environment */
    f->env->par = e;

    /* Evaluate and return */
    return builtin_eval(f->env, lval_add(lval_sexpr(), lval_copy(f->body)));
  } else {
    /* Otherwise return partially evaluated faunction */
    return lval_copy(f);
  }
}

lval *builtin_join(lenv *e, lval *a)
{
  for (int i = 0; i < a->count; i++)
  {
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
            "Function 'join' passed incorrect type for argument 0. Got %s, Expected %s.",
            ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));
  }

  lval *x = lval_pop(a, 0);

  while (a->count)
  {
    x = lval_join(x, lval_pop(a, 0));
  }

  lval_del(a);
  return x;
}

lval *builtin_op(lenv *e, lval *a, char *op)
{
  /* Ensure all arguments are numbers */
  for (int i = 0; i < a->count; i++)
  {
    LASSERT(a, a->cell[i]->type == LVAL_NUM,
            "Function '%s' passed incorrect type for argument 1. Got %s, Expected %s.",
            op, ltype_name(a->cell[i]->type), ltype_name(LVAL_NUM));
  }

  /* Pop the first element */
  lval *x = lval_pop(a, 0);

  /* If no arguments and sub the perform unary negation */
  if ((strcmp(op, "-") == 0) && a->count == 0)
  {
    x->num = -x->num;
  }

  /* While there are still elements remaining */
  while (a->count > 0)
  {
    /* Pop the next element */
    lval *y = lval_pop(a, 0);

    if (strcmp(op, "+") == 0)
    {
      x->num += y->num;
    }
    if (strcmp(op, "-") == 0)
    {
      x->num -= y->num;
    }
    if (strcmp(op, "*") == 0)
    {
      x->num *= y->num;
    }
    if (strcmp(op, "/") == 0)
    {
      if (y->num == 0)
      {
        lval_del(x);
        lval_del(y);
        x = lval_err("Division By Zero!");
        break;
      }
      x->num /= y->num;
    }
    if (strcmp(op, "%") == 0)
    {
      x->num = fmod(x->num, y->num);
    }
    if (strcmp(op, "add") == 0)
    {
      x->num += y->num;
    }
    if (strcmp(op, "sub") == 0)
    {
      x->num -= y->num;
    }
    if (strcmp(op, "mult") == 0)
    {
      x->num *= y->num;
    }
    if (strcmp(op, "div") == 0)
    {
      if (y->num == 0)
      {
        lval_del(x);
        lval_del(y);
        x = lval_err("Division By Zero!");
        break;
      }
      x->num /= y->num;
    }
    if (strcmp(op, "mod") == 0)
    {
      x->num = fmod(x->num, y->num);
    }
    if (strcmp(op, "max") == 0)
    {
      x->num = x->num > y->num ? x->num : y->num;
    }
    if (strcmp(op, "min") == 0)
    {
      x->num = x->num < y->num ? x->num : y->num;
    }
    if (strcmp(op, "pow") == 0)
    {
      x->num = pow(x->num, y->num);
    }
    lval_del(y);
  }

  lval_del(a);
  return x;
}

lval *builtin_add(lenv *e, lval *a)
{
  return builtin_op(e, a, "+");
}

lval *builtin_sub(lenv *e, lval *a)
{
  return builtin_op(e, a, "-");
}

lval *builtin_mul(lenv *e, lval *a)
{
  return builtin_op(e, a, "*");
}

lval *builtin_div(lenv *e, lval *a)
{
  return builtin_op(e, a, "/");
}

lval *builtin(lenv *e, lval *a, char *func)
{
  if (strcmp("list", func) == 0)
  {
    return builtin_list(e, a);
  }
  if (strcmp("head", func) == 0)
  {
    return builtin_head(e, a);
  }
  if (strcmp("tail", func) == 0)
  {
    return builtin_tail(e, a);
  }
  if (strcmp("join", func) == 0)
  {
    return builtin_join(e, a);
  }
  if (strcmp("eval", func) == 0)
  {
    return builtin_eval(e, a);
  }
  if (strstr("+-/*%", func))
  {
    return builtin_op(e, a, func);
  }
  if (strcmp("add", func) == 0)
  {
    return builtin_op(e, a, func);
  }
  if (strcmp("sub", func) == 0)
  {
    return builtin_op(e, a, func);
  }
  if (strcmp("mult", func) == 0)
  {
    return builtin_op(e, a, func);
  }
  if (strcmp("div", func) == 0)
  {
    return builtin_op(e, a, func);
  }
  if (strcmp("mod", func) == 0)
  {
    return builtin_op(e, a, func);
  }
  if (strcmp("pow", func) == 0)
  {
    return builtin_op(e, a, func);
  }
  if (strcmp("min", func) == 0)
  {
    return builtin_op(e, a, func);
  }
  if (strcmp("max", func) == 0)
  {
    return builtin_op(e, a, func);
  }
  lval_del(a);
  return lval_err("Unkown Function!");
}

lval *lval_eval_sexpr(lenv *e, lval *v)
{
  /* Evaluate Children */
  for (int i = 0; i < v->count; i++)
  {
    v->cell[i] = lval_eval(e, v->cell[i]);
  }

  /* Error Checking */
  for (int i = 0; i < v->count; i++)
  {
    if (v->cell[i]->type == LVAL_ERR)
    {
      return lval_take(v, i);
    }
  }

  /* Empty Expression */
  if (v->count == 0)
  {
    return v;
  }

  /* Single Expression*/
  if (v->count == 1)
  {
    return lval_take(v, 0);
  }

  /* Ensure First Element is a function after evaluation */
  lval *f = lval_pop(v, 0);
  if (f->type != LVAL_FUN)
  {
    lval *err = lval_err("S-Expression starts with incorrect type. Got %s, Expected %s.",
        ltype_name(f->type), ltype_name(LVAL_FUN));
    lval_del(f);
    lval_del(v);
    return err;
  }

  /* If so call function to get result */
  lval *result = lval_call(e, f, v);
  lval_del(f);
  return result;
}

lval *lval_eval(lenv *e, lval *v)
{
  if (v->type == LVAL_SYM)
  {
    lval *x = lenv_get(e, v);
    lval_del(v);
    return x;
  }

  if (v->type == LVAL_SEXPR)
  {
    return lval_eval_sexpr(e, v);
  }

  return v;
}

/* Register builtins into environment */
void lenv_add_builtin(lenv *e, char *name, lbuiltin func)
{
  lval *k = lval_sym(name);
  lval *v = lval_fun(func);
  lenv_put(e, k, v);
  lval_del(k);
  lval_del(v);
}

lval *builtin_var(lenv *e, lval *a, char *func)
{
  LASSERT_TYPE(a, func, 0, LVAL_QEXPR);

  /* First argument is symbol list */
  lval *syms = a->cell[0];

  /* Ensure all elements of first list are symbols */
  for (int i = 0; i < syms->count; i++)
  {
    LASSERT(a, syms->cell[i]->type == LVAL_SYM,
            "Function '%s' cannot define non-symbol. Got %s, Expected %s",
            func, ltype_name(syms->cell[i]->type), ltype_name(LVAL_SYM));
  }

  /* Check correct number of symbols and values */
  LASSERT(a, syms->count == a->count - 1,
          "Function '%s' passed too many arguments for symbols. Got %i, Expected %i",
          func, ltype_name(syms->count), a->count - 1);

  /* Assign copies of values to symbols */
  for (int i = 0; i < syms->count; i++)
  {
    /* If 'def' define in globally. If 'put' define in locally. */
    if (strcmp(func, "def") == 0)
      lenv_def(e, syms->cell[i], a->cell[i + 1]);
    if (strcmp(func, "put") == 0)
      lenv_put(e, syms->cell[i], a->cell[i + 1]);
  }

  lval_del(a);
  return lval_sexpr();
}

/* Define variable */
lval *builtin_def(lenv *e, lval *a)
{
  return builtin_var(e, a, "def");
}

lval *builtin_put(lenv *e, lval *a)
{
  return builtin_var(e, a, "put");
}

void lenv_add_builtins(lenv *e)
{
  /* List Functions */
  lenv_add_builtin(e, "list", builtin_list);
  lenv_add_builtin(e, "head", builtin_head);
  lenv_add_builtin(e, "tail", builtin_tail);
  lenv_add_builtin(e, "eval", builtin_eval);
  lenv_add_builtin(e, "join", builtin_join);

  /* Variable Functions */
  lenv_add_builtin(e, "def", builtin_def);
  lenv_add_builtin(e, "=", builtin_put);
  lenv_add_builtin(e, "\\", builtin_lambda);

  /* Comparison Functions */
  lenv_add_builtin(e, "if", builtin_if);
  lenv_add_builtin(e, "==", builtin_eq);
  lenv_add_builtin(e, "!=", builtin_ne);
  lenv_add_builtin(e, ">", builtin_gt);
  lenv_add_builtin(e, "<", builtin_lt);
  lenv_add_builtin(e, ">=", builtin_ge);
  lenv_add_builtin(e, "<=", builtin_le);

  /* Mathematical Functions */
  lenv_add_builtin(e, "+", builtin_add);
  lenv_add_builtin(e, "-", builtin_sub);
  lenv_add_builtin(e, "*", builtin_mul);
  lenv_add_builtin(e, "/", builtin_div);
}

int main(int argc, char **argv)
{
  /*Create Some Parsers */
  mpc_parser_t *Number = mpc_new("number");
  mpc_parser_t *Symbol = mpc_new("symbol");
  mpc_parser_t *Sexpr = mpc_new("sexpr");
  mpc_parser_t *Qexpr = mpc_new("qexpr");
  mpc_parser_t *Expr = mpc_new("expr");
  mpc_parser_t *Lispy = mpc_new("lispy");

  /* Define them with the following Language */
  mpca_lang(MPCA_LANG_DEFAULT,
            "                                                         \
      number : /-?[0-9]+[.]*[0-9]*/ ;                                 \
      symbol : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ;                     \
      sexpr  : '(' <expr>* ')' ;                                      \
      qexpr  : '{' <expr>* '}' ;                                      \
      expr   : <number> | <symbol> | <sexpr> | <qexpr> ;              \
      lispy  : /^/ <expr>* /$/ ;                                      \
      ",
            Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

  puts("Lispy version 0.0.0.0.7");
  puts("Press Ctrl+c to Exit\n");

  lenv *e = lenv_new();
  lenv_add_builtins(e);

  while (1)
  {
    char *input = readline("lispy :> ");
    add_history(input);

    /* Attempt to Parse the user Input */
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r))
    {
      lval *x = lval_eval(e, lval_read(r.output));
      lval_println(x);
      lval_del(x);

      mpc_ast_delete(r.output);
    }
    else
    {
      /* Print the Error */
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    free(input);
  }

  lenv_del(e);

  /* Undefine and Delete our Parsers */
  mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

  return 0;
}