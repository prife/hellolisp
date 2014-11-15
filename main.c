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

lenv* lenv_new(void);
void lenv_del(lenv* e);
lenv* lenv_copy(lenv* e);

struct lenv {
    lenv* par;
    int count;
    char** syms;
    lval** vals;
};

struct lval {
    int type;

    //for basic
    long num;
    char* err;
    char* sym;
    char* str;

    //for Function
    lbuildin buildin; //NULL: lambda, non-null: buildin function
    lenv* env;
    lval* formals;
    lval* body;

    //for S-expr
    int count;
    lval** cell;
};

enum {LVAL_ERR, LVAL_NUM, LVAL_SYM, LVAL_STR, LVAL_FUN, LVAL_SEXPR, LVAL_QEXPR};

char* ltype_name(int type)
{
#define LVAL_TPYE(type)  \
    case type: return #type;

    switch (type)
    {
    LVAL_TPYE(LVAL_ERR);
    LVAL_TPYE(LVAL_NUM);
    LVAL_TPYE(LVAL_SYM);
    LVAL_TPYE(LVAL_STR);
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

lval* lval_str(char* str)
{
    lval* x = malloc(sizeof(lval));
    x->type = LVAL_STR;
    x->str = strdup(str);
    return x;
}

lval* lval_buidin(lbuildin func)
{
    lval* x = malloc(sizeof(lval));
    x->type = LVAL_FUN;
    x->buildin = func;
    return x;
}

lval* lval_lambda(lval* formals, lval* body)
{
    lval* x = malloc(sizeof(lval));
    x->type = LVAL_FUN;
    x->buildin = NULL;
    x->env = lenv_new();
    x->formals = formals;
    x->body = body;

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
    case LVAL_STR: free(v->str); break;
    case LVAL_SYM: free(v->sym); break;
    case LVAL_FUN:
        if (v->buildin == NULL) //lambda
        {
            lenv_del(v->env);
            lval_del(v->formals);
            lval_del(v->body);
        }
        break;
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
    case LVAL_STR: x = lval_str(v->str); break;
    case LVAL_FUN:
           x = malloc(sizeof(lval));
           x->type = v->type;
           x->buildin = v->buildin;
           if (!v->buildin)
           {
               x->env = lenv_copy(v->env);
               x->formals = lval_copy(v->formals);
               x->body = lval_copy(v->body);
           }
           break;
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
    e->par = NULL;
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

lenv* lenv_copy(lenv* e)
{
    lenv* v = malloc(sizeof(lenv));
    v->par =e->par;
    v->count = e->count;
    v->syms = malloc(v->count*sizeof(char*));
    v->vals = malloc(v->count*sizeof(lval*));
    for (int i=0; i<v->count; i++)
    {
        v->syms[i] = strdup(e->syms[i]);
        v->vals[i] = lval_copy(e->vals[i]);
    }
    return v;
}

/* k is just the name of val, to find the value of the val in the env */
lval* lenv_get(lenv* e, lval* k)
{
    for (int i=0; i<e->count; i++)
        if (!strcmp(e->syms[i], k->sym))
            return lval_copy(e->vals[i]);
    if (e->par)
        return lenv_get(e->par, k);
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

void lenv_def(lenv* e, lval* k, lval* v)
{
    while(e->par) e = e->par;
    lenv_put(e, k, v);
}

lval* lval_read_str(mpc_ast_t* t)
{
    /* cut the " at the head/end of the string */
    t->contents[strlen(t->contents)-1] = 0;
    char * unescaped = strdup(t->contents+1);

    /* str : [" h e l l o \ n "] --> "hello\n"
     * size:  9                  --> 6
     */
    unescaped = mpcf_unescape(unescaped);
    lval* str = lval_str(unescaped);
    free(unescaped);
    return str;
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
    else if (strstr(t->tag, "string"))
    {
        x = lval_read_str(t);
        return x;
    }
    else if (!strcmp(t->tag, ">") || strstr(t->tag, "sexpr"))
        x = lval_sexpr();
    else if (strstr(t->tag, "qexpr"))
        x = lval_qexpr();

    for (int i=0; i<t->children_num; i++)
    {
        if (!strcmp(t->children[i]->tag, "regex")) continue;
        if (strstr(t->children[i]->tag, "comment")) continue;
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
                        "get %d, expectedd %d", 1, v->count-1);
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

lval* buildin_ord(lval* v, const char* op)
{
    if (v->count != 3)
        return lval_err("Function %s passed incorrent number of arguments, "
                        "get %d, expectedd %d", op, 1, v->count-1);

    lval* r = lval_num(0);
    lval* x = v->cell[1];
    lval* y = v->cell[2];
    if (x->type != LVAL_NUM || y->type != LVAL_NUM)
        return lval_err("Function '%s' passed incorrect type, "
                        "get <%s> <%s>, expected<%s>", op,
                        ltype_name(x->type), ltype_name(y->type),
                        ltype_name(LVAL_NUM));
    if (!strncmp(op, ">", 1))
        r->num = x->num > y->num;
    if (!strncmp(op, "<", 1))
        r->num = x->num < y->num;
    if (!strncmp(op, ">=", 2))
        r->num = x->num >= y->num;
    if (!strncmp(op, "<=", 2))
        r->num = x->num <= y->num;

    return r;
}

lval* buildin_gt(lenv* e, lval* v)
{
    return buildin_ord(v, ">");
}

lval* buildin_lt(lenv* e, lval* v)
{
    return buildin_ord(v, "<");
}

lval* buildin_ge(lenv* e, lval* v)
{
    return buildin_ord(v, ">=");
}

lval* buildin_le(lenv* e, lval* v)
{
    return buildin_ord(v, "<=");
}

int lval_equal(lval* x, lval* y)
{
    if (x->type != y->type)
        return 0;

    int r = 0;
    switch(x->type)
    {
    case LVAL_ERR: r = !strcmp(x->err, y->err); break;
    case LVAL_NUM: r = x->num == y->num; break;
    case LVAL_SYM: r = !strcmp(x->sym, y->sym); break;
    case LVAL_FUN:
        if (x->buildin || y->buildin)
            r = x->buildin == y->buildin;
        else
            r = lval_equal(x->formals, y->formals)
                && lval_equal(x->body, y->body);
        break;
    case LVAL_SEXPR:
    case LVAL_QEXPR:
        if (x->count == y->count)
        {
            r = !0;
            for (int i=0; i<x->count; i++)
            {
                if (lval_equal(x->cell[i], y->cell[i]) == 0)
                {
                    r = 0;
                    break;
                }
            }
        }
        break;
    }

    return r;
}

lval* buildin_cmp(lval* v, char* op)
{
    if (v->count != 3)
        return lval_err("Function %s passed incorrent number of arguments, "
                        "get %d, expectedd %d", op, 2, v->count-1);
    int r;
    if (!strcmp(op, "=="))
        r = lval_equal(v->cell[1], v->cell[2]);
    else if (!strcmp(op, "!="))
        r = !lval_equal(v->cell[1], v->cell[2]);

    return lval_num(r);
}

lval* buildin_eq(lenv* e, lval* v)
{
    return buildin_cmp(v, "==");
}

lval* buildin_ne(lenv* e, lval* v)
{
    return buildin_cmp(v, "!=");
}

lval* buildin_if(lenv* e, lval* v)
{
    if (v->count-1 != 3)
        return lval_err("Function 'if' passed incorrent number of arguments, "
                        "get %d, expectedd %d", 3, v->count-1);
    if (v->cell[1]->type != LVAL_NUM)
        return lval_err("Function 'if' passed incorrent type, "
                        "get %d, expectedd %d",
                        ltype_name(v->cell[1]->type), ltype_name(LVAL_NUM));
    if (v->cell[2]->type != LVAL_QEXPR)
        return lval_err("Function 'if' passed incorrent type, "
                        "get %d, expectedd %d",
                        ltype_name(v->cell[2]->type), ltype_name(LVAL_QEXPR));
    if (v->cell[3]->type != LVAL_QEXPR)
        return lval_err("Function 'if' passed incorrent type, "
                        "get %d, expectedd %d",
                        ltype_name(v->cell[3]->type), ltype_name(LVAL_QEXPR));

    lval* cond = v->cell[1];
    if (cond->num)
    {
        lval* x = lval_copy(v->cell[2]);
        x->type = LVAL_SEXPR;
        lval_eval(e, x);
    }
    else
    {
        lval* x = lval_copy(v->cell[3]);
        x->type = LVAL_SEXPR;
        return lval_eval(e, x);
    }
}

/*
 * def {x} 1
 * = {x} 1
 */
lval* buildin_val(lenv* e, lval* v, int type)
{
    static const char* name_table[] = {
        "def",
        "=",
    };
    const char* name = name_table[type];
    lval* syms = v->cell[1];

    if (syms->type != LVAL_QEXPR)
        return lval_err("Function %s passed incorrect type!", name);
    if (syms->count == 0 || syms->count != v->count-2)
        return lval_err("Function %s cannot define incorrect"
                        "number of values to symbols!", name);

    for (int i=0; i<syms->count; i++)
    {
        if (syms->cell[i]->type != LVAL_SYM)
            return lval_err("Function %s passed incorrect type, "
                            "get <%s>, expected<%s>", name,
                            ltype_name(syms->cell[i]->type), ltype_name(LVAL_SYM));
    }

    for (int i=0; i<syms->count; i++)
    {
        if (type) //global
            lenv_def(e, syms->cell[i], v->cell[2+i]);
        else
            lenv_put(e, syms->cell[i], v->cell[2+i]);
    }

    return lval_sexpr();
}

lval* buildin_def_global(lenv* e, lval* v)
{
    return buildin_val(e, v, 1); //global
}

lval* buildin_def_local(lenv* e, lval* v)
{
    return buildin_val(e, v, 0); //local
}

//note: v is a S-Expr:
//1. v->cell[0] == "\\"
//2. v->cell[1] == Q-Expr
//3. v->cell[2] == Q-Expr
lval* buildin_lambda(lenv* e, lval* v)
{
    if (v->count != 3)
        return lval_err("Function '\\' passed too many arguments, "
                        "get %d, expectedd %d", 2, v->count);
    if (v->cell[1]->type != LVAL_QEXPR)
        return lval_err("Function '\\' passed incorrect type, "
                        "get <%s>, expected<%s>",
                        ltype_name(v->cell[1]->type), ltype_name(LVAL_QEXPR));
    if (v->cell[2]->type != LVAL_QEXPR)
        return lval_err("Function '\\' passed incorrect type, "
                        "get <%s>, expected<%s>",
                        ltype_name(v->cell[2]->type), ltype_name(LVAL_QEXPR));

    //check formals
    for (int i=0; i<v->cell[1]->count; i++)
    {
        lval*x = v->cell[1]->cell[i];
        if (x->type != LVAL_SYM)
            return lval_err("cannot define a non-symbol. "
                            "get <%s>, expected<%s>",
                            ltype_name(x->type), ltype_name(LVAL_SYM));
    }

    return lval_lambda(lval_copy(v->cell[1]), lval_copy(v->cell[2]));
}

/**
 * f= a->cell[0]
 * v: function
 * a: the function arguments
 **/
lval* lval_call(lenv* e, lval* f, lval* a)
{
    if (f->buildin) return f->buildin(e, a);

    //lambda
    if (a->count-1 > f->formals->count)
        return lval_err("Function passed too many arguments. "
                        "Got %i, Expected %i.", a->count-1, f->formals->count);

    int i=0;
    for (; i<a->count-1; i++)
    {
        lval* val = a->cell[1+i];
        lval* sym = f->formals->cell[i];
        lenv_put(f->env, sym, val);
    }

    if (i == f->formals->count)
    {
        f->env->par = e;
        lval* body = lval_copy(f->body);
        body->type = LVAL_SEXPR;
        return lval_eval(f->env, body);
    } else {
    //construct
        lval* x = malloc(sizeof(lval));
        x->env = lenv_copy(f->env);
        x->type = f->type;
        x->buildin = f->buildin;

        //construct the remain formals
        lval* remain_formals = lval_expr(f->formals->type);
        for (int k=i; k<f->formals->count; k++)
        {
            lval_add(remain_formals, lval_clone(f->formals, k));
        }

        x->formals = remain_formals;
        x->body = lval_copy(f->body);
        return x;
    }
}

void lenv_add_buildin(lenv* e, char* name, lbuildin func)
{
    lval* k = lval_sym(name);
    lval* f = lval_buidin(func);
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
    lenv_add_buildin(e, "\\",  buildin_lambda);
    lenv_add_buildin(e, "def",  buildin_def_global);
    lenv_add_buildin(e, "=",  buildin_def_local);

    lenv_add_buildin(e, ">",  buildin_gt);
    lenv_add_buildin(e, "<",  buildin_lt);
    lenv_add_buildin(e, ">=",  buildin_ge);
    lenv_add_buildin(e, "<=",  buildin_le);

    lenv_add_buildin(e, "==",  buildin_eq);
    lenv_add_buildin(e, "!=",  buildin_ne);

    lenv_add_buildin(e, "if",  buildin_if);

    lenv_add_buildin(e, "+", buildin_add);
    lenv_add_buildin(e, "-", buildin_sub);
    lenv_add_buildin(e, "*", buildin_mul);
    lenv_add_buildin(e, "/", buildin_div);
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

void lval_str_print(lval* v)
{
    char* escaped = strdup(v->str);
    escaped = mpcf_escape(escaped);
    printf("\"%s\"", escaped);
    free(escaped);
}

void lval_print(lval *v)
{
    switch (v->type)
    {
    case LVAL_ERR: printf("Error: %s", v->err); break;
    case LVAL_NUM: printf("%ld", v->num); break;
    case LVAL_SYM: printf("%s", v->sym); break;
    case LVAL_STR: lval_str_print(v); break;
    case LVAL_FUN:
        if (v->buildin)
        {
            printf("<buildin: %p>", v->buildin);
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

    result = lval_call(e, f, v);

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
    mpc_parser_t* String = mpc_new("string");
    mpc_parser_t* Comment = mpc_new("comment");
    mpc_parser_t* Sexpr = mpc_new("sexpr");
    mpc_parser_t* Qexpr = mpc_new("qexpr");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lispy = mpc_new("lispy");

    mpca_lang(MPCA_LANG_DEFAULT,                                      \
        "                                                             \
            number   : /-?[0-9]+/ ;                                   \
            symbol   : /[a-zA-Z_0-9+\\-*\\/\\\\=<>!&]+/ ;             \
            string   : /\"(\\\\.|[^\"])*\"/ ;                         \
            comment  : /;[^\\r\\n]*/ ;                                 \
            sexpr    : '(' <expr>* ')' ;                              \
            qexpr    : '{' <expr>* '}' ;                              \
            expr     : <number>  | <symbol> | <string> |              \
                       <comment> | <sexpr>  | <qexpr> ;                           \
            lispy    : /^/ <expr>* /$/ ;                              \
        ",                                                            \
        Number, Symbol, String, Comment, Sexpr, Qexpr, Expr, Lispy);

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

    mpc_cleanup(8, Number, Symbol, String, Comment, Sexpr, Qexpr, Expr, Lispy);
    return 0;
}
