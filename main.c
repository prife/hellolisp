#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <error.h>
#include <editline/readline.h>
#include <mpc.h>

typedef struct lval {
    int type;
    //for Number
    long num;
    //for Error
    char* err;
    //for Symbol
    char* sym;
    //for S-expr
    int count;
    struct lval** cell;
} lval;

enum {LVAL_ERR, LVAL_NUM, LVAL_SYM, LVAL_SEXPR};
enum {LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

lval* lval_err(char* str)
{
    lval *x = malloc(sizeof(lval));
    x->type = LVAL_ERR;
    x->err = malloc(strlen(str)+1);
    strcpy(x->err, str);
    //x->err = strdup(str);
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

lval* lval_sexpr(void)
{
    lval* x = malloc(sizeof(lval));
    x->type = LVAL_SEXPR;
    x->count = 0;
    x->cell = NULL;
    return x;
}

void lval_del(lval* v)
{
    switch (v->type)
    {
    case LVAL_ERR: free(v->err); break;
    case LVAL_NUM: break;
    case LVAL_SYM: free(v->sym); break;
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

lval* lval_read(mpc_ast_t* t)
{
    lval* x = NULL;
    if (strstr(t->tag, "number"))
    {
        errno = 0;
        int i = strtol(t->contents, NULL, 10);
        x = errno != ERANGE ? lval_num(i) : lval_err("Error: invalid number!");
        return x;
    }
    else if (strstr(t->tag, "symbol"))
    {
        x = lval_sym(t->contents);
        return x;
    }
    else if (!strcmp(t->tag, ">") || strstr(t->tag, "sexpr"))
        x = lval_sexpr();

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

lval* buildin_op(lval* v, const char* op)
{
    lval* x = NULL;

    for (int i=1; i<v->count; i++)
    {
        if (v->cell[i]->type != LVAL_NUM)
            return lval_err("Cannot operate on non-number!");
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

void lval_print(lval *v, char open, char close)
{
    switch (v->type)
    {
    case LVAL_NUM:
        printf("%ld", v->num);
        break;
    case LVAL_SYM:
        printf("%s", v->sym);
        break;
    case LVAL_ERR:
        printf("%s", v->err);
        break;
    case LVAL_SEXPR:
        putchar(open);
        for (int i=0; i <v->count; i++)
        {
            lval_print(v->cell[i], open, close);
            if (i != v->count-1)
                putchar(' ');
        }
        putchar(close);
        break;
    }
}

void lval_println(lval *v)
{
    lval_print(v, '(', ')');
    putchar('\n');
}

lval* lval_eval(lval* v);
lval* lval_eval_sexpr(lval* v)
{
    lval* result = NULL;
    for (int i=0; i<v->count; i++)
    {
        v->cell[i] = lval_eval(v->cell[i]);
        //error checking
        //if (v->cell[i]->type == LVAL_ERR)
        //    return lval_take(v, i);
    }

    if (v->count == 0)
        return v;
    if (v->count == 1)
        return v;

    //get the first child of S-expr, it should be a `symbol'
    lval* f = v->cell[0];
    if (f->type != LVAL_SYM)
    {
        result = lval_err("S-Expression does not start with symbol!");
        goto out;
    }

    else if (v->count == 2)
    {
        result = lval_err("S-Expression does not support unary symbol!");
        goto out;
    }

    result = buildin_op(v, f->sym);

out:
    lval_del(v);
    return result;
}

lval* lval_eval(lval* v)
{
    if (v->type == LVAL_SEXPR)
        return lval_eval_sexpr(v);

    return v;
}

int main(int argc, char* argv[])
{
    printf("version: 0.0.1\n");

    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Sexpr = mpc_new("sexpr");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lispy = mpc_new("lispy");

    mpca_lang(MPCA_LANG_DEFAULT,                               \
        "                                                      \
            number   : /-?[0-9]+/ ;                            \
            symbol   : '+' | '-' | '*' | '/' ;                 \
            sexpr    : '(' <expr>* ')' ;                       \
            expr     : <number> | <symbol> | <sexpr>  ;        \
            lispy    : /^/ <expr>+ /$/ ;                       \
        ",                                                     \
        Number, Symbol, Sexpr, Expr, Lispy);

    while (1)
    {
        char* line = readline("lispy> ");
        add_history(line);
        mpc_result_t r;
        if (mpc_parse("<stdin>", line, Lispy, &r))
        {
            mpc_ast_print(r.output);
            //show(r.output);
            //printf("number of nodes: %d\n", number_of_nodes(r.output));
            //printf("number of leaves: %d\n", number_of_leaves(r.output));
            //lval x = eval(r.output);
            lval* x = lval_eval(lval_read(r.output));
            assert(x != NULL);
            lval_println(x);
            lval_del(x);
            mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(line);
    }

    mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Lispy);
    return 0;
}
