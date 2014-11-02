#include <stdio.h>
#include <stdlib.h>
#include <error.h>
#include <editline/readline.h>
#include <mpc.h>

typedef struct {
    int type;
    long num;
    int err;
} lval;

enum {LVAL_NUM, LVAL_ERR,};
enum {LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

lval lval_num(long n)
{
    lval x;
    x.type = LVAL_NUM;
    x.num = n;
    return x;
}

lval lval_err(int e)
{
    lval x;
    x.type = LVAL_ERR;
    x.err = e;
    return x;
}

lval eval_op(lval x, const char* op, lval y)
{
    lval res = lval_err(LERR_BAD_OP);

    if (!strncmp(op, "+", 1))
    {
        res = lval_num(x.num + y.num);
    }
    else if (!strncmp(op, "-", 1))
    {
        res = lval_num(x.num - y.num);
    }
    else if (!strncmp(op, "*", 1))
    {
        res = lval_num(x.num * y.num);
    }
    else if (!strncmp(op, "/", 1))
    {
        res = y.num == 0 ? lval_err(LERR_DIV_ZERO) : lval_num(x.num / y.num);
    }

    return res;
}

void lval_print(lval v)
{
    switch (v.type)
    {
    case LVAL_NUM:
        printf("%ld", v.num);
        break;
    case LVAL_ERR:
        {
            switch (v.err)
            {
            case LERR_BAD_OP:
                printf("Error: invlaid operation!");
                break;
            case LERR_BAD_NUM:
                printf("Error: invalid number!");
                break;
            case LERR_DIV_ZERO:
                printf("Error: division by zero!");
                break;
            }
        }
        break;
    }
}

void lval_println(lval v)
{
    lval_print(v);
    putchar('\n');
}

int number_of_nodes(mpc_ast_t *t)
{
    if (t->children_num == 0)
        return 1;

    int num = 1;
    for (int i=0; i<t->children_num; i++)
    {
        num += number_of_nodes(t->children[i]);
    }

    return num;
}

int number_of_leaves(mpc_ast_t *t)
{
    if (t->children_num == 0)
        return 1;

    int num = 0;
    for (int i=0; i<t->children_num; i++)
    {
        num += number_of_nodes(t->children[i]);
    }

    return num;
}

void show(mpc_ast_t* t)
{
    printf("---------------------\n");
    printf("tag: %s\n", t->tag ? t->tag : "null");
    printf("contents: %s\n", t->contents? t->contents: "null");
    printf("num: %d\n", t->children_num);
    for (int i=0; i<t->children_num; i++)
    {
        show(t->children[i]);
    }
}

lval eval(mpc_ast_t* t)
{
    if (strstr(t->tag, "number"))
    {
        errno = 0;
        int i = strtol(t->contents, NULL, 10);
        return errno != ERANGE ? lval_num(i) : lval_err(LERR_BAD_NUM);
    }

    char * op = t->children[1]->contents;
    lval x = eval(t->children[2]);

    int i = 3;
    while (strstr(t->children[i]->tag, "expr"))
    {
        x = eval_op(x, op, eval(t->children[i]));
        i++;
    }

    return x;
}

int main(int argc, char* argv[])
{
    printf("version: 0.0.1");

    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Operator= mpc_new("operator");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lispy = mpc_new("lispy");

    mpca_lang(MPCA_LANG_DEFAULT,                               \
        "                                                      \
            number   : /-?[0-9]+/ ;                            \
            operator : '+' | '-' | '*' | '/' ;                 \
            expr     : <number> | '(' <operator> <expr>+ ')' ; \
            lispy    : /^/ <operator> <expr>+ /$/ ;            \
        ",                                                     \
        Number, Operator, Expr, Lispy);

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
            lval x = eval(r.output);
            lval_println(x);
            mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        //printf("%s\n", line);
        free(line);
    }

    mpc_cleanup(4, Number, Operator, Expr, Lispy);
    return 0;
}
