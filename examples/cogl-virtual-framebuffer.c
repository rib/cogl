#include <cogl/cogl.h>
#include <glib.h>
#include <stdio.h>

CoglColor black;

#define TEX_WIDTH 300
#define TEX_HEIGHT 220

int
main (int argc, char **argv)
{
    CoglContext *ctx;
    CoglOnscreen *onscreen;
    CoglTexture2D *textures[4];
    CoglHandle offscreens[4];
    CoglVirtualFramebuffer *vfb;
    CoglFramebuffer *fb;
    GError *error = NULL;
    CoglVertexP2C4 triangle_vertices[] = {
        {0, 0.7, 0xff, 0x00, 0x00, 0x80},
        {-0.7, -0.7, 0x00, 0xff, 0x00, 0xff},
        {0.7, -0.7, 0x00, 0x00, 0xff, 0xff}
    };
    CoglPrimitive *triangle;
    int i;

    ctx = cogl_context_new (NULL, &error);
    if (!ctx) {
        fprintf (stderr, "Failed to create context: %s\n", error->message);
        return 1;
    }

    for (i = 0; i < 4; i++)
      {
        textures[i] = cogl_texture_2d_new_with_size (ctx,
                                                     300, 220,
                                                     COGL_PIXEL_FORMAT_RGBA_8888,
                                                     &error);
        if (!textures[i])
          {
            fprintf (stderr, "Failed to allocated texture: %s\n", error->message);
            return 1;
          }
        offscreens[i] =
          cogl_offscreen_new_to_texture (COGL_TEXTURE (textures[i]));
      }

    vfb = cogl_virtual_framebuffer_new (ctx, 640, 480);
    cogl_virtual_framebuffer_add_slice (vfb, offscreens[0],
                                        0, 0, -1, -1,
                                        10, 10);
    cogl_virtual_framebuffer_add_slice (vfb, offscreens[1],
                                        0, 0, -1, -1,
                                        330, 10);
    cogl_virtual_framebuffer_add_slice (vfb, offscreens[2],
                                        0, 0, -1, -1,
                                        10, 250);
    cogl_virtual_framebuffer_add_slice (vfb, offscreens[3],
                                        0, 0, -1, -1,
                                        330, 250);

    onscreen = cogl_onscreen_new (ctx, 640, 480);
    /* Eventually there will be an implicit allocate on first use so this
     * will become optional... */
    fb = COGL_FRAMEBUFFER (onscreen);
    if (!cogl_framebuffer_allocate (fb, &error)) {
        fprintf (stderr, "Failed to allocate framebuffer: %s\n", error->message);
        return 1;
    }

    cogl_onscreen_show (onscreen);

    cogl_push_framebuffer (COGL_FRAMEBUFFER (vfb));

    triangle = cogl_primitive_new_p2c4 (COGL_VERTICES_MODE_TRIANGLES,
                                        3, triangle_vertices);
    for (;;) {
        cogl_clear (&black, COGL_BUFFER_BIT_COLOR);
        cogl_primitive_draw (triangle);

        /* Now draw the slices of the virtual framebuffer onscreen */
        cogl_set_source_texture (COGL_TEXTURE (textures[0]));
        cogl_rectangle (10, 10, 10 + TEX_WIDTH, 10 + TEX_HEIGHT);
        cogl_set_source_texture (COGL_TEXTURE (textures[1]));
        cogl_rectangle (330, 10, 330 + TEX_WIDTH, 10 + TEX_HEIGHT);
        cogl_set_source_texture (COGL_TEXTURE (textures[2]));
        cogl_rectangle (10, 250, 10 + TEX_WIDTH, 250 + TEX_HEIGHT);
        cogl_set_source_texture (COGL_TEXTURE (textures[3]));
        cogl_rectangle (330, 250, 330 + TEX_WIDTH, 250 + TEX_HEIGHT);

        cogl_framebuffer_swap_buffers (fb);
    }

    return 0;
}
