#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <error.h>
#include <stdarg.h>
#include <editline/readline.h>
#include <mpc.h>

char *strdup(const char *s);

struct lenv;
struct lval;

typedef struct lenv lenv;
typedef struct lval lval;
typedef lval* (*lbuildin)(lenv*, lval*);

struct lenv {
    int count;
    char** syms;
    lval** vals;
};

struct lval {
    int type;
    //for Number
    long num;
    //for Error
    char* err;
    //for Symbol
    char* sym;
    //for Symbol
    lbuildin fun;

    //for S-expr
    int count;
    lval** cell;
};

enum {LVAL_ERR, LVAL_NUM, LVAL_SYM, LVAL_FUN, LVAL_SEXPR, LVAL_QEXPR};

char* ltype_name(int type)
{
#define LVAL_TPYE(type)  \
    case type: return #type;

    switch (type)
    {
    LVAL_TPYE(LVAL_ERR);
    LVAL_TPYE(LVAL_NUM);
    LVAL_TPYE(LVAL_SYM);
    LVAL_TPYE(LVAL_FUN);
    LVAL_TPYE(LVAL_SEXPR);
    LVAL_TPYE(LVAL_QEXPR);
    default: return "Unknown";
    }

}

lval* lval_err(char* fmt, ...)
{
    lval *x = malloc(sizeof(lval));
    x->type = LVAL_ERR;
    x->err = malloc(512);

    va_list va;
    va_start(va, fmt);
    vsnprintf(x->err, 511, fmt, va);
    va_end(va);

    x->err = realloc(x->err, strlen(x->err)+1);

    return x;
}

lval* lval_num(long n)
{
    lval* x = malloc(sizeof(lval));
    x->type = LVAL_NUM;
    x->num = n;
    return x;
}

lval* lval_sym(char* str)
{
    lval* x = malloc(sizeof(lval));
    x->type = LVAL_SYM;
    x->sym = malloc(strlen(str)+1);
    strcpy(x->sym, str);
    return x;
}

lval* lval_fun(lbuildin func)
{
    lval* x = malloc(sizeof(lval));
    x->type = LVAL_FUN;
    x->fun = func;
    return x;
}

lval* lval_expr(int type)
{
    lval* x = malloc(sizeof(lval));
    x->type = type;
    x->count = 0;
    x->cell = NULL;
    return x;
}

lval* lval_sexpr(void)
{
    return lval_expr(LVAL_SEXPR);
}

lval* lval_qexpr(void)
{
    return lval_expr(LVAL_QEXPR);
}

void lval_del(lval* v)
{
    switch (v->type)
    {
    case LVAL_ERR: free(v->err); break;
    case LVAL_NUM: break;
    case LVAL_SYM: free(v->sym); break;
    case LVAL_FUN: break;
    case LVAL_QEXPR:
    case LVAL_SEXPR:
       for (int i=0; i<v->count; i++)
       {
           lval_del(v->cell[i]);
       }
       break;
    }

    free(v);
}

lval* lval_add(lval* v, lval* x)
{
    v->count++;
    v->cell = realloc(v->cell, v->count * sizeof(lval));
    v->cell[v->count-1] = x;
    return v;
}

//V is an S-Expr or Q-Expr
//this version is really effectiveless
lval* lval_copy(lval* v)
{
    lval* x = NULL;
    switch (v->type)
    {
    case LVAL_NUM: x = lval_num(v->num); break;
    case LVAL_ERR:
       x = malloc(sizeof(lval));
       x->type = LVAL_ERR;
       x->err = strdup(v->err);
       break;
    case LVAL_SYM: x = lval_sym(v->sym); break;
    case LVAL_FUN: x = lval_fun(v->fun); break;
    case LVAL_QEXPR:
    case LVAL_SEXPR:
       x = lval_expr(v->type);
       for (int i=0; i<v->count; i++)
       {
           lval_add(x, lval_copy(v->cell[i]));
       }
       break;
    }

    return x;
}

