#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#define MAX_IDENT_LENGTH  4096

enum token_e { Tk_Null = 0, Tk_EOF, Tk_LParen, Tk_RParen, Tk_LBrace, Tk_RBrace, Tk_LBracket, Tk_RBracket, Tk_Ident, Tk_Literal, Tk_String, Tk_Semi, Tk_Colon, Tk_Comma, Tk_Hash, Tk_Dot,
               Tk_BaseHex, Tk_BaseDec, Tk_BaseOct, Tk_BaseBin, Tk_Op_Plus, Tk_Op_Minus, Tk_Op_Mul, Tk_Op_Div,
               Tk_Kw_assign, Tk_Kw_begin, Tk_Kw_end, Tk_Kw_endmodule, Tk_Kw_inout, Tk_Kw_input, Tk_Kw_module, Tk_Kw_output, Tk_Kw_reg, Tk_Kw_wire };

char * tk_print[] = { "Tk_Null", "Tk_EOF", "(", ")", "{", "}", "[", "]", "Tk_Ident", "Tk_Literal", "Tk_String", ";", ":", ",", "#", ".",
                      "Tk_BaseHex", "Tk_BaseDec", "Tk_BaseOct", "Tk_BaseBin", "+", "-", "*", "/", "assign", "begin", "end", "endmodule", "inout", "input", "module", "output", "reg", "wire" };

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
   enum Expr_Kind { Expr_Literal, Expr_Ident, Expr_Idx, Expr_Concat } kind;
   union { char * ident;
           struct Literal { int width; char * lit; } * literal;
           struct Idx { char * name; int idx; } * idx;
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

void pp_tokens(Token * tok_list) {

   int line_num = 0;
   for (Token * tk = tok_list; tk->next != NULL; tk = tk->next) {
      if(tk->line_num != line_num) {
         line_num = tk->line_num;
         printf("\nLine %4d: ", line_num);
      }

      if      (tk->kind == Tk_Ident)   printf(" <Id %s> ", tk->val.str );
      else if (tk->kind == Tk_BaseHex) printf(" <Hx %s> ", tk->val.str );
      else if (tk->kind == Tk_BaseDec) printf(" <Dc %s> ", tk->val.str );
      else if (tk->kind == Tk_BaseOct) printf(" <Oc %s> ", tk->val.str );
      else if (tk->kind == Tk_BaseBin) printf(" <Bn %s> ", tk->val.str );
      else if (tk->kind == Tk_Literal) printf(" <Lt %d> ", tk->val.val );
      else                             printf(" %s ", tk_print[tk->kind] );
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
                         for(struct Expr * iter = e->expr.concat; iter != NULL; iter = iter->next) {
                           pp_expr(iter);
                           if (iter->next) printf(", ");
                         }
                         printf("}");
                         break;
   }
   return;
}

struct Module_Entity * reverse_mod_entity(struct Module_Entity * me) {
   if(me == NULL) return me;

   struct Module_Entity * tmp;
   struct Module_Entity * prev = me;
   struct Module_Entity * iter = me->next;
   me->next = NULL;

   while(iter != NULL) {
      tmp  = iter->next;
      iter->next = prev;
      prev = iter;
      iter = tmp;
   }

   return prev;
}

struct mod_count {char * nm; int cnt;};

static int stash_allocated = 0;
static int malloc_allocated = 0;

void * my_malloc(size_t  size) {
   malloc_allocated += size;
   return malloc(size);
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

int comp_func(const void *a, const void *b) { return strcmp( (*(struct mod_count **)a)->nm,
                                                             (*(struct mod_count **)b)->nm ); }

struct mod_count ** unique_modules(struct Module_Def * md) {
   struct mod_count ** modules;
   int num_modules = 0;

   for( struct Module_Entity * iter = md->entities; iter != NULL; iter = iter->next)
      if(iter -> kind == M_Ent_Inst) num_modules++;

   modules = my_malloc((num_modules + 1) * sizeof(struct mod_count *));
   modules[num_modules] = NULL;

   int i = 0;
   for( struct Module_Entity * iter = md->entities; iter != NULL; iter = iter->next)
      if(iter -> kind == M_Ent_Inst) {
         modules[i] = my_malloc(sizeof(struct mod_count));
         modules[i]->nm  = iter->ent.mod_inst.type;
         modules[i]->cnt = 1;
         i++;
      }

   qsort(modules, num_modules, sizeof(char *), comp_func);

   int last_uniq = 0;
   int uniq_iter = 0;
   for(int i = 1; i < num_modules; ++i)
      if(strcmp(modules[last_uniq]->nm, modules[i]->nm)) { // if not equal
         last_uniq = i;
         modules[++uniq_iter] = modules[i];
      }
      else modules[last_uniq]->cnt += 1;

   modules[++uniq_iter] = NULL;

   return modules;
}


void pp_modules(struct Module_Def * md) {
   for(struct Module_Def * mi = md; mi != NULL; mi = mi->next) {
      
      printf("\nModule %s:\n", mi->name);
      for(struct IO_Port * iter = mi->io_ports; iter != NULL; iter = iter->next ) printf("  io port: %s\n", iter->name);
      for(struct Module_Entity * iter = mi->entities; iter != NULL; iter = iter->next ) pp_mod_entity(iter);

      struct mod_count ** mod_list = unique_modules(mi);

      printf("\nModule '%s' statistics:\n", mi->name);
      while(*mod_list) {printf("%50s %-4d\n", (*mod_list)->nm, (*mod_list)->cnt ); mod_list++;}

   }
   return;
}

char * string_stash(char * src, int len)
{
   char * new_string = my_malloc((len+1)*sizeof(char));  stash_allocated += len + 1;
   for (int i = 0; i < len; ++i) new_string[i] = src[i]; // strncpy, anyone?
   new_string[len] = '\0';

   return new_string;
}

char * string_stash_temp(char * src, int len)
{
   static char string_temp[MAX_IDENT_LENGTH];

   if (len < MAX_IDENT_LENGTH) {
      for (int i = 0; i < len; ++i) string_temp[i] = src[i];
      string_temp[len] = '\0';
      return string_temp;
   }
   else {
      printf("Oh noes! Attempt to allocate above limit (%d bytes) failed!\n", MAX_IDENT_LENGTH);
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
int isnotstarparen (int c) { return c != ')'; } //FIXME: star-paren, not just paren!
int isnotquote  (int c) { return c != '"'; }

Token * tok_append(Token * last_token, Token * new_tkn ) {
   last_token->next = my_malloc(sizeof(Token));
   *(last_token->next) = *new_tkn;
   return last_token->next;
}

Token * tokenize(char * char_stream, long size) {
   Token * last_token  = malloc(sizeof(Token));
   Token * first_token = last_token;

   int line_num = 1;
   int pos      = 0;

   while (pos < size - 1) {
      if(char_stream[pos] == '_' || isalpha(char_stream[pos])) {
         int len = take_while(isidentsym, &char_stream[pos]);
         char *s = string_stash(&char_stream[pos], len); // no need to stash keywords, just more convenient to compare with KWs
         last_token = tok_append(last_token, & (Token) { .kind = is_keyword(s), .val.str = s, .line_num = line_num, .next = NULL });
         pos += len;
      }
      else if(char_stream[pos] == '"' ) {
         int len = take_while(isnotquote, &char_stream[pos+1]);
         char * s = string_stash(&char_stream[pos+1], len);
         last_token = tok_append(last_token, & (Token) { .kind = Tk_String, .val.str = s, .line_num = line_num, .next = NULL });
         pos += len+2; //Eat the end quote, recall we started with a +1
      }
      else if(char_stream[pos] == '\\') {
         int len = take_while(isnotspace, &char_stream[pos]);
         char *s = string_stash(&char_stream[pos+1], len-1); // Drop the slash itself
         last_token = tok_append(last_token, & (Token) { .kind = Tk_Ident, .val.str = s, .line_num = line_num, .next = NULL });
         pos += len+1; //Final space needs to be eaten
      }
      else if( isnumber(char_stream[pos]) ) {
         int len = take_while(isnumber, &char_stream[pos]);
         char *s = string_stash_temp(&char_stream[pos], len);
         last_token = tok_append(last_token, & (Token) { .kind = Tk_Literal, .val.val = atoi(s), .line_num = line_num, .next = NULL });
         pos += len;
      }
      else if( char_stream[pos] == '\'' && char_stream[pos+1] == 'h' ) {
         if (pos + 2 >= size) { printf("Expected hex literal, got EOF\n"); exit(3); }
         int len = take_while(ishex, &char_stream[pos+2]);
         char *s = string_stash(&char_stream[pos+2], len);
         last_token = tok_append(last_token, & (Token) { .kind = Tk_BaseHex, .val.str = s, .line_num = line_num, .next = NULL });
         pos += len+2;
      }
      else if( char_stream[pos] == '\'' && char_stream[pos+1] == 'd' ) {
         if (pos + 2 >= size) { printf("Expected dec literal, got EOF\n"); exit(3); }
         int len = take_while(isdec, &char_stream[pos+2]);
         char *s = string_stash(&char_stream[pos+2], len);
         last_token = tok_append(last_token, & (Token) { .kind = Tk_BaseDec, .val.str = s, .line_num = line_num, .next = NULL });
         pos += len+2;
      }
      else if( char_stream[pos] == '\'' && char_stream[pos+1] == 'o' ) {
         if (pos + 2 >= size) { printf("Expected oct literal, got EOF\n"); exit(3); }
         int len = take_while(isoct, &char_stream[pos+2]);
         char *s = string_stash(&char_stream[pos+2], len);
         last_token = tok_append(last_token, & (Token) { .kind = Tk_BaseOct, .val.str = s, .line_num = line_num, .next = NULL });
         pos += len+2;
      }
      else if( char_stream[pos] == '\'' && char_stream[pos+1] == 'b' ) {
         if (pos + 2 >= size) { printf("Expected bin literal, got EOF\n"); exit(3); }
         int len = take_while(isbin, &char_stream[pos+2]);
         char *s = string_stash(&char_stream[pos+2], len);
         last_token = tok_append(last_token, & (Token) { .kind = Tk_BaseBin, .val.str = s, .line_num = line_num, .next = NULL });
         pos += len+2;
      }
      else if(char_stream[pos] == '/' && char_stream[pos+1] == '/')  { pos +=take_while(isnotnewline, &char_stream[pos]); }
      else if(char_stream[pos] == '(' && char_stream[pos+1] == '*')  { pos +=take_while(isnotstarparen, &char_stream[pos])+1; }
      else if(char_stream[pos] == '`')  { pos +=take_while(isnotnewline, &char_stream[pos]); }
      else if(char_stream[pos] == '.')  { last_token = tok_append(last_token, & (Token) { .kind = Tk_Dot,      .line_num = line_num, .next = NULL }); ++pos; }
      else if(char_stream[pos] == '+')  { last_token = tok_append(last_token, & (Token) { .kind = Tk_Op_Plus,  .line_num = line_num, .next = NULL }); ++pos; }
      else if(char_stream[pos] == '-')  { last_token = tok_append(last_token, & (Token) { .kind = Tk_Op_Minus, .line_num = line_num, .next = NULL }); ++pos; }
      else if(char_stream[pos] == '*')  { last_token = tok_append(last_token, & (Token) { .kind = Tk_Op_Mul,   .line_num = line_num, .next = NULL }); ++pos; }
      else if(char_stream[pos] == '/')  { last_token = tok_append(last_token, & (Token) { .kind = Tk_Op_Div,   .line_num = line_num, .next = NULL }); ++pos; }
      else if(char_stream[pos] == '(')  { last_token = tok_append(last_token, & (Token) { .kind = Tk_LParen,   .line_num = line_num, .next = NULL }); ++pos; }
      else if(char_stream[pos] == ')')  { last_token = tok_append(last_token, & (Token) { .kind = Tk_RParen,   .line_num = line_num, .next = NULL }); ++pos; }
      else if(char_stream[pos] == '{')  { last_token = tok_append(last_token, & (Token) { .kind = Tk_LBrace,   .line_num = line_num, .next = NULL }); ++pos; }
      else if(char_stream[pos] == '}')  { last_token = tok_append(last_token, & (Token) { .kind = Tk_RBrace,   .line_num = line_num, .next = NULL }); ++pos; }
      else if(char_stream[pos] == '[')  { last_token = tok_append(last_token, & (Token) { .kind = Tk_LBracket, .line_num = line_num, .next = NULL }); ++pos; }
      else if(char_stream[pos] == ']')  { last_token = tok_append(last_token, & (Token) { .kind = Tk_RBracket, .line_num = line_num, .next = NULL }); ++pos; }
      else if(char_stream[pos] == ';')  { last_token = tok_append(last_token, & (Token) { .kind = Tk_Semi,     .line_num = line_num, .next = NULL }); ++pos; }
      else if(char_stream[pos] == ':')  { last_token = tok_append(last_token, & (Token) { .kind = Tk_Colon,    .line_num = line_num, .next = NULL }); ++pos; }
      else if(char_stream[pos] == ',')  { last_token = tok_append(last_token, & (Token) { .kind = Tk_Comma,    .line_num = line_num, .next = NULL }); ++pos; }
      else if(char_stream[pos] == '#')  { last_token = tok_append(last_token, & (Token) { .kind = Tk_Hash,     .line_num = line_num, .next = NULL }); ++pos; }
      else if(char_stream[pos] == ' ')  { ++pos; }
      else if(char_stream[pos] == '\n') { ++pos; ++line_num; }
      else { printf("Unexpected token at line %d: %c\n", line_num, char_stream[pos]); exit(1); }
   }

   last_token = tok_append(last_token, & (Token) { .kind = Tk_EOF, .line_num = line_num });

   return first_token->next;
}

Token * tokenize_file (const char * filepath) {
   FILE * fp;
   if(!(fp = fopen(filepath, "r"))) { printf("Failed to open file: %s\nBye!\n", filepath); exit(1); }

   fseek(fp, 0, SEEK_END); long f_size = ftell(fp); rewind(fp);
   printf("File size: %ld bytes\n", f_size);

   char * contents = my_malloc(f_size);

   fread(contents, 1, f_size, fp);
   fclose(fp);

   Token * tok_list = tokenize(contents, f_size);
   //pp_tokens(tok_list);

   free(contents);
   return tok_list;
}


Token * expect(Token * tk, enum token_e expectation) {
   if( tk->kind == expectation) { return tk->next; }
   else {
      printf("At line %d: expected token: '%s', and got: '%s'\n", tk->line_num, tk_print[expectation], tk_print[tk->kind]);
      exit(3);
   }
}

Token * parse_wire_decl(Token * tk, struct Wire_Decl * nw) {
   tk = tk->next; // Assumes wire|input|output|inout is checked for one level above
   if(tk->kind == Tk_LBracket) {
      tk = tk->next;
      expect(tk, Tk_Literal);  nw->hi = tk->val.val; tk = tk->next;
      expect(tk, Tk_Colon);    tk = tk->next;
      expect(tk, Tk_Literal);  nw->lo = tk->val.val; tk = tk->next;
      expect(tk, Tk_RBracket); tk = tk->next;
      nw->is_bus = 1;
   } else nw->hi = nw->lo = nw->is_bus = 0;
   expect(tk, Tk_Ident);
   nw->name = tk->val.str; tk = tk->next;
   expect(tk, Tk_Semi); tk = tk->next;
   return tk;
}

Token * parse_expr(Token * tk, struct Expr * e) {

   switch(tk->kind) {
      case Tk_Ident:
         if( tk->next->kind == Tk_LBracket) {
            struct Idx * new_idx = my_malloc(sizeof(struct Idx));
            e->kind = Expr_Idx;
            new_idx->name = tk->val.str; tk = tk->next;
            expect(tk, Tk_LBracket); tk = tk->next;
            expect(tk, Tk_Literal);
            new_idx->idx = tk->val.val; tk = tk->next;
            expect(tk, Tk_RBracket); tk = tk->next;
            e->expr.idx = new_idx;
            break;
         } else {
            e->kind = Expr_Ident;
            e->expr.ident = tk->val.str;
            tk = tk->next;
         }
         break;
      case Tk_Literal:
         e->kind = Expr_Literal;
         struct Literal * lit = my_malloc(sizeof(struct Literal));
         if( tk->next->kind == Tk_BaseHex || 
             tk->next->kind == Tk_BaseDec || 
             tk->next->kind == Tk_BaseOct ||
             tk->next->kind == Tk_BaseBin ) {
            lit->width = tk->val.val; tk = tk->next;
            lit->lit   = tk->val.str; tk = tk->next;
         } else {
            lit->width = 32;
            char * lit_str = my_malloc(12);
            sprintf(lit_str, "%d", tk->val.val);
            lit->lit   = lit_str; tk = tk->next;
         }
         e->expr.literal = lit;
         break;
      case Tk_LBrace:
         e->kind = Expr_Concat;
         tk = tk->next;

         struct Expr * fst = my_malloc(sizeof(struct Expr));
         struct Expr * lst = fst;
         lst->next = NULL;

         while(tk->kind != Tk_RBrace) {
            struct Expr * new_expr = my_malloc(sizeof(struct Expr));
            tk = parse_expr(tk, new_expr);

            lst -> next = new_expr;
            lst = new_expr;

            if(tk->kind != Tk_Comma) break;
            else tk = tk->next;
         }

         e->expr.concat = fst->next;

         expect(tk, Tk_RBrace); tk = tk->next;
         break;

      default: printf("Expected literal|ident|lbrace at line %d, but got: %s\n", tk->line_num, tk_print[tk->kind]);
   }
   return tk;
}

Token * parse_module_inst(Token * tk, struct Module_Inst * mi) {
   // token is 'ident' here
   mi->type = tk->val.str; tk = tk->next;
   mi->params = NULL;
   mi->conns = NULL;

   if(tk->kind == Tk_Hash) {
      tk = tk->next;
      expect(tk, Tk_LParen); tk = tk->next;
      
      while(tk->kind != Tk_RParen) {
         struct Bind * new_bind = my_malloc(sizeof(struct Bind));
         struct Expr * new_expr = my_malloc(sizeof(struct Expr));

         expect(tk, Tk_Dot);    tk = tk->next;
         expect(tk, Tk_Ident);
         new_bind->name = tk->val.str; tk = tk->next;

         expect(tk, Tk_LParen); tk = tk->next;
         tk = parse_expr(tk, new_expr);
         new_bind->expr = new_expr;
         new_bind->next = mi->params;
         mi->params = new_bind;
         expect(tk, Tk_RParen); tk = tk->next;

         if(tk->kind == Tk_Comma) tk = tk->next; else break;
      }
      expect(tk, Tk_RParen); tk = tk->next;
   }

   expect(tk, Tk_Ident);
   mi->name = tk->val.str; tk = tk->next;

   expect(tk, Tk_LParen); tk = tk->next;

   while(tk->kind != Tk_RParen) {
      struct Bind * new_bind = my_malloc(sizeof(struct Bind));
      struct Expr * new_expr = my_malloc(sizeof(struct Expr));

      expect(tk, Tk_Dot); tk = tk->next;
      expect(tk, Tk_Ident);
      new_bind->name = tk->val.str; tk = tk->next;

      expect(tk, Tk_LParen); tk = tk->next;
      tk = parse_expr(tk, new_expr);
      new_bind->expr = new_expr;
      new_bind->next = mi->conns;
      mi->conns = new_bind;
      expect(tk, Tk_RParen); tk = tk->next;

      if(tk->kind == Tk_Comma) tk = tk->next; else break;
   }

   expect(tk, Tk_RParen); tk = tk->next;
   expect(tk, Tk_Semi); tk = tk->next;
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
   expect(tk, Tk_Kw_module); tk = tk->next;
   expect(tk, Tk_Ident);

   md->name = tk->val.str; tk = tk->next;
   md->io_ports = NULL;
   md->entities = NULL;
   md->next     = NULL;

   if(tk->kind == Tk_LParen) { //Possibly empty io list
      tk = tk->next;
      if(tk->kind != Tk_RParen) // Handle empty list
         while(1) {
            expect(tk, Tk_Ident);
            struct IO_Port * new_port = my_malloc(sizeof(struct IO_Port));
            new_port->name = tk->val.str;
            new_port->next = md->io_ports;
            md->io_ports = new_port;
            tk = tk->next;
            if(tk->kind == Tk_Comma) tk = tk->next;
            else break;
         }
      expect(tk, Tk_RParen); tk = tk->next;
   }
   expect(tk, Tk_Semi); tk = tk->next;

   while(tk->kind != Tk_Kw_endmodule || tk->kind == Tk_EOF) {
      struct Module_Entity * new_entity = my_malloc(sizeof(struct Module_Entity));
      tk = parse_module_entity(tk, new_entity);
      new_entity->next = md->entities;
      md->entities = new_entity;
   }

   if(tk->kind == Tk_EOF) { printf("Expected endmodule, but got EOF!\n"); exit(2); }

   expect(tk, Tk_Kw_endmodule); tk = tk -> next;

   md->entities = reverse_mod_entity(md->entities);

   return tk;
}

struct Module_Def * parse_file(Token * tk) {
   struct Module_Def * first_md = my_malloc(sizeof(struct Module_Def));
   struct Module_Def * current_md = first_md;

   while(tk->kind != Tk_EOF) {
      struct Module_Def * new_md = my_malloc(sizeof(struct Module_Def));
      tk = parse_module_def(tk, new_md);
      current_md->next = new_md;
      current_md = new_md;
   }

   expect(tk, Tk_EOF);

   return first_md->next;
}

int main(int ac, char **av) {
   printf("int: %lu, long: %lu, struct: %lu, enum: %lu\n", sizeof(int), sizeof(long), sizeof(struct token_s), sizeof(enum token_e) );

   for (char **q = av; *q != NULL; ++q) printf(">> %s\n", *q);

   Token * tok_list = tokenize_file(av[1]);

   struct Module_Def * md = parse_file(tok_list);

   pp_modules(md);

   printf("\nAllocated %d bytes for string stash and %d bytes for parse structures\n\n", stash_allocated, malloc_allocated);

   return 0;
}