#include <cogl/cogl.h>
#include <glib.h>
#include <stdio.h>

CoglColor black;

#define OFFSCREEN_WIDTH 100
#define OFFSCREEN_HEIGHT 100

void
update_offscreen_texture (CoglContext *ctx, CoglTexture *offscreen_texture)
{
  static CoglGLES2Context *gles2_ctx = NULL;
  static CoglGLES2Vtable *vtable = NULL;
  static CoglFramebuffer *offscreen_framebuffer = NULL;
  GError *error = NULL;

  if (gles2_ctx == NULL)
    {
      /* Prepare offscreen framebuffer */
      gles2_ctx = cogl_gles2_context_new (ctx, &error);
      if (!gles2_ctx) {
          g_error ("Failed to create GLES2 context: %s\n", error->message);
      }

      vtable = cogl_gles2_context_get_vtable (gles2_ctx);

      offscreen_framebuffer = cogl_offscreen_new_to_texture (offscreen_texture);
      if (!cogl_framebuffer_push_gles2_context (offscreen_framebuffer,
                                           gles2_ctx,
                                           &error)) {
          g_error ("Failed to push GLES2 context: %s\n", error->message);
      }
    }

  cogl_push_framebuffer (offscreen_framebuffer);

  /* Clear offscreen framebuffer with green */
  vtable->glClearColor(g_random_double (), g_random_double (), g_random_double (), 1.0f);
  vtable->glClear(GL_COLOR_BUFFER_BIT);

  cogl_pop_framebuffer ();
}

int
main (int argc, char **argv)
{
    CoglContext *ctx;
    CoglOnscreen *onscreen;
    CoglFramebuffer *fb;
    GError *error = NULL;
    CoglVertexP2C4 triangle_vertices[] = {
        {0, 0.7, 0xff, 0x00, 0x00, 0x80},
        {-0.7, -0.7, 0x00, 0xff, 0x00, 0xff},
        {0.7, -0.7, 0x00, 0x00, 0xff, 0xff}
    };
    CoglPrimitive *triangle;
    CoglTexture *offscreen_texture;

    ctx = cogl_context_new (NULL, &error);
    if (!ctx) {
        fprintf (stderr, "Failed to create context: %s\n", error->message);
        return 1;
    }

    onscreen = cogl_onscreen_new (ctx, 640, 480);
    /* Eventually there will be an implicit allocate on first use so this
     * will become optional... */
    fb = COGL_FRAMEBUFFER (onscreen);
    if (!cogl_framebuffer_allocate (fb, &error)) {
        fprintf (stderr, "Failed to allocate framebuffer: %s\n", error->message);
        return 1;
    }

    cogl_onscreen_show (onscreen);

    /* Prepare onscreen primitive */
    triangle = cogl_primitive_new_p2c4 (COGL_VERTICES_MODE_TRIANGLES,
                                        3, triangle_vertices);

    offscreen_texture = cogl_texture_new_with_size (OFFSCREEN_WIDTH, OFFSCREEN_HEIGHT,
                                                    COGL_TEXTURE_NO_SLICING,
                                                    COGL_PIXEL_FORMAT_ANY);
    for (;;) {
        /* Draw scene with Cogl */
        cogl_push_framebuffer (fb);
        cogl_clear (&black, COGL_BUFFER_BIT_COLOR);

        cogl_set_source_color4f (0.0f, 0.0f, 0.0f, 1.0f);
        cogl_primitive_draw (triangle);

        update_offscreen_texture (ctx, offscreen_texture);
        cogl_set_source_texture (offscreen_texture);
        cogl_rectangle_with_texture_coords (-0.5, -0.5, 0.5, 0.5,
                                            0, 0, 1.0, 1.0);

        cogl_framebuffer_swap_buffers (fb);
    }

    return 0;
}