lval* lval_clone(lval* v, int i)
{
    return lval_copy(v->cell[i]);
}

lenv* lenv_new(void)
{
    lenv* e = malloc(sizeof(lenv));
    e->count = 0;
    e->syms = NULL;
    e->vals = NULL;
    return e;
}

void lenv_del(lenv* e)
{
    for (int i=0; i<e->count; i++)
    {
        free(e->syms[i]);
        lval_del(e->vals[i]);
    }

    free(e->syms);
    free(e->vals);
    free(e);
}

/* k is just the name of val, to find the value of the val in the env */
lval* lenv_get(lenv* e, lval* k)
{
    for (int i=0; i<e->count; i++)
        if (!strcmp(e->syms[i], k->sym))
            return lval_copy(e->vals[i]);

    return lval_err("unbounded symbol %s", k->sym);
}

/* k is just the name of val, v is the value of the val*/
void lenv_put(lenv* e, lval* k, lval* v)
{
    for (int i=0; i<e->count; i++)
    {
        if (!strcmp(e->syms[i], k->sym))
        {
            lval_del(e->vals[i]);
            e->vals[i] = lval_copy(v);
            return ;
        }
    }

    e->count++;
    e->syms = realloc(e->syms, sizeof(e->syms[0]) * e->count);
    e->vals = realloc(e->vals, sizeof(e->vals[0]) * e->count);
    e->syms[e->count-1] = strdup(k->sym);
    e->vals[e->count-1] = lval_copy(v);
}

lval* lval_read(mpc_ast_t* t)
{
    lval* x = NULL;
    if (strstr(t->tag, "number"))
    {
        errno = 0;
        int i = strtol(t->contents, NULL, 10);
        x = errno != ERANGE ? lval_num(i) : lval_err("invalid number!");
        return x;
    }
    else if (strstr(t->tag, "symbol"))
    {
        x = lval_sym(t->contents);
        return x;
    }
    else if (!strcmp(t->tag, ">") || strstr(t->tag, "sexpr"))
        x = lval_sexpr();
    else if (strstr(t->tag, "qexpr"))
        x = lval_qexpr();

    for (int i=0; i<t->children_num; i++)
    {
        if (!strcmp(t->children[i]->tag, "regex")) continue;
        if (!strcmp(t->children[i]->contents, "(")) continue;
        if (!strcmp(t->children[i]->contents, ")")) continue;
        if (!strcmp(t->children[i]->contents, "{")) continue;
        if (!strcmp(t->children[i]->contents, "}")) continue;

        x = lval_add(x, lval_read(t->children[i]));
    }

    return x;
}

lval* buildin_head(lenv *e, lval* v)
{
    if (v->count != 2)
        return lval_err("Function 'head' passed too many arguments, "
                        "get %d, expectedd %d", 1, v->count);
    if (v->cell[1]->type != LVAL_QEXPR)
        return lval_err("Function 'head' passed incorrect type, "
                        "get <%s>, expected<%s>",
                        ltype_name(v->cell[1]->type), ltype_name(LVAL_QEXPR));
    if (v->cell[1]->count == 0)
        return lval_err("Function 'head' passed {}!");

    lval* x = lval_expr(v->cell[1]->type);
    lval_add(x, lval_clone(v->cell[1], 0));
    return x;
}

lval* buildin_tail(lenv* e, lval* v)
{
    if (v->count != 2)
        return lval_err("Function 'tail' passed too many arguments, "
                        "get %d, expectedd %d", v->count, 1);
    if (v->cell[1]->type != LVAL_QEXPR)
        return lval_err("Function 'tail' passed incorrect type, "
                        "get <%s>, expected<%s>",
                        ltype_name(v->cell[1]->type), ltype_name(LVAL_QEXPR));
    if (v->cell[1]->count == 0)
        return lval_err("Function 'tail' passed {}!");

    lval* x = lval_expr(v->cell[1]->type);
    for (int i=1; i<v->cell[1]->count; i++)
        lval_add(x, lval_clone(v->cell[1], i));

    return x;
}

