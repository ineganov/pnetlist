#ifndef PNETLIST_H
#define PNETLIST_H

enum token_e { Tk_Null = 0, Tk_EOF, Tk_LParen, Tk_RParen, Tk_LBrace, Tk_RBrace, Tk_LBracket, Tk_RBracket, Tk_Ident, Tk_Literal, Tk_String, Tk_Semi, Tk_Colon, Tk_Comma, Tk_Hash, Tk_Dot,
               Tk_BaseHex, Tk_BaseDec, Tk_BaseOct, Tk_BaseBin, Tk_Op_Plus, Tk_Op_Minus, Tk_Op_Mul, Tk_Op_Div,
               Tk_Kw_assign, Tk_Kw_begin, Tk_Kw_end, Tk_Kw_endmodule, Tk_Kw_inout, Tk_Kw_input, Tk_Kw_module, Tk_Kw_output, Tk_Kw_reg, Tk_Kw_wire };

union val_u {
   char c;
   char * str;
   int  val;
};

typedef struct token_s {
   enum  token_e    kind;
   union val_u      val;
   int              line_num;
   struct token_s * next;
} Token;

struct Expr {
   struct Expr * next;
   enum Expr_Kind { Expr_Literal, Expr_Ident, Expr_Idx, Expr_Range, Expr_Concat, Expr_String } kind;
   union { char * ident;
           char * string_lit;
           struct Literal { int width; char * lit; } * literal;
           struct Idx { char * name; int idx; } * idx;
           struct Range { char * name; int hi; int lo; } * range;
           struct Expr * concat;
   } expr; 
};

struct Wire_Decl {
   char *name;
   int hi, lo;
   int is_bus;
};

struct Module_Inst {
   char *type;
   char *name;
   struct Bind {char * name; struct Expr * expr; struct Bind * next;} * params;
   struct Bind * conns;
};

struct Module_Entity {
   enum   { M_Ent_Inst, M_Ent_Wire, M_Ent_Input, M_Ent_Output } kind;
   union  { struct Module_Inst  mod_inst;
            struct Wire_Decl wire_decl; } ent;
   struct Module_Entity * next;
};

struct Module_Def {
   char * name;
   struct IO_Port {char * name; struct IO_Port * next; } * io_ports;
   struct Module_Entity *entities;
   struct Module_Def * next;
};

struct mod_count {char * nm; int cnt;};

/* forward declarations */

void * my_malloc(size_t);
char * string_stash(char *, int);
char * string_stash_temp(char *, int);

struct mod_count ** unique_modules(struct Module_Def *);

Token *             tokenize_file(char *);
struct Module_Def * parse_file(Token *);
void                pp_modules(struct Module_Def *);

#endif
