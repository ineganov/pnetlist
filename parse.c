#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "pnetlist.h"

extern char ** tk_print;

void pp_expr(struct Expr * e) {
   switch(e->kind) {
      case Expr_Ident:   printf("%s", e->expr.ident); break;
      case Expr_String:  printf("\"%s\"", e->expr.string_lit); break;
      case Expr_Literal: printf("%d'%s",  e->expr.literal->width, e->expr.literal->lit); break;
      case Expr_Idx:     printf("%s[%d]", e->expr.idx->name, e->expr.idx->idx ); break;
      case Expr_Range:   printf("%s[%d:%d]", e->expr.range->name, e->expr.range->hi, e->expr.range->lo ); break;
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

void pp_modules(struct Module_Def * md) {
   for(struct Module_Def * mi = md; mi != NULL; mi = mi->next) {
      
      printf("\nModule %s:\n", mi->name);
      //for(struct IO_Port * iter = mi->io_ports; iter != NULL; iter = iter->next ) printf("  io port: %s\n", iter->name);
      for(struct Module_Entity * iter = mi->entities; iter != NULL; iter = iter->next ) pp_mod_entity(iter);

      struct mod_count ** mod_list = unique_modules(mi);

      printf("\nModule '%s' statistics:\n", mi->name);
      while(*mod_list) {printf("%50s %-4d\n", (*mod_list)->nm, (*mod_list)->cnt ); mod_list++;}

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
      case Tk_Ident: // FIXME: lookahead of 3 is unnecessary, dangerous and plain wrong
         if( tk->next->kind == Tk_LBracket && tk->next->next->next->kind == Tk_RBracket) {
            struct Idx * new_idx = my_malloc(sizeof(struct Idx));
            e->kind = Expr_Idx;
            new_idx->name = tk->val.str; tk = tk->next;
            expect(tk, Tk_LBracket); tk = tk->next;
            expect(tk, Tk_Literal);
            new_idx->idx = tk->val.val; tk = tk->next;
            expect(tk, Tk_RBracket); tk = tk->next;
            e->expr.idx = new_idx;
            break;
         }
         else if( tk->next->kind == Tk_LBracket && tk->next->next->next->kind == Tk_Colon) {
            struct Range * new_range = my_malloc(sizeof(struct Range));
            e->kind = Expr_Range;
            new_range->name = tk->val.str; tk = tk->next;
            expect(tk, Tk_LBracket); tk = tk->next;
            expect(tk, Tk_Literal);
            new_range->hi = tk->val.val; tk = tk->next;
            expect(tk, Tk_Colon); tk = tk->next;
            new_range->lo = tk->val.val; tk = tk->next;
            expect(tk, Tk_RBracket); tk = tk->next;
            e->expr.range = new_range;
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
      case Tk_String:
         e->kind = Expr_String;
         e->expr.string_lit = tk->val.str;
         tk = tk->next;
         break;

      default: printf("Expected literal|ident|lbrace|string at line %d, but got: %s\n", tk->line_num, tk_print[tk->kind]);
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