lval* buildin_list(lenv* e, lval* v)
{
    lval* x = lval_expr(LVAL_QEXPR);
    for (int i=1; i<v->count; i++)
        x = lval_add(x, lval_clone(v, i));
    return x;
}

//x y should be Q-Expr
lval* lval_join(lval* x, lval* y)
{
    for (int i=0; i<y->count; i++)
    {
        x = lval_add(x, lval_clone(y, i));
    }

    return x;
}

lval* buildin_join(lenv* e, lval* v)
{
    for (int i=1; i<v->count; i++)
    {
        if (v->cell[i]->type != LVAL_QEXPR)
            return lval_err("Function 'join' passed incorrect type!");
    }

    lval* x = lval_clone(v, 1);
    for (int i=2; i<v->count; i++)
    {
        lval_join(x, v->cell[i]);
    }

    return x;
}

lval* lval_eval(lenv* e, lval* v);
lval* buildin_eval(lenv* e, lval* v)
{
    if (v->count != 2)
        return lval_err("Function 'eval' passed too many arguments, "
                        "get %d, expectedd %d", 1, v->count);
    if (v->cell[1]->type != LVAL_QEXPR)
        return lval_err("Function 'eval' passed incorrect type, "
                        "get <%s>, expected<%s>",
                        ltype_name(v->cell[1]->type), ltype_name(LVAL_QEXPR));
//    if (v->cell[0]->count == 0)
//        return lval_err("Function 'eval' passed {}!");
    lval* x = lval_clone(v, 1);
    x->type = LVAL_SEXPR;

    return lval_eval(e, x);
}

lval* buildin_op(lval* v, const char* op)
{
    lval* x = NULL;

    for (int i=1; i<v->count; i++)
    {
        if (v->cell[i]->type != LVAL_NUM)
            return lval_err("Function '%s' passed incorrect type, "
                            "get <%s>, expected<%s>", op,
                            ltype_name(v->cell[i]->type), ltype_name(LVAL_NUM));
    }

    x = lval_num(v->cell[1]->num);
    for (int i=2; i<v->count; i++)
    {
        lval* y = v->cell[i];
        if (!strncmp(op, "+", 1))
            x->num += y->num;
        else if (!strncmp(op, "-", 1))
            x->num -= y->num;
        else if (!strncmp(op, "*", 1))
            x->num *= y->num;
        else if (!strncmp(op, "/", 1))
        {
            if (y->num == 0)
            {
                lval_del(x);
                x = lval_err("Division by zero!");
                break;
            }
            x->num /= y->num;
        }
    }

    return x;
}

lval* buildin_add(lenv* e, lval* v)
{
    return buildin_op(v, "+");
}
lval* buildin_sub(lenv* e, lval* v)
{
    return buildin_op(v, "-");
}
lval* buildin_mul(lenv* e, lval* v)
{
    return buildin_op(v, "*");
}
lval* buildin_div(lenv* e, lval* v)
{
    return buildin_op(v, "/");
}

/*
 * def {x} 1
 */
lval* buildin_def(lenv* e, lval* v)
{
    lval* syms = v->cell[1];

    if (syms->type != LVAL_QEXPR)
        return lval_err("Function 'def' passed incorrect type!");
    if (syms->count == 0 || syms->count != v->count-2)
        return lval_err("Function 'def' cannot define incorrect"
                        "number of values to symbols!");

    for (int i=0; i<syms->count; i++)
    {
        lenv_put(e, syms->cell[i], v->cell[2+i]);
    }

    return lval_sexpr();
}

