#include "rbtty.h"
#include <font_rsrc.h>
#include <lp/lp.h>
#include <lp/lp_font.h>
#include <lp/lp_printer.h>
#include <rb/rbi.h>
#include <snlsys/math.h>
#include <snlsys/mem_allocator.h>
#include <snlsys/ref_count.h>
#include <limits.h>
#include <string.h>

#define TO_UPPER_font FONT
#define TO_UPPER_lp LP
#define TO_UPPER( x ) TO_UPPER_ ## x

struct rbtty {
  struct ref ref;
  struct mem_allocator* allocator;

  /* Render backend */
  struct rbi* rbi;
  struct rb_context* rb_ctxt;

  /* Line printer */
  struct lp* lp;
  struct lp_font* font;
  struct lp_printer* printer;

  /* Resource */
  struct font_system* font_sys;
  struct font_rsrc* font_rsrc;
};

/*******************************************************************************
 *
 * Helper data structure
 *
 ******************************************************************************/
static const wchar_t rbtty_charset[] =
  L"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
  L" &~\"#'{([-|`_\\^@)]=}+$%*,?;.:/!<>";

#define RBTTY_CHARSET_LEN (sizeof(rbtty_charset)/sizeof(wchar_t)-1)/* -1 = \0 */

/*******************************************************************************
 *
 * Helper functions
 *
 ******************************************************************************/
static enum rbtty_error
lp_to_rbtty_error(const enum lp_error lp_err)
{
  enum rbtty_error rbtty_err = RBTTY_NO_ERROR;
  switch(lp_err) {
    case LP_INVALID_ARGUMENT: rbtty_err = RBTTY_INVALID_ARGUMENT; break;
    case LP_MEMORY_ERROR: rbtty_err = RBTTY_MEMORY_ERROR; break;
    case LP_NO_ERROR: rbtty_err = RBTTY_NO_ERROR; break;
    default: rbtty_err = RBTTY_UNKNOWN_ERROR;  break;
  }
  return rbtty_err;
}

static enum rbtty_error
font_to_rbtty_error(const enum font_error font_err)
{
  enum rbtty_error rbtty_err = RBTTY_NO_ERROR;
  switch(font_err) {
    case FONT_INVALID_ARGUMENT: rbtty_err = RBTTY_INVALID_ARGUMENT; break;
    case FONT_MEMORY_ERROR: rbtty_err = RBTTY_MEMORY_ERROR; break;
    case FONT_NO_ERROR: rbtty_err = RBTTY_NO_ERROR; break;
    default: rbtty_err = RBTTY_UNKNOWN_ERROR; break;
  }
  return rbtty_err;
}

static void
release_rbtty(struct ref* ref)
{
  struct rbtty* tty = NULL;
  ASSERT(ref);

  tty = CONTAINER_OF(ref, struct rbtty, ref);

  if(tty->lp)
    LP(ref_put(tty->lp));
  if(tty->font)
    LP(font_ref_put(tty->font));
  if(tty->printer)
    LP(printer_ref_put(tty->printer));

  if(tty->font_sys)
    FONT(system_ref_put(tty->font_sys));
  if(tty->font_rsrc)
    FONT(rsrc_ref_put(tty->font_rsrc));

  MEM_FREE(tty->allocator, tty);
}

/*******************************************************************************
 *
 * rbtty functions
 *
 ******************************************************************************/
enum rbtty_error
rbtty_create
  (struct rbi* rbi,
   struct rb_context* ctxt,
   struct mem_allocator* allocator,
   struct rbtty** out_tty)
{
  struct rbtty* tty = NULL;
  struct mem_allocator* alloc = allocator ? allocator : &mem_default_allocator;
  enum rbtty_error rbtty_err = RBTTY_NO_ERROR;

  if(UNLIKELY(!rbi || !out_tty)) {
    rbtty_err = RBTTY_INVALID_ARGUMENT;
    goto error;
  }

  #define RB_FUNC(func_name, ...)                                              \
    if(!rbi->func_name) {                                                      \
      rbtty_err = RBTTY_INVALID_ARGUMENT;                                      \
      goto error;                                                              \
    }
  #include <rb/rb_func.h>
  #undef RB_FUNC

  tty = MEM_CALLOC(alloc, 1, sizeof(struct rbtty));
  if(!tty) {
    rbtty_err = RBTTY_MEMORY_ERROR;
    goto error;
  }
  ref_init(&tty->ref);
  tty->allocator = alloc;
  tty->rbi = rbi;
  tty->rb_ctxt = ctxt;

  #define FUNC(prefix, func)                                                   \
    {                                                                          \
      const enum prefix ## _error err = prefix ## _ ## func;                   \
      if(err != CONCAT( TO_UPPER(prefix), _NO_ERROR )) {                       \
        rbtty_err = prefix ## _to_rbtty_error(err);                            \
        goto error;                                                            \
      }                                                                        \
    } (void) 0
  FUNC(lp, create(tty->rbi, tty->rb_ctxt, tty->allocator, &tty->lp));
  FUNC(lp, font_create(tty->lp, &tty->font));
  FUNC(lp, printer_create(tty->lp, &tty->printer));
  FUNC(lp, printer_set_font(tty->printer, tty->font));
  FUNC(font, system_create(tty->allocator, &tty->font_sys));
  FUNC(font, rsrc_create(tty->font_sys, NULL, &tty->font_rsrc));
  #undef FUNC

exit:
  if(out_tty)
    *out_tty = tty;
  return rbtty_err;
error:
  if(tty) {
    RBTTY(ref_put(tty));
    tty = NULL;
  }
  goto exit;
}

