#include <cogl/cogl.h>

#include "test-utils.h"

#define RED 0
#define GREEN 1
#define BLUE 2

typedef struct _TestState
{
  int fb_width;
  int fb_height;
} TestState;

static void
check_quadrant (TestState *state,
                int qx,
                int qy,
                uint32_t expected_rgba)
{
  /* The quadrants are all stuffed into the top right corner of the
     framebuffer */
  int x = state->fb_width * qx / 4 + state->fb_width / 2;
  int y = state->fb_height * qy / 4;
  int width = state->fb_width / 4;
  int height = state->fb_height / 4;

  /* Subtract a two-pixel gap around the edges to allow some rounding
     differences */
  x += 2;
  y += 2;
  width -= 4;
  height -= 4;

  test_utils_check_region (test_fb, x, y, width, height, expected_rgba);
}

static void
test_paint (TestState *state)
{
  CoglTexture2D *tex_2d;
  CoglTexture *tex;
  CoglFramebuffer *offscreen;
  CoglPipeline *color;
  CoglPipeline *textured;

  tex_2d = cogl_texture_2d_new_with_size (test_ctx,
                                          state->fb_width,
                                          state->fb_height);
  tex = tex_2d;

  offscreen = cogl_offscreen_new_with_texture (tex);

  /* Set a scale and translate transform on the window framebuffer
   * before switching to the offscreen framebuffer so we can verify it
   * gets restored when we switch back
   *
   * The test is going to draw a grid of 4 colors to a texture which
   * we subsequently draw to the window with a fullscreen rectangle.
   * This transform will flip the texture left to right, scale it to a
   * quarter of the window size and slide it to the top right of the
   * window.
   */
  cogl_framebuffer_push_matrix (test_fb);
  cogl_framebuffer_translate (test_fb, 0.5, 0.5, 0);
  cogl_framebuffer_scale (test_fb, -0.5, 0.5, 1);

  /* Use something other than the identity matrix for the modelview so we can
   * verify it gets restored when we switch back to test_fb */
  cogl_framebuffer_scale (offscreen, 2, 2, 1);

  color = cogl_pipeline_new (test_ctx);

  /* red, top left */
  cogl_pipeline_set_color4f (color, 1, 0, 0, 1);
  cogl_framebuffer_draw_rectangle (offscreen, color, -0.5, 0.5, 0, 0);
  /* green, top right */
  cogl_pipeline_set_color4f (color, 0, 1, 0, 1);
  cogl_framebuffer_draw_rectangle (offscreen, color, 0, 0.5, 0.5, 0);
  /* blue, bottom left */
  cogl_pipeline_set_color4f (color, 0, 0, 1, 1);
  cogl_framebuffer_draw_rectangle (offscreen, color, -0.5, 0, 0, -0.5);
  /* white, bottom right */
  cogl_pipeline_set_color4f (color, 1, 1, 1, 1);
  cogl_framebuffer_draw_rectangle (offscreen, color, 0, 0, 0.5, -0.5);

  cogl_object_unref (offscreen);

  textured = cogl_pipeline_new (test_ctx);
  cogl_pipeline_set_layer_texture (textured, 0, tex);
  cogl_framebuffer_draw_rectangle (test_fb, textured, -1, 1, 1, -1);
  cogl_object_unref (textured);

  cogl_object_unref (tex_2d);

  cogl_framebuffer_pop_matrix (test_fb);

  /* NB: The texture is drawn flipped horizontally and scaled to fit in the
   * top right corner of the window. */

  /* red, top right */
  check_quadrant (state, 1, 0, 0xff0000ff);
  /* green, top left */
  check_quadrant (state, 0, 0, 0x00ff00ff);
  /* blue, bottom right */
  check_quadrant (state, 1, 1, 0x0000ffff);
  /* white, bottom left */
  check_quadrant (state, 0, 1, 0xffffffff);
}

static void
test_flush (TestState *state)
{
  CoglTexture2D *tex_2d;
  CoglTexture *tex;
  CoglFramebuffer *offscreen;
  int i;

  for (i = 0; i < 3; i++)
    {
      CoglPipeline *red;

      /* This tests that rendering to a framebuffer and then reading back
         the contents of the texture will automatically flush the
         journal */

      tex_2d = cogl_texture_2d_new_with_size (test_ctx,
                                              16, 16); /* width/height */
      tex = tex_2d;

      offscreen = cogl_offscreen_new_with_texture (tex);

      cogl_framebuffer_clear4f (offscreen, COGL_BUFFER_BIT_COLOR,
                                0, 0, 0, 1);

      red = cogl_pipeline_new (test_ctx);
      cogl_pipeline_set_color4f (red, 1, 0, 0, 1);
      cogl_framebuffer_draw_rectangle (offscreen,
                                       red,
                                       -1, -1, 1, 1);
      cogl_object_unref (red);

      if (i == 0)
        /* First time check using read pixels on the offscreen */
        test_utils_check_region (offscreen,
                                 1, 1, 15, 15, 0xff0000ff);
      else if (i == 1)
        {
          uint8_t data[16 * 4 * 16];
          int x, y;

          /* Second time try reading back the texture contents */
          cogl_texture_get_data (tex,
                                 COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                 16 * 4, /* rowstride */
                                 data);

          for (y = 1; y < 15; y++)
            for (x = 1; x < 15; x++)
              test_utils_compare_pixel (data + x * 4 + y * 16 * 4,
                                        0xff0000ff);
        }

      if (i == 2)
        {
          CoglPipeline *textured = cogl_pipeline_new (test_ctx);
          cogl_pipeline_set_layer_texture (textured, 0, tex);
          /* Third time try drawing the texture to the screen */
          cogl_framebuffer_draw_rectangle (test_fb,
                                           textured,
                                           -1, -1, 1, 1);
          cogl_object_unref (textured);
          test_utils_check_region (test_fb,
                                   2, 2, /* x/y */
                                   state->fb_width - 4,
                                   state->fb_height - 4,
                                   0xff0000ff);
        }

      cogl_object_unref (tex_2d);
      cogl_object_unref (offscreen);
    }
}

void
test_offscreen (void)
{
  TestState state;

  state.fb_width = cogl_framebuffer_get_width (test_fb);
  state.fb_height = cogl_framebuffer_get_height (test_fb);

  test_paint (&state);
  test_flush (&state);

  if (cogl_test_verbose ())
    u_print ("OK\n");
}
