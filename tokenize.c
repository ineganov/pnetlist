#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#define MAX_ALLOC      65536
#define MAX_TMP_ALLOC  1024
#define MAX_TOKENS     65536

#define MAX_CONCAT     32

enum token_e { Tk_Null = 0, Tk_EOF, Tk_LParen, Tk_RParen, Tk_LBrace, Tk_RBrace, Tk_LBracket, Tk_RBracket, Tk_Ident, Tk_Literal, Tk_Semi, Tk_Colon, Tk_Comma, Tk_Hash, Tk_Dot,
               Tk_BaseHex, Tk_BaseDec, Tk_BaseOct, Tk_BaseBin, Tk_Op_Plus, Tk_Op_Minus, Tk_Op_Mul, Tk_Op_Div,
               Tk_Kw_assign, Tk_Kw_begin, Tk_Kw_end, Tk_Kw_endmodule, Tk_Kw_inout, Tk_Kw_input, Tk_Kw_module, Tk_Kw_output, Tk_Kw_reg, Tk_Kw_wire };

char * tk_print[] = { "Tk_Null", "Tk_EOF", "(", ")", "{", "}", "[", "]", "Tk_Ident", "Tk_Literal", ";", ":", ",", "#", ".",
                      "Tk_BaseHex", "Tk_BaseDec", "Tk_BaseOct", "Tk_BaseBin", "+", "-", "*", "/", "assign", "begin", "end", "endmodule", "inout", "input", "module", "output", "reg", "wire" };

union val_u {
   char c;
   char * str;
   int  val;
};

typedef struct token_s {
   enum  token_e kind;
   union val_u   val;
   int           line_num;
} Token;

struct token_s tok_array[MAX_TOKENS];


struct Expr {
   enum Expr_Kind { Expr_Literal, Expr_Ident, Expr_Idx, Expr_Concat } kind;
   union { char * ident;
           struct Literal { int width; char * lit; } * literal;
           struct Idx { char * name; int idx; } * idx;
           char ** concat_idents;
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
};

void pp_tokens(Token * tok_array, int num_tkn ) {

   int line_num = 0;
   for (int i = 0; i < num_tkn; ++i) {
      if(tok_array[i].line_num != line_num) {
         line_num = tok_array[i].line_num;
         printf("\nLine %4d: ", line_num);
      }

      if (tok_array[i].kind == Tk_Ident)
         printf(" <Id %s> ", tok_array[i].val.str );
      else if (tok_array[i].kind == Tk_BaseHex)
         printf(" <Hx %s> ", tok_array[i].val.str );
      else if (tok_array[i].kind == Tk_BaseDec)
         printf(" <Dc %s> ", tok_array[i].val.str );
      else if (tok_array[i].kind == Tk_BaseOct)
         printf(" <Oc %s> ", tok_array[i].val.str );
      else if (tok_array[i].kind == Tk_BaseBin)
         printf(" <Bn %s> ", tok_array[i].val.str );
      else if (tok_array[i].kind == Tk_Literal)
         printf(" <Lt %d> ", tok_array[i].val.val );
      else
         printf(" %s ", tk_print[tok_array[i].kind] );
   }
   printf("\n");
   return;
}

void pp_expr(struct Expr * e) {
   switch(e->kind) {
      case Expr_Ident:   printf("%s", e->expr.ident); break;
      case Expr_Literal: printf("%d'%s",  e->expr.literal->width, e->expr.literal->lit); break;
      case Expr_Idx:     printf("%s[%d]", e->expr.idx->name, e->expr.idx->idx ); break;
      case Expr_Concat:  printf("{");
                         for(char ** iter = e->expr.concat_idents; *iter != NULL; iter++) printf("%s ", *iter);
                         printf("}");
                         break;
   }
   return;
}

void pp_mod_entity(struct Module_Entity * me) {
   switch(me->kind){
      case M_Ent_Inst:  printf(" <instance> %s/%s (", me->ent.mod_inst.type, me->ent.mod_inst.name);
                        for(struct Bind * conn = me->ent.mod_inst.conns; conn != NULL; conn = conn->next )
                           {printf(" .%s->", conn->name); pp_expr(conn->expr);} // conn->expr->kind == Expr_Ident ? conn->expr->expr.ident : "expr");
                        printf(")\n");
                        break;
      case M_Ent_Input:  printf(" <input: %2d>  %s\n", me->ent.wire_decl.hi - me->ent.wire_decl.lo + 1, me->ent.wire_decl.name); break;
      case M_Ent_Output: printf(" <output:%2d>  %s\n", me->ent.wire_decl.hi - me->ent.wire_decl.lo + 1, me->ent.wire_decl.name); break;
      case M_Ent_Wire:   printf(" <wire:  %2d>  %s\n", me->ent.wire_decl.hi - me->ent.wire_decl.lo + 1, me->ent.wire_decl.name); break;
   }
   return;
}

void pp_module(struct Module_Def * md) {
   printf("Module %s:\n", md->name);

   for(struct IO_Port * iter = md->io_ports; iter != NULL; iter = iter->next ) printf("  io port: %s\n", iter->name);
   for(struct Module_Entity * iter = md->entities; iter != NULL; iter = iter->next ) pp_mod_entity(iter);

   return;
}

static int stash_allocated = 0;
static int malloc_allocated = 0;

void * my_malloc(size_t  size) {
   malloc_allocated += size;
   return malloc(size);
}


char * string_stash(char * src, int len)
{
   static char string_array[MAX_ALLOC];

   if (stash_allocated + len < MAX_ALLOC) {
      int old_alloc = stash_allocated;
      for (int i = 0; i < len; ++i) string_array[stash_allocated++] = src[i];
      string_array[stash_allocated++] = '\0';
      return &string_array[old_alloc];
   }
   else {
      printf("Oh noes! Attempt to allocate above limit (%d bytes) failed!\n", MAX_ALLOC);
      exit(2);
   }
}

char * string_stash_temp(char * src, int len)
{
   static char string_temp[MAX_TMP_ALLOC];

   if (len < MAX_ALLOC) {
      for (int i = 0; i < len; ++i) string_temp[i] = src[i];
      string_temp[len] = '\0';
      return string_temp;
   }
   else {
      printf("Oh noes! Attempt to allocate above limit (%d bytes) failed!\n", MAX_TMP_ALLOC);
      exit(2);
   }
}

int take_while(int (*test)(int), char * stream) {
   int len = 0;
   while (test(stream[len])) ++len;
   return len;
}

enum token_e is_keyword(char * str) {
   // This should be a hash map, but binsearch would do for now
   static char * keywords[] = {"assign", "begin", "end", "endmodule", "inout", "input", "module", "output", "reg", "wire" };

