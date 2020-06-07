#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "pnetlist.h"

char * tk_print[] = { "Tk_Null", "Tk_EOF", "(", ")", "{", "}", "[", "]", "Tk_Ident", "Tk_Literal", "Tk_String", ";", ":", ",", "#", ".",
                      "Tk_BaseHex", "Tk_BaseDec", "Tk_BaseOct", "Tk_BaseBin", "+", "-", "*", "/", "assign", "begin", "end", "endmodule", "inout", "input", "module", "output", "reg", "wire" };

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

Token * tokenize_file (char * filepath) {
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
