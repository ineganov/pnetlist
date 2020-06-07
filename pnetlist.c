#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "pnetlist.h"

/* *****  *****  *****  *****  *****  *****  *****  *****  *****  *****  *****  *****  *****  *****  ***** */
/* alloc */ #define MAX_IDENT_LENGTH  4096
/* alloc */ static int stash_allocated = 0;
/* alloc */ static int malloc_allocated = 0;
/* alloc */ 
/* alloc */ void * my_malloc(size_t  size) {
/* alloc */    malloc_allocated += size;
/* alloc */    return malloc(size);
/* alloc */ }
/* alloc */ 
/* alloc */ char * string_stash(char * src, int len)
/* alloc */ {
/* alloc */    char * new_string = my_malloc((len+1)*sizeof(char));  stash_allocated += len + 1;
/* alloc */    for (int i = 0; i < len; ++i) new_string[i] = src[i]; // strncpy, anyone?
/* alloc */    new_string[len] = '\0';
/* alloc */ 
/* alloc */    return new_string;
/* alloc */ }
/* alloc */ 
/* alloc */ char * string_stash_temp(char * src, int len)
/* alloc */ {
/* alloc */    static char string_temp[MAX_IDENT_LENGTH];
/* alloc */ 
/* alloc */    if (len < MAX_IDENT_LENGTH) {
/* alloc */       for (int i = 0; i < len; ++i) string_temp[i] = src[i];
/* alloc */       string_temp[len] = '\0';
/* alloc */       return string_temp;
/* alloc */    }
/* alloc */    else {
/* alloc */       printf("Oh noes! Attempt to allocate above limit (%d bytes) failed!\n", MAX_IDENT_LENGTH);
/* alloc */       exit(2);
/* alloc */    }
/* alloc */ }
/* *****  *****  *****  *****  *****  *****  *****  *****  *****  *****  *****  *****  *****  *****  ***** */


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


int main(int ac, char **av) {
   printf("int: %lu, long: %lu, struct: %lu, enum: %lu\n", sizeof(int), sizeof(long), sizeof(struct token_s), sizeof(enum token_e) );

   for (char **q = av; *q != NULL; ++q) printf(">> %s\n", *q);

   Token * tok_list = tokenize_file(av[1]);

   struct Module_Def * md = parse_file(tok_list);

   pp_modules(md);

   printf("\nAllocated %d bytes for string stash and %d bytes for parse structures\n\n", stash_allocated, malloc_allocated);

   return 0;
}
