#include "rbtty_screen.h"
#include <sl/sl_vector.h>
#include <sl/sl_wstring.h>
#include <string.h>

/*******************************************************************************
 *
 * Helper functions
 *
 ******************************************************************************/
static enum rbtty_error
sl_to_rbtty_error(const enum sl_error sl_err)
{
  enum rbtty_error rbtty_err = RBTTY_NO_ERROR;
  switch(sl_err) {
    case SL_INVALID_ARGUMENT: rbtty_err = RBTTY_INVALID_ARGUMENT; break;
    case SL_MEMORY_ERROR: rbtty_err = RBTTY_MEMORY_ERROR; break;
    case SL_NO_ERROR: rbtty_err = RBTTY_NO_ERROR; break;
    default: rbtty_err = RBTTY_UNKNOWN_ERROR; break;
  }
  return rbtty_err;
}

/*******************************************************************************
 *
 * rbtty_text functions
 *
 ******************************************************************************/
static enum rbtty_error
rbtty_text_init(struct mem_allocator* allocator, struct rbtty_text* text)
{
  enum rbtty_error rbtty_err = RBTTY_NO_ERROR;
  ASSERT(allocator && text);

  #define FUNC(func)                                                           \
    {                                                                          \
      const enum sl_error sl_err = func;                                       \
      if(sl_err != SL_NO_ERROR) {                                              \
        rbtty_err = sl_to_rbtty_error(sl_err);                                 \
        goto error;                                                            \
      }                                                                        \
    } (void) 0
  FUNC(sl_create_wstring(NULL, allocator, &text->string));
  FUNC(sl_create_vector(sizeof(float)*3, 16, allocator, &text->color));
  #undef FUNC

exit:
  return rbtty_err;
error:
  if(text->string) {
    SL(free_wstring(text->string));
    text->string = NULL;
  }
  if(text->color) {
    SL(free_vector(text->color));
    text->color = NULL;
  }
  goto exit;
}

/*******************************************************************************
 *
 * rbtty_screen functions
 *
 ******************************************************************************/
enum rbtty_error
rbtty_screen_init(struct mem_allocator* allocator, struct rbtty_screen* scr)
{
  ASSERT(allocator && scr);

  memset(scr, 0, sizeof(struct rbtty_screen));
  list_init(&scr->line_list_free);
  list_init(&scr->line_list_stdout);
  scr->allocator = allocator;

  return rbtty_text_init(scr->allocator, &scr->prompt);
}

enum rbtty_error
rbtty_screen_shutdown(struct rbtty_screen* scr)
{
  ASSERT(scr);
  ASSERT(0); /* TODO */
  return RBTTY_UNKNOWN_ERROR;
}
