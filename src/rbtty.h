#ifndef RBTTY_H
#define RBTTY_H

#include <snlsys/snlsys.h>

#if defined(RBTTY_SHARED_BUILD)
  #define RBTTY_API EXPORT_SYM
#else
  #define RBTTY_API IMPORT_SYM
#endif

#ifndef NDEBUG
  #define RBTTY_CALL(func) ASSERT(RBTTY_NO_ERROR == func)
#else
  #define RBTTY_CALL(func) func
#endif
#define RBTTY(func) RBTTY_CALL(rbtty_##func)

struct mem_allocator;
struct rb_context;
struct rbi;
struct rbtty;

enum rbtty_error
{
  RBTTY_INVALID_ARGUMENT,
  RBTTY_MEMORY_ERROR,
  RBTTY_NO_ERROR,
  RBTTY_UNKNOWN_ERROR
};

#ifdef __cplusplus
extern "C" {
#endif

RBTTY_API enum rbtty_error
rbtty_create
  (struct rbi* rbi,
   struct rb_context* ctxt,
   struct mem_allocator* allocator, /* May be NULL */
   struct rbtty** tty);

RBTTY_API enum rbtty_error
rbtty_ref_get
  (struct rbtty* tty);

RBTTY_API enum rbtty_error
rbtty_ref_put
  (struct rbtty* tty);

RBTTY_API enum rbtty_error
rbtty_set_font
  (struct rbtty* tty,
   const char* font_path);

RBTTY_API enum rbtty_error
rbtty_set_viewport
  (struct rbtty* tty,
   const int x,
   const int y,
   const int width,
   const int height);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* RBTTY_H */

