#ifndef RBTTY_SCREEN_H
#define RBTTY_SCREEN_H

#include "rbtty_error.h"
#include "rbtty_types.h"
#include <snlsys/snlsys.h>
#include <snlsys/list.h>

struct mem_allocator;
struct sl_wstring;
struct sl_vector;

struct rbtty_text {
  struct sl_wstring* string;
  struct sl_vector* color; /* vector of float[3] */
};

struct rbtty_line {
  struct list_node node;
  struct rbtty_text text;
};

struct rbtty_screen {
  /* Free list of lines */
  struct list_node lines_list_free;
  struct list_node lines_list_stdout;
  struct rbtty_line* lines_list;
  /* tty field */
  struct rbtty_text prompt;
  struct rbtty_line* outbuf;
  struct rbtty_line* cmdbuf;
  /* miscellaneous data */
  struct mem_allocator* allocator;
  /* screen data */
  int lines_count;
  int scroll_id;
  int cursor;
};

extern LOCAL_SYM enum rbtty_error
rbtty_screen_init
  (struct mem_allocator* allocator,
   struct rbtty_screen* screen);

extern LOCAL_SYM enum rbtty_error
rbtty_screen_shutdown
  (struct rbtty_screen* screen);

extern LOCAL_SYM enum rbtty_error
rbtty_screen_storage
  (struct rbtty_screen* screen,
   const int lines_count_per_screen); 

#endif /* RBTTY_SCREEN_H */

