#include "rbtty_screen.h"
#include <sl/sl_vector.h>
#include <sl/sl_wstring.h>
#include <snlsys/math.h>
#include <snlsys/mem_allocator.h>
#include <limits.h>
#include <string.h>

static enum rbtty_error
sl_to_rbtty_error(const enum sl_error sl_err);

/*******************************************************************************
 *
 * rbtty_text functions
 *
 ******************************************************************************/
static void
text_shutdown(struct mem_allocator* allocator, struct rbtty_text* text)
{
  (void)allocator;
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

static FINLINE void
text_clear(struct rbtty_text* text)
{
  ASSERT(text);
  SL(clear_wstring(text->string));
  SL(clear_vector(text->color));
}

static void
text_copy(struct rbtty_text* dst, const struct rbtty_text* src)
{
  const wchar_t* src_cstr = NULL;
  void* src_colors = NULL;
  void* dst_colors = NULL;
  size_t colors_count = 0;
  size_t color_size = 0;
  ASSERT(dst && src);

  /* Copy text */
  SL(wstring_get(src->string, &src_cstr));
  SL(wstring_set(dst->string, src_cstr));

  /* Copy color */
  SL(vector_buffer(src->color, &colors_count, &color_size, NULL, &src_colors));
  SL(clear_vector(dst->color));
  SL(vector_resize(dst->color, colors_count, NULL));
  SL(vector_buffer(dst->color, NULL, NULL, NULL, &dst_colors));
  memcpy(dst_colors, src_colors, colors_count * color_size);
}

/*******************************************************************************
 *
 * Helper functions
 *
 ******************************************************************************/
enum rbtty_error
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

static void
screen_new_buf
  (struct rbtty_screen* scr,
   const enum rbtty_output output)
{
  struct rbtty_line** line = NULL;
  struct list_node* node = NULL;
  ASSERT(scr);

  if(!scr->lines_list) /* No storage defined => no buf available */
    return;

  /* Retrieve the current line associated to the buf */
  if(output == RBTTY_STDOUT) {
    line = &scr->outbuf;
  } else { ASSERT(output == RBTTY_CMDOUT);
    size_t len = 0;
    line = &scr->cmdbuf;
    /* Translate the cursor after the prompt */
    SL(wstring_length(scr->prompt.string, &len));
    ASSERT(len <= INT_MAX);
    scr->cursor = (int)len;
  }

  /* Flush the retrieved line to the stdout */
  if(*line) {
    list_add(&scr->lines_list_stdout, &(*line)->node);
    *line = NULL;
  }

  /* Associate a new line to the buf */
  if(is_list_empty(&scr->lines_list_free)) {
    node = list_tail(&scr->lines_list_stdout);
  } else {
    node = list_head(&scr->lines_list_free);
  }
  list_del(node);
  (*line) = CONTAINER_OF(node, struct rbtty_line, node);
  text_clear(&(*line)->text);

  /* Init the cmdbuf with prompt text */
  if(output == RBTTY_CMDOUT && scr->cursor != 0 /* Prompt exist ? */) {
    text_copy(&(*line)->text, &scr->prompt);
  }
}

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
   const int lines_count)
{
  enum rbtty_error rbtty_err = RBTTY_NO_ERROR;

  ASSERT(scr && lines_count <= 0);
  screen_reset_storage(scr);

  scr->lines_count = lines_count;
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

enum rbtty_error
rbtty_screen_translate_cursor(struct rbtty_screen* scr, const int trans)
{
  ASSERT(scr);

  if(trans == 0)
    return RBTTY_NO_ERROR;

  if(trans < 0) {
    size_t prompt_len = 0;
    const int x = -trans;

    SL(wstring_length(scr->prompt.string, &prompt_len));
    ASSERT(scr->cursor >= (int)prompt_len);
    scr->cursor -= MIN(x, scr->cursor - (int)prompt_len);
  } else {
    size_t len = 0;
    const int x = trans;

    SL(wstring_length(scr->cmdbuf->text.string, &len));
    ASSERT((int)len >= scr->cursor);
    scr->cursor += MIN(x, (int)len - scr->cursor);
  }

  return RBTTY_NO_ERROR;
}

enum rbtty_error
rbtty_screen_print_wstring
  (struct rbtty_screen* scr,
   const enum rbtty_output output,
   const wchar_t* str,
   const float color[3])
{
  size_t len = 0;
  enum rbtty_error rbtty_err = RBTTY_NO_ERROR;

  ASSERT(scr && str && color);

  len = wcslen(str);

  #define CALL(func)                                                           \
    {                                                                          \
      const enum sl_error sl_err = func;                                       \
      if(sl_err != SL_NO_ERROR) {                                              \
        rbtty_err = sl_to_rbtty_error(sl_err);                                 \
        goto error;                                                            \
      }                                                                        \
    } (void) 0
  if(output == RBTTY_PROMPT) {
    size_t plen = 0;

    SL(vector_buffer(scr->prompt.color, &plen, NULL, NULL, NULL));
    CALL(sl_wstring_append(scr->prompt.string, str));
    CALL(sl_vector_push_back_n(scr->prompt.color, len, color));

    if(scr->cmdbuf) {
      CALL(sl_wstring_insert(scr->cmdbuf->text.string, plen, str));
      CALL(sl_vector_insert_n(scr->cmdbuf->text.color, plen, len, color));
    }
    scr->cursor += (int)len;

  } else if(scr->lines_count > 0) {
    const size_t cursor = (size_t)scr->cursor;
    struct rbtty_line* line = NULL;

    if(output == RBTTY_CMDOUT) {
      line = scr->cmdbuf;
      CALL(sl_wstring_insert(line->text.string, cursor, str));
      CALL(sl_vector_insert_n(line->text.color, cursor, len, color));
      scr->cursor += (int)len;

    } else { ASSERT(output == RBTTY_STDOUT);
      wchar_t* tkn = NULL;
      wchar_t* str_end = NULL;

      if(len + 1 /* +1 <=> \0 */ > sizeof(scr->scratch)/sizeof(wchar_t)) {
        rbtty_err = RBTTY_MEMORY_ERROR;
        goto error;
      }
      wcscpy(scr->scratch, str);
      tkn = scr->scratch;
      str_end = scr->scratch + len;
      while((uintptr_t)tkn < (uintptr_t)str_end) {
        wchar_t* tkn_end = wcschr(tkn, L'\n');
        if(tkn_end) {
          *tkn_end = L'\0';
        }
        const size_t tkn_len = wcslen(tkn);
        line = scr->outbuf;
        CALL(sl_wstring_append(line->text.string, tkn));
        CALL(sl_vector_push_back_n(line->text.color, tkn_len, color));
        tkn += tkn_len + 1; /* +1 <=> \0 */
        if(tkn_end) {
          screen_new_buf(scr, RBTTY_STDOUT);
        }
      }
    }
  }
  #undef CALL

exit:
  return rbtty_err;
error:
  goto exit;
}