   int hi = sizeof(keywords)/sizeof(char *) - 1;
   int lo = 0;

   do {
      int idx = (hi + lo) / 2;
      int comp = strcmp(str, keywords[idx]);
      // printf("binsearch: %s, [%d:%d@%d], cmp = %s, cc = %d\n", str, lo, hi, idx, keywords[idx], comp );
      if      (comp < 0) { hi = idx - 1;  }
      else if (comp > 0) { lo = idx + 1;  }
      else               return Tk_Kw_assign + idx;
   } while (hi >= lo);

   return Tk_Ident;
}

int isidentsym  (int c) { return c == '_' || isalnum(c);  }
int isnotspace  (int c) { return ! isspace(c); }
int isnotnewline(int c) { return c != '\n'; }
int ishex       (int c) { return c == '_' || c == 'x' || c == 'z' || c == 'X' || c == 'Z' || ishexnumber(c); }
int isdec       (int c) { return c == '_' || c == 'x' || c == 'z' || c == 'X' || c == 'Z' || isnumber(c); }
int isoct       (int c) { return c == '_' || c == 'x' || c == 'z' || c == 'X' || c == 'Z' || (c >= '0' && c <= '7'); }
int isbin       (int c) { return c == '_' || c == 'x' || c == 'z' || c == 'X' || c == 'Z' || c == '0' || c == '1';  }

int tokenize(char * char_stream, long size, struct token_s tok_array[]) {
   int num_tkn  = 0;
   int line_num = 1;
   int pos      = 0;

   while (pos < size - 1) {
      if(char_stream[pos] == '_' || isalpha(char_stream[pos])) {
         int len = take_while(isidentsym, &char_stream[pos]);
         char *s = string_stash(&char_stream[pos], len); // no need to stash keywords, just more convenient to compare with KWs
         tok_array[num_tkn++] = (Token) { .kind = is_keyword(s), .val.str = s, .line_num = line_num };
         pos += len;
      }
      else if(char_stream[pos] == '\\') {
         int len = take_while(isnotspace, &char_stream[pos]);
         char *s = string_stash(&char_stream[pos+1], len-1); // Drop the slash itself
         tok_array[num_tkn++] = (Token) { .kind = Tk_Ident, .val.str = s, .line_num = line_num };
         pos += len+1; //Final space needs to be eaten
      }
      else if( isnumber(char_stream[pos]) ) {
         int len = take_while(isnumber, &char_stream[pos]);
         char *s = string_stash_temp(&char_stream[pos], len);
         tok_array[num_tkn++] = (Token) { .kind = Tk_Literal, .val.val = atoi(s), .line_num = line_num };
         pos += len;
      }
      else if( char_stream[pos] == '\'' && char_stream[pos+1] == 'h' ) {
         if (pos + 2 >= size) { printf("Expected hex literal, got EOF\n"); exit(3); }
         int len = take_while(ishex, &char_stream[pos+2]);
         char *s = string_stash(&char_stream[pos+2], len);
         tok_array[num_tkn++] = (Token) { .kind = Tk_BaseHex, .val.str = s, .line_num = line_num };
         pos += len+2;
      }
      else if( char_stream[pos] == '\'' && char_stream[pos+1] == 'd' ) {
         if (pos + 2 >= size) { printf("Expected dec literal, got EOF\n"); exit(3); }
         int len = take_while(isdec, &char_stream[pos+2]);
         char *s = string_stash(&char_stream[pos+2], len);
         tok_array[num_tkn++] = (Token) { .kind = Tk_BaseDec, .val.str = s, .line_num = line_num };
         pos += len+2;
      }
      else if( char_stream[pos] == '\'' && char_stream[pos+1] == 'o' ) {
         if (pos + 2 >= size) { printf("Expected oct literal, got EOF\n"); exit(3); }
         int len = take_while(isoct, &char_stream[pos+2]);
         char *s = string_stash(&char_stream[pos+2], len);
         tok_array[num_tkn++] = (Token) { .kind = Tk_BaseOct, .val.str = s, .line_num = line_num };
         pos += len+2;
      }
      else if( char_stream[pos] == '\'' && char_stream[pos+1] == 'b' ) {
         if (pos + 2 >= size) { printf("Expected bin literal, got EOF\n"); exit(3); }
         int len = take_while(isbin, &char_stream[pos+2]);
         char *s = string_stash(&char_stream[pos+2], len);
         tok_array[num_tkn++] = (Token) { .kind = Tk_BaseBin, .val.str = s, .line_num = line_num };
         pos += len+2;
      }
      else if(char_stream[pos] == '/' && char_stream[pos+1] == '/')  { pos +=take_while(isnotnewline, &char_stream[pos]);      }
      else if(char_stream[pos] == '.')  { tok_array[num_tkn++] = (Token) { .kind = Tk_Dot,      .line_num = line_num }; ++pos; }
      else if(char_stream[pos] == '+')  { tok_array[num_tkn++] = (Token) { .kind = Tk_Op_Plus,  .line_num = line_num }; ++pos; }
      else if(char_stream[pos] == '-')  { tok_array[num_tkn++] = (Token) { .kind = Tk_Op_Minus, .line_num = line_num }; ++pos; }
      else if(char_stream[pos] == '*')  { tok_array[num_tkn++] = (Token) { .kind = Tk_Op_Mul,   .line_num = line_num }; ++pos; }
      else if(char_stream[pos] == '/')  { tok_array[num_tkn++] = (Token) { .kind = Tk_Op_Div,   .line_num = line_num }; ++pos; }
      else if(char_stream[pos] == '(')  { tok_array[num_tkn++] = (Token) { .kind = Tk_LParen,   .line_num = line_num }; ++pos; }
      else if(char_stream[pos] == ')')  { tok_array[num_tkn++] = (Token) { .kind = Tk_RParen,   .line_num = line_num }; ++pos; }
      else if(char_stream[pos] == '{')  { tok_array[num_tkn++] = (Token) { .kind = Tk_LBrace,   .line_num = line_num }; ++pos; }
      else if(char_stream[pos] == '}')  { tok_array[num_tkn++] = (Token) { .kind = Tk_RBrace,   .line_num = line_num }; ++pos; }
      else if(char_stream[pos] == '[')  { tok_array[num_tkn++] = (Token) { .kind = Tk_LBracket, .line_num = line_num }; ++pos; }
      else if(char_stream[pos] == ']')  { tok_array[num_tkn++] = (Token) { .kind = Tk_RBracket, .line_num = line_num }; ++pos; }
      else if(char_stream[pos] == ';')  { tok_array[num_tkn++] = (Token) { .kind = Tk_Semi,     .line_num = line_num }; ++pos; }
      else if(char_stream[pos] == ':')  { tok_array[num_tkn++] = (Token) { .kind = Tk_Colon,    .line_num = line_num }; ++pos; }
      else if(char_stream[pos] == ',')  { tok_array[num_tkn++] = (Token) { .kind = Tk_Comma,    .line_num = line_num }; ++pos; }
      else if(char_stream[pos] == '#')  { tok_array[num_tkn++] = (Token) { .kind = Tk_Hash,     .line_num = line_num }; ++pos; }
      else if(char_stream[pos] == ' ')  { ++pos; }
      else if(char_stream[pos] == '\n') { ++pos; ++line_num; }
      else { printf("Unexpected token at line %d: %c\n", line_num, char_stream[pos]); exit(1); }
   }

   tok_array[num_tkn++] = (Token) { .kind = Tk_EOF, .line_num = line_num };

   return num_tkn;
}

void tokenize_file (const char * filepath) {
   FILE * fp;
   if(!(fp = fopen(filepath, "r"))) { printf("Failed to open file: %s\nBye!\n", filepath); exit(1); }

   fseek(fp, 0, SEEK_END); long f_size = ftell(fp); rewind(fp);
   printf("File size: %ld bytes\n", f_size);

   char * contents = my_malloc(f_size);

   fread(contents, 1, f_size, fp);
   fclose(fp);

   int tkns = tokenize(contents, f_size, tok_array);
   pp_tokens(tok_array, tkns);

   free(contents);
   return;
}


Token * expect(Token * tok_array, enum token_e expectation) {
   if( tok_array->kind == expectation) { return ++tok_array; }
   else {
      printf("At line %d: expected token: '%s', and got: '%s'\n", tok_array->line_num, tk_print[expectation], tk_print[tok_array->kind]);
      exit(3);
   }
}

Token * parse_wire_decl(Token * tk, struct Wire_Decl * nw) {
   tk++; // Assumes wire|input|output|inout is checked for one level above
   if(tk->kind == Tk_LBracket) {
      tk++;
      expect(tk, Tk_Literal);  nw->hi = tk->val.val; tk++;
      expect(tk, Tk_Colon);    tk++;
      expect(tk, Tk_Literal);  nw->lo = tk->val.val; tk++;
      expect(tk, Tk_RBracket); tk++;
      nw->is_bus = 1;
   } else nw->hi = nw->lo = nw->is_bus = 0;
   expect(tk, Tk_Ident);
   nw->name = tk->val.str; tk++;
   expect(tk, Tk_Semi); tk++;
   return tk;
}

Token * parse_expr(Token * tk, struct Expr * e) {

   switch(tk->kind) {
      case Tk_Ident:
         if( (tk+1)->kind == Tk_LBracket) {
            struct Idx * new_idx = my_malloc(sizeof(struct Idx));
            e->kind = Expr_Idx;
            new_idx->name = tk->val.str; tk++;
            expect(tk, Tk_LBracket); tk++;
            expect(tk, Tk_Literal);
            new_idx->idx = tk->val.val; tk++;
            expect(tk, Tk_RBracket); tk++;
            e->expr.idx = new_idx;
            break;
         } else {
            e->kind = Expr_Ident;
            e->expr.ident = tk->val.str;
            tk++;
         }
         break;
      case Tk_Literal:
         e->kind = Expr_Literal;
         struct Literal * lit = my_malloc(sizeof(struct Literal));
         if( (tk+1)->kind == Tk_BaseHex || 
             (tk+1)->kind == Tk_BaseDec || 
             (tk+1)->kind == Tk_BaseOct ||
             (tk+1)->kind == Tk_BaseBin ) {
            lit->width = tk->val.val; tk++;
            lit->lit   = tk->val.str; tk++;
         } else {
            lit->width = 32;
            char * lit_str = my_malloc(12);
            sprintf(lit_str, "%d", tk->val.val);
            lit->lit   = lit_str; tk++;
         }
         e->expr.literal = lit;
         break;
      case Tk_LBrace:
         e->kind = Expr_Concat;
         tk++;
         char ** concat_lst = my_malloc(MAX_CONCAT*sizeof(char *));
         e->expr.concat_idents = concat_lst;

         while(tk->kind != Tk_RBrace) {
            expect(tk, Tk_Ident);
            *concat_lst++ = tk->val.str; tk++;
            if(tk->kind != Tk_Comma) break;
            else tk++;
         }
         *concat_lst = NULL;
         expect(tk, Tk_RBrace); tk++;
         break;

      default: printf("Expected literal|ident|lbrace at line %d, but got: %s\n", tk->line_num, tk_print[tk->kind]);
   }
   return tk;
}

Token * parse_module_inst(Token * tk, struct Module_Inst * mi) {
   // token is 'ident' here
   mi->type = tk->val.str; tk++;
   mi->params = NULL;
   mi->conns = NULL;

   if(tk->kind == Tk_Hash) {
      tk++;
      expect(tk, Tk_LParen); tk++;
      
      while(tk->kind != Tk_RParen) {
         struct Bind * new_bind = my_malloc(sizeof(struct Bind));
         struct Expr * new_expr = my_malloc(sizeof(struct Expr));

         expect(tk, Tk_Dot);    tk++;
         expect(tk, Tk_Ident);
         new_bind->name = tk->val.str; tk++;

         expect(tk, Tk_LParen); tk++;
         tk = parse_expr(tk, new_expr);
         new_bind->expr = new_expr;
         new_bind->next = mi->params;
         mi->params = new_bind;
         expect(tk, Tk_RParen); tk++;

         if(tk->kind == Tk_Comma) tk++; else break;
      }
      expect(tk, Tk_RParen); tk++;
   }

   expect(tk, Tk_Ident);
   mi->name = tk->val.str; tk++;

   expect(tk, Tk_LParen); tk++;

   while(tk->kind != Tk_RParen) {
      struct Bind * new_bind = my_malloc(sizeof(struct Bind));
      struct Expr * new_expr = my_malloc(sizeof(struct Expr));

      expect(tk, Tk_Dot); tk++;
      expect(tk, Tk_Ident);
      new_bind->name = tk->val.str; tk++;

      expect(tk, Tk_LParen); tk++;
      tk = parse_expr(tk, new_expr);
      new_bind->expr = new_expr;
      new_bind->next = mi->conns;
      mi->conns = new_bind;
      expect(tk, Tk_RParen); tk++;

      if(tk->kind == Tk_Comma) tk++; else break;
   }

   expect(tk, Tk_RParen); tk++;
   expect(tk, Tk_Semi); tk++;
   return tk;
}

Token * parse_module_entity(Token * tk, struct Module_Entity * e) {
   switch(tk->kind) {
      case Tk_Kw_wire:
         e->kind = M_Ent_Wire;
         tk = parse_wire_decl(tk, &e->ent.wire_decl);
         break;
      case Tk_Kw_input:
         e->kind = M_Ent_Input;
         tk = parse_wire_decl(tk, &e->ent.wire_decl);
         break;
      case Tk_Kw_output:
         e->kind = M_Ent_Output;
         tk = parse_wire_decl(tk, &e->ent.wire_decl);
         break;
      case Tk_Kw_assign:
         printf("assign statement not implemented! (line %d)\n", tk->line_num);
         exit(3);
         break;
      case Tk_Ident:
         e->kind = M_Ent_Inst;
         tk = parse_module_inst(tk, &e->ent.mod_inst);
         break;
      default: {printf("Expected wire|input|output|module_inst at line %d, but got '%s'\n", tk->line_num, tk_print[tk->kind]); exit(3); }
   }
   return tk;
}

Token * parse_module_def(Token * tk, struct Module_Def * md) {
   expect(tk, Tk_Kw_module); tk++;

   expect(tk, Tk_Ident);
   md->name = tk->val.str; tk++;
   md->io_ports = NULL;
   md->entities = NULL;

   if(tk->kind == Tk_LParen) { //Possibly empty io list
      tk++;
      if(tk->kind != Tk_RParen) // Handle empty list
         while(1) {
            expect(tk, Tk_Ident);
            struct IO_Port * new_port = my_malloc(sizeof(struct IO_Port));
            new_port->name = tk->val.str;
            new_port->next = md->io_ports;
            md->io_ports = new_port;
            tk++;
            if(tk->kind == Tk_Comma) tk++;
            else break;
         }
      expect(tk, Tk_RParen); tk++;
   }
   expect(tk, Tk_Semi); tk++;

   while(tk->kind != Tk_Kw_endmodule || tk->kind == Tk_EOF) {
      struct Module_Entity * new_entity = my_malloc(sizeof(struct Module_Entity));
      tk = parse_module_entity(tk, new_entity);
      new_entity->next = md->entities;
      md->entities = new_entity;
   }

   if(tk->kind == Tk_EOF) { printf("Expected endmodule, but got EOF!\n"); exit(2); }

   expect(tk, Tk_Kw_endmodule);

   return tk;
}


int main(int ac, char **av) {
   printf("int: %lu, long: %lu, struct: %lu, enum: %lu\n", sizeof(int), sizeof(long), sizeof(struct token_s), sizeof(enum token_e) );

   for (char **q = av; *q != NULL; ++q) printf(">> %s\n", *q);

   tokenize_file(av[1]);

   struct Module_Def * md = my_malloc(sizeof(struct Module_Def));
   parse_module_def(tok_array, md);
   pp_module(md);

   printf("\nAllocated %d bytes for string stash and %d bytes for parse structures\n\n", stash_allocated, malloc_allocated);


}