#include <stdio.h>
#include <stdlib.h>
#include <editline/readline.h>
#include <mpc.h>

int main(int argc, char* argv[])
{
    printf("version: 0.0.1");

    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Operator= mpc_new("operator");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lispy = mpc_new("lispy");

    mpca_lang(MPCA_LANG_DEFAULT,                               \
        "                                                      \
            expr     : 'a' \"ba\"* 'b'? ;                      \
            lispy    : /^/ <expr> /$/ ;                        \
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
            mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.output);
            mpc_err_delete(r.output);
        }
        //printf("%s\n", line);
        free(line);
    }

    mpc_cleanup(4, Number, Operator, Expr, Lispy);
    return 0;
}