void lenv_add_buildin(lenv* e, char* name, lbuildin func)
{
    lval* k = lval_sym(name);
    lval* f = lval_fun(func);
    lenv_put(e, k, f);
    lval_del(k);
    lval_del(f);
}

void lenv_add_buildins(lenv* e)
{
    lenv_add_buildin(e, "list", buildin_list);
    lenv_add_buildin(e, "head", buildin_head);
    lenv_add_buildin(e, "tail", buildin_tail);
    lenv_add_buildin(e, "join", buildin_join);
    lenv_add_buildin(e, "eval", buildin_eval);
    lenv_add_buildin(e, "def",  buildin_def);

    lenv_add_buildin(e, "+", buildin_add);
    lenv_add_buildin(e, "-", buildin_add);
    lenv_add_buildin(e, "*", buildin_add);
    lenv_add_buildin(e, "/", buildin_add);
}

void lval_print(lval *v);
void lval_expr_print(lval *v, char open, char close)
{
    putchar(open);
    for (int i=0; i <v->count; i++)
    {
        lval_print(v->cell[i]);
        if (i != v->count-1)
            putchar(' ');
    }
    putchar(close);
}

void lval_print(lval *v)
{
    switch (v->type)
    {
    case LVAL_ERR: printf("Error: %s", v->err); break;
    case LVAL_NUM: printf("%ld", v->num); break;
    case LVAL_SYM: printf("%s", v->sym); break;
    case LVAL_FUN: printf("<function: %p>", v->fun); break;
    case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
    case LVAL_QEXPR: lval_expr_print(v, '{', '}'); break;
    }
}

void lval_println(lval *v)
{
    lval_print(v);
    putchar('\n');
}

lval* lval_eval_sexpr(lenv* e, lval* v)
{
    lval* result = NULL;
    for (int i=0; i<v->count; i++)
    {
        v->cell[i] = lval_eval(e, v->cell[i]);
        //error checking
        if (v->cell[i]->type == LVAL_ERR)
        {
            result = lval_err(v->cell[i]->err);
            goto out;
        }
    }

    if (v->count == 0)
        return v;
    if (v->count == 1)
    {
        result = v->cell[0];
        free(v);
        return result;
    }

    //get the first child of S-expr, it should be a `symbol'
    lval* f = v->cell[0];
    if (f->type != LVAL_FUN)
    {
        result = lval_err("first emlempent is not a function!");
        goto out;
    }

    result = f->fun(e, v);

out:
    lval_del(v);
    return result;
}

lval* lval_eval(lenv* e, lval* v)
{
    if (v->type == LVAL_SYM)
        return lenv_get(e, v);
    else if (v->type == LVAL_SEXPR)
        return lval_eval_sexpr(e, v);

    return v;
}

int main(int argc, char* argv[])
{
    printf("version: 0.0.1\n");

    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Sexpr = mpc_new("sexpr");
    mpc_parser_t* Qexpr = mpc_new("qexpr");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lispy = mpc_new("lispy");

    mpca_lang(MPCA_LANG_DEFAULT,                                      \
        "                                                             \
            number   : /-?[0-9]+/ ;                                   \
            symbol   : /[a-zA-Z_0-9+\\-*\\/\\\\=<>!&]+/ ;             \
            sexpr    : '(' <expr>* ')' ;                              \
            qexpr    : '{' <expr>* '}' ;                              \
            expr     : <number> | <symbol> | <sexpr> | <qexpr> ;      \
            lispy    : /^/ <expr>* /$/ ;                              \
        ",                                                            \
        Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

    lenv *e = lenv_new();
    lenv_add_buildins(e);
    while (1)
    {
        char* line = readline("lispy> ");
        add_history(line);
        mpc_result_t r;
        if (mpc_parse("<stdin>", line, Lispy, &r))
        {
            mpc_ast_print(r.output);
            lval* x = lval_eval(e, lval_read(r.output));
            lval_println(x);
            lval_del(x);
            mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(line);
    }

    mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
    return 0;
}
