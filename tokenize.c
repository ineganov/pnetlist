#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#define MAX_ALLOC      65536
#define MAX_TMP_ALLOC  1024
#define MAX_TOKENS     65536

enum token_e { Tk_Null = 0, Tk_EOF, Tk_LParen, Tk_RParen, Tk_LBrace, Tk_RBrace, Tk_LBracket, Tk_RBracket, Tk_Ident, Tk_Literal, Tk_Semi, Tk_Colon, Tk_Comma, Tk_Hash,
               Tk_Op_Plus, Tk_Op_Minus, Tk_Op_Mul, Tk_Op_Div,
               Tk_Kw_assign, Tk_Kw_begin, Tk_Kw_end, Tk_Kw_endmodule, Tk_Kw_inout, Tk_Kw_input, Tk_Kw_module, Tk_Kw_output, Tk_Kw_reg, Tk_Kw_wire };

char * tk_print[] = { "Tk_Null", "Tk_EOF", "Tk_LParen", "Tk_RParen", "Tk_LBrace", "Tk_RBrace", "Tk_LBracket", "Tk_RBracket", "Tk_Ident", "Tk_Literal", "Tk_Semi", "Tk_Colon", "Tk_Comma", "Tk_Hash",
                      "Tk_Op_Plus", "Tk_Op_Minus", "Tk_Op_Mul", "Tk_Op_Div",
                      "Tk_Kw_assign", "Tk_Kw_begin", "Tk_Kw_end", "Tk_Kw_endmodule", "Tk_Kw_inout", "Tk_Kw_input", "Tk_Kw_module", "Tk_Kw_output", "Tk_Kw_reg", "Tk_Kw_wire" };

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

void pp_tokens(Token * tok_array, int num_tkn ) {
   for (int i = 0; i < num_tkn; ++i) {
      if (tok_array[i].kind == Tk_Ident)
         printf(" Line %d: %-20s -> \"%s\"\n", tok_array[i].line_num, tk_print[tok_array[i].kind], tok_array[i].val.str );
      else if (tok_array[i].kind == Tk_Literal)
         printf(" Line %d: %-20s -> %d\n", tok_array[i].line_num, tk_print[tok_array[i].kind], tok_array[i].val.val );
      else
         printf(" Line %d: %-20s\n", tok_array[i].line_num, tk_print[tok_array[i].kind] );
   }

   return;
}

char * string_stash(char * src, int len)
{
   static char string_array[MAX_ALLOC];
   static int allocated = 0;

   if (allocated + len < MAX_ALLOC) {
      int old_alloc = allocated;
      for (int i = 0; i < len; ++i) string_array[allocated++] = src[i];
      string_array[allocated++] = '\0';
      printf(" -- allocated %d bytes\n", len + 1);
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

int isidentsym(int c) { return c == '_' || isalnum(c);  }
int isnotspace(int c) { return ! isspace(c); }

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
         char *s = string_stash(&char_stream[pos], len);
         tok_array[num_tkn++] = (Token) { .kind = Tk_Ident, .val.str = s, .line_num = line_num };
         pos += len+1; //Final space needs to be eaten
      }
      else if( isnumber(char_stream[pos]) ) {
         int len = take_while(isnumber, &char_stream[pos]);
         char *s = string_stash_temp(&char_stream[pos], len);
         tok_array[num_tkn++] = (Token) { .kind = Tk_Literal, .val.val = atoi(s), .line_num = line_num };
         pos += len;
      }
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

   char * contents = malloc(f_size);

   fread(contents, 1, f_size, fp);
   fclose(fp);

   int tkns = tokenize(contents, f_size, tok_array);
   pp_tokens(tok_array, tkns);

   free(contents);
   return;
}

int main(int ac, char **av) {
   printf("int: %lu, long: %lu, struct: %lu, enum: %lu\n", sizeof(int), sizeof(long), sizeof(struct token_s), sizeof(enum token_e) );

   for (char **q = av; *q != NULL; ++q) printf(">> %s\n", *q);

   tokenize_file(av[1]);

}