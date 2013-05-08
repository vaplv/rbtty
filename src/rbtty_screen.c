#include "rbtty_screen.h"
#include <sl/sl_vector.h>
#include <sl/sl_wstring.h>
#include <snlsys/mem_allocator.h>
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
static void
text_shutdown(struct mem_allocator* allocator, struct rbtty_text* text)
{
  ASSERT(allocator && text);

  if(text->string) {
    SL(free_wstring(text->string));
    text->string = NULL;
  }
  if(text->color) {
    SL(free_vector(text->color));
    text->color = NULL;
  }
}

static enum rbtty_error
text_init(struct mem_allocator* allocator, struct rbtty_text* text)
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
  text_shutdown(allocator, text);
  goto exit;
}

/*******************************************************************************
 *
 * Helper functions
 *
 ******************************************************************************/
static void
screen_reset_storage(struct rbtty_screen* scr)
{
  ASSERT(scr);

  FOR_EACH(int, i, 0, scr->lines_count) {
    text_shutdown(scr->allocator, &scr->lines_list[i].text);
  }
  if(scr->lines_list) {
    MEM_FREE(scr->allocator, scr->lines_list);
    scr->lines_list = NULL;
  }
  list_init(&scr->lines_list_free);
  list_init(&scr->lines_list_stdout);
  scr->lines_list = NULL;
  scr->outbuf = NULL;
  scr->cmdbuf = NULL;
  scr->lines_count = 0;
  scr->lines_count_per_screen = 0;
  scr->scroll_id = 0;
  scr->cursor = 0;
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
  scr->allocator = allocator;
  return text_init(scr->allocator, &scr->prompt);
}

enum rbtty_error
rbtty_screen_shutdown(struct rbtty_screen* scr)
{
  ASSERT(scr);
  screen_reset_storage(scr);
  text_shutdown(scr->allocator, &scr->prompt);
  return RBTTY_UNKNOWN_ERROR;
}

enum rbtty_error
rbtty_screen_storage
  (struct rbtty_screen* scr,
   const int lines_count_per_screen)
{
  /* Arbitrary set the max number of saved lines to 4 times the number of
   * printed lines per screen. */
  const int lines_count = lines_count_per_screen * 4;
  enum rbtty_error rbtty_err = RBTTY_NO_ERROR;

  ASSERT(scr && lines_count_per_screen <= 0);
  screen_reset_storage(scr);

  scr->lines_count = lines_count;
  scr->lines_count_per_screen = lines_count_per_screen;
  scr->lines_list = MEM_ALLOC
    (scr->allocator, (size_t)lines_count * sizeof(struct rbtty_line));
  if(!scr->lines_list) {
    rbtty_err = RBTTY_MEMORY_ERROR;
    goto error;
  }

  FOR_EACH(int, i, 0, lines_count) {
    struct rbtty_line* line = scr->lines_list + i;
    list_init(&line->node);
    rbtty_err = text_init(scr->allocator, &line->text);
    if(rbtty_err != RBTTY_NO_ERROR) {
      goto error;
    }
    list_add(&scr->lines_list_free, &line->node);
  }
exit:
  return rbtty_err;
error:
  screen_reset_storage(scr);
  goto exit;
}