enum rbtty_error
rbtty_ref_get(struct rbtty* tty)
{
  if(UNLIKELY(!tty))
    return RBTTY_INVALID_ARGUMENT;
  ref_get(&tty->ref);
  return RBTTY_NO_ERROR;
}

enum rbtty_error
rbtty_ref_put(struct rbtty* tty)
{
  if(UNLIKELY(!tty))
    return RBTTY_INVALID_ARGUMENT;
  ref_put(&tty->ref, release_rbtty);
  return RBTTY_NO_ERROR;
}

enum rbtty_error
rbtty_set_font(struct rbtty* tty, const char* font_path)
{
  struct lp_font_glyph_desc lp_font_glyph_desc_list[RBTTY_CHARSET_LEN];
  unsigned char* glyph_bitmap_list[RBTTY_CHARSET_LEN];
  enum rbtty_error rbtty_err = RBTTY_NO_ERROR;
  int glyph_min_width = INT_MAX;
  int line_space = 0;
  memset(lp_font_glyph_desc_list, 0, sizeof(lp_font_glyph_desc_list));
  memset(glyph_bitmap_list, 0, sizeof(glyph_bitmap_list));

  if(UNLIKELY(!tty || !font_path))
    return RBTTY_INVALID_ARGUMENT;

  #define FUNC(prefix, func)                                                   \
    {                                                                          \
      const enum prefix ## _error err = prefix ## _ ## func;                   \
      if(err != CONCAT( TO_UPPER(prefix), _NO_ERROR )) {                       \
        rbtty_err = prefix ## _to_rbtty_error(err);                            \
        goto error;                                                            \
      }                                                                        \
    } (void) 0

  FUNC(font, rsrc_load(tty->font_rsrc, font_path));

  FOR_EACH(size_t, i, 0, RBTTY_CHARSET_LEN) {
    struct font_glyph_desc font_glyph_desc;
    struct font_glyph* font_glyph = NULL;
    int width = 0, height = 0, Bpp = 0;

    FUNC(font, rsrc_get_glyph(tty->font_rsrc, rbtty_charset[i], &font_glyph));
    FUNC(font, glyph_get_desc(font_glyph, &font_glyph_desc));
    glyph_min_width = MIN(font_glyph_desc.width, glyph_min_width);
    lp_font_glyph_desc_list[i].width = font_glyph_desc.width;
    lp_font_glyph_desc_list[i].character = font_glyph_desc.character;
    lp_font_glyph_desc_list[i].bitmap_left = font_glyph_desc.bbox.x_min;
    lp_font_glyph_desc_list[i].bitmap_top = font_glyph_desc.bbox.y_min;

    FUNC(font, glyph_get_bitmap(font_glyph, true, &width, &height, &Bpp, NULL));
    if(width && height) {
      const size_t bmp_size = (size_t)(width*height);
      glyph_bitmap_list[i] = MEM_CALLOC(tty->allocator, bmp_size, (size_t)Bpp);
      if(UNLIKELY(!glyph_bitmap_list[i])) {
        rbtty_err = RBTTY_MEMORY_ERROR;
        goto error;
      }
      FUNC(font, glyph_get_bitmap
        (font_glyph, true, &width, &height, &Bpp, glyph_bitmap_list[i]));
    }

    lp_font_glyph_desc_list[i].bitmap.width = width;
    lp_font_glyph_desc_list[i].bitmap.height = height;
    lp_font_glyph_desc_list[i].bitmap.bytes_per_pixel = Bpp;
    lp_font_glyph_desc_list[i].bitmap.buffer = glyph_bitmap_list[i];

    FONT(glyph_ref_put(font_glyph));
  }

  FONT(rsrc_get_line_space(tty->font_rsrc, &line_space));
  FUNC(lp, font_set_data
    (tty->font, line_space, (int)RBTTY_CHARSET_LEN, lp_font_glyph_desc_list));

  #undef FUNC
exit:
  FOR_EACH(size_t, i, 0, RBTTY_CHARSET_LEN) {
    if(glyph_bitmap_list[i]) {
      MEM_FREE(tty->allocator, glyph_bitmap_list);
    }
  }
  return rbtty_err;
error:
  goto exit;
}

enum rbtty_error
rbtty_set_viewport
  (struct rbtty* tty,
   const int x,
   const int y,
   const int width,
   const int height)
{
  if(UNLIKELY(!tty || width < 0 || height < 0))
    return RBTTY_INVALID_ARGUMENT;
  return lp_to_rbtty_error
    (lp_printer_set_viewport(tty->printer, x, y, width, height));
}

