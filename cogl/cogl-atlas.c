/*
 * Cogl
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2010,2011 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *  Neil Roberts   <neil@linux.intel.com>
 */

#include <config.h>

#include "cogl-atlas-private.h"
#include "cogl-rectangle-map.h"
#include "cogl-context-private.h"
#include "cogl-texture-private.h"
#include "cogl-texture-2d-private.h"
#include "cogl-texture-2d-sliced.h"
#include "cogl-texture-driver.h"
#include "cogl-pipeline-opengl-private.h"
#include "cogl-debug.h"
#include "cogl-framebuffer-private.h"
#include "cogl-blit.h"
#include "cogl-private.h"

#include <stdlib.h>

static void _cogl_atlas_free (CoglAtlas *atlas);

COGL_OBJECT_DEFINE (Atlas, atlas);

CoglAtlas *
_cogl_atlas_new (CoglContext *context,
                 CoglPixelFormat internal_format,
                 CoglAtlasFlags flags)
{
  CoglAtlas *atlas = u_new (CoglAtlas, 1);

  atlas->context = context;
  atlas->map = NULL;
  atlas->texture = NULL;
  atlas->flags = flags;
  atlas->internal_format = internal_format;

  _cogl_list_init (&atlas->allocate_closures);

  _cogl_list_init (&atlas->pre_reorganize_closures);
  _cogl_list_init (&atlas->post_reorganize_closures);

  return _cogl_atlas_object_new (atlas);
}

CoglAtlasAllocateClosure *
cogl_atlas_add_allocate_callback (CoglAtlas *atlas,
                                  CoglAtlasAllocateCallback callback,
                                  void *user_data,
                                  CoglUserDataDestroyCallback destroy)
{
  return _cogl_closure_list_add (&atlas->allocate_closures,
                                 callback,
                                 user_data,
                                 destroy);
}

void
cogl_atlas_remove_allocate_callback (CoglAtlas *atlas,
                                     CoglAtlasAllocateClosure *closure)
{
  _cogl_closure_disconnect (closure);
}

static void
_cogl_atlas_free (CoglAtlas *atlas)
{
  COGL_NOTE (ATLAS, "%p: Atlas destroyed", atlas);

  if (atlas->texture)
    cogl_object_unref (atlas->texture);
  if (atlas->map)
    _cogl_rectangle_map_free (atlas->map);

  _cogl_closure_list_disconnect_all (&atlas->allocate_closures);

  _cogl_closure_list_disconnect_all (&atlas->pre_reorganize_closures);
  _cogl_closure_list_disconnect_all (&atlas->post_reorganize_closures);

  u_free (atlas);
}

typedef struct _CoglAtlasRepositionData
{
  /* The current user data for this texture */
  void *allocation_data;

  /* The old and new positions of the texture */
  CoglRectangleMapEntry old_position;
  CoglRectangleMapEntry new_position;
} CoglAtlasRepositionData;

static void
_cogl_atlas_migrate (CoglAtlas *atlas,
                     int n_textures,
                     CoglAtlasRepositionData *textures,
                     CoglTexture *old_texture,
                     CoglTexture *new_texture,
                     void *skip_allocation_data)
{
  int i;
  CoglBlitData blit_data;

  /* If the 'disable migrate' flag is set then we won't actually copy
     the textures to their new location. Instead we'll just invoke the
     callback to update the position */
  if ((atlas->flags & COGL_ATLAS_DISABLE_MIGRATION))
    for (i = 0; i < n_textures; i++)
      {
        CoglAtlasAllocation *allocation =
          (CoglAtlasAllocation *)&textures[i].new_position;

        /* Update the texture position */
        _cogl_closure_list_invoke (&atlas->allocate_closures,
                                   CoglAtlasAllocateCallback,
                                   atlas,
                                   new_texture,
                                   allocation,
                                   textures[i].allocation_data);
      }
  else
    {
      _cogl_blit_begin (&blit_data, new_texture, old_texture);

      for (i = 0; i < n_textures; i++)
        {
          CoglAtlasAllocation *allocation =
            (CoglAtlasAllocation *)&textures[i].new_position;

          /* Skip the texture that is being added because it doesn't contain
             any data yet */
          if (textures[i].allocation_data != skip_allocation_data)
            _cogl_blit (&blit_data,
                        textures[i].old_position.x,
                        textures[i].old_position.y,
                        textures[i].new_position.x,
                        textures[i].new_position.y,
                        textures[i].new_position.width,
                        textures[i].new_position.height);

          /* Update the texture position */
          _cogl_closure_list_invoke (&atlas->allocate_closures,
                                     CoglAtlasAllocateCallback,
                                     atlas,
                                     new_texture,
                                     allocation,
                                     textures[i].allocation_data);
        }

      _cogl_blit_end (&blit_data);
    }
}

typedef struct _CoglAtlasGetRectanglesData
{
  CoglAtlasRepositionData *textures;
  /* Number of textures found so far */
  int n_textures;
} CoglAtlasGetRectanglesData;

static void
_cogl_atlas_get_rectangles_cb (const CoglRectangleMapEntry *rectangle,
                               void *rect_data,
                               void *user_data)
{
  CoglAtlasGetRectanglesData *data = user_data;

  data->textures[data->n_textures].old_position = *rectangle;
  data->textures[data->n_textures++].allocation_data = rect_data;
}

static void
_cogl_atlas_get_next_size (int *map_width,
                           int *map_height)
{
  /* Double the size of the texture by increasing whichever dimension
     is smaller */
  if (*map_width < *map_height)
    *map_width <<= 1;
  else
    *map_height <<= 1;
}

static void
_cogl_atlas_get_initial_size (CoglAtlas *atlas,
                              int *map_width,
                              int *map_height)
{
  CoglContext *ctx = atlas->context;
  unsigned int size;
  GLenum gl_intformat;
  GLenum gl_format;
  GLenum gl_type;

  ctx->driver_vtable->pixel_format_to_gl (ctx,
                                          atlas->internal_format,
                                          &gl_intformat,
                                          &gl_format,
                                          &gl_type);

  /* At least on Intel hardware, the texture size will be rounded up
     to at least 1MB so we might as well try to aim for that as an
     initial minimum size. If the format is only 1 byte per pixel we
     can use 1024x1024, otherwise we'll assume it will take 4 bytes
     per pixel and use 512x512. */
  if (_cogl_pixel_format_get_bytes_per_pixel (atlas->internal_format) == 1)
    size = 1024;
  else
    size = 512;

  /* Some platforms might not support this large size so we'll
     decrease the size until it can */
  while (size > 1 &&
         !ctx->texture_driver->size_supported (ctx,
                                               GL_TEXTURE_2D,
                                               gl_intformat,
                                               gl_format,
                                               gl_type,
                                               size, size))
    size >>= 1;

  *map_width = size;
  *map_height = size;
}

static CoglRectangleMap *
_cogl_atlas_create_map (CoglAtlas *atlas,
                        int map_width,
                        int map_height,
                        int n_textures,
                        CoglAtlasRepositionData *textures)
{
  CoglContext *ctx = atlas->context;
  GLenum gl_intformat;
  GLenum gl_format;
  GLenum gl_type;

  ctx->driver_vtable->pixel_format_to_gl (ctx,
                                          atlas->internal_format,
                                          &gl_intformat,
                                          &gl_format,
                                          &gl_type);

  /* Keep trying increasingly larger atlases until we can fit all of
     the textures */
  while (ctx->texture_driver->size_supported (ctx,
                                              GL_TEXTURE_2D,
                                              gl_intformat,
                                              gl_format,
                                              gl_type,
                                              map_width, map_height))
    {
      CoglRectangleMap *new_atlas = _cogl_rectangle_map_new (map_width,
                                                             map_height,
                                                             NULL);
      int i;

      COGL_NOTE (ATLAS, "Trying to resize the atlas to %ux%u",
                 map_width, map_height);

      /* Add all of the textures and keep track of the new position */
      for (i = 0; i < n_textures; i++)
        if (!_cogl_rectangle_map_add (new_atlas,
                                      textures[i].old_position.width,
                                      textures[i].old_position.height,
                                      textures[i].allocation_data,
                                      &textures[i].new_position))
          break;

      /* If the atlas can contain all of the textures then we have a
         winner */
      if (i >= n_textures)
        return new_atlas;
      else
        COGL_NOTE (ATLAS, "Atlas size abandoned after trying "
                   "%u out of %u textures",
                   i, n_textures);

      _cogl_rectangle_map_free (new_atlas);
      _cogl_atlas_get_next_size (&map_width, &map_height);
    }

  /* If we get here then there's no atlas that can accommodate all of
     the rectangles */

  return NULL;
}

static CoglTexture2D *
_cogl_atlas_create_texture (CoglAtlas *atlas,
                            int width,
                            int height)
{
  CoglContext *ctx = atlas->context;
  CoglTexture2D *tex;
  CoglError *ignore_error = NULL;

  if ((atlas->flags & COGL_ATLAS_CLEAR_TEXTURE))
    {
      uint8_t *clear_data;
      CoglBitmap *clear_bmp;
      int bpp = _cogl_pixel_format_get_bytes_per_pixel (atlas->internal_format);

      /* Create a buffer of zeroes to initially clear the texture */
      clear_data = u_malloc0 (width * height * bpp);
      clear_bmp = cogl_bitmap_new_for_data (ctx,
                                            width,
                                            height,
                                            atlas->internal_format,
                                            width * bpp,
                                            clear_data);

      tex = cogl_texture_2d_new_from_bitmap (clear_bmp);

      _cogl_texture_set_internal_format (COGL_TEXTURE (tex),
                                         atlas->internal_format);

      if (!cogl_texture_allocate (COGL_TEXTURE (tex), &ignore_error))
        {
          cogl_error_free (ignore_error);
          cogl_object_unref (tex);
          tex = NULL;
        }

      cogl_object_unref (clear_bmp);

      u_free (clear_data);
    }
  else
    {
      tex = cogl_texture_2d_new_with_size (ctx, width, height);

      _cogl_texture_set_internal_format (COGL_TEXTURE (tex),
                                         atlas->internal_format);

      if (!cogl_texture_allocate (COGL_TEXTURE (tex), &ignore_error))
        {
          cogl_error_free (ignore_error);
          cogl_object_unref (tex);
          tex = NULL;
        }
    }

  return tex;
}

static int
_cogl_atlas_compare_size_cb (const void *a,
                             const void *b)
{
  const CoglAtlasRepositionData *ta = a;
  const CoglAtlasRepositionData *tb = b;
  unsigned int a_size, b_size;

  a_size = ta->old_position.width * ta->old_position.height;
  b_size = tb->old_position.width * tb->old_position.height;

  return a_size < b_size ? 1 : a_size > b_size ? -1 : 0;
}

CoglBool
_cogl_atlas_allocate_space (CoglAtlas *atlas,
                            int width,
                            int height,
                            void *allocation_data)
{
  CoglAtlasGetRectanglesData data;
  CoglRectangleMap *new_map;
  CoglTexture2D *new_tex;
  int map_width, map_height;
  CoglBool ret;
  CoglAtlasAllocation new_allocation;

  /* Check if we can fit the rectangle into the existing map */
  if (atlas->map &&
      _cogl_rectangle_map_add (atlas->map, width, height,
                               allocation_data,
                               (CoglRectangleMapEntry *)&new_allocation))
    {
      COGL_NOTE (ATLAS, "%p: Atlas is %ix%i, has %i textures and is %i%% waste",
                 atlas,
                 _cogl_rectangle_map_get_width (atlas->map),
                 _cogl_rectangle_map_get_height (atlas->map),
                 _cogl_rectangle_map_get_n_rectangles (atlas->map),
                 /* waste as a percentage */
                 _cogl_rectangle_map_get_remaining_space (atlas->map) *
                 100 / (_cogl_rectangle_map_get_width (atlas->map) *
                        _cogl_rectangle_map_get_height (atlas->map)));

      _cogl_closure_list_invoke (&atlas->allocate_closures,
                                 CoglAtlasAllocateCallback,
                                 atlas,
                                 atlas->texture,
                                 &new_allocation,
                                 allocation_data);

      return TRUE;
    }

  /* If we make it here then we need to reorganize the atlas. First
     we'll notify any users of the atlas that this is going to happen
     so that for example in CoglAtlasTexture it can notify that the
     storage has changed and cause a flush */
  _cogl_closure_list_invoke (&atlas->pre_reorganize_closures,
                             CoglAtlasReorganizeCallback,
                             atlas);

  /* Get an array of all the textures currently in the atlas. */
  data.n_textures = 0;
  if (atlas->map == NULL)
    data.textures = u_malloc (sizeof (CoglAtlasRepositionData));
  else
    {
      int n_rectangles =
        _cogl_rectangle_map_get_n_rectangles (atlas->map);
      data.textures = u_malloc (sizeof (CoglAtlasRepositionData) *
                                (n_rectangles + 1));
      _cogl_rectangle_map_foreach (atlas->map,
                                   _cogl_atlas_get_rectangles_cb,
                                   &data);
    }

  /* Add the new rectangle as a dummy texture so that it can be
     positioned with the rest */
  data.textures[data.n_textures].old_position.x = 0;
  data.textures[data.n_textures].old_position.y = 0;
  data.textures[data.n_textures].old_position.width = width;
  data.textures[data.n_textures].old_position.height = height;
  data.textures[data.n_textures++].allocation_data = allocation_data;

  /* The atlasing algorithm works a lot better if the rectangles are
     added in decreasing order of size so we'll first sort the
     array */
  qsort (data.textures, data.n_textures,
         sizeof (CoglAtlasRepositionData),
         _cogl_atlas_compare_size_cb);

  /* Try to create a new atlas that can contain all of the textures */
  if (atlas->map)
    {
      map_width = _cogl_rectangle_map_get_width (atlas->map);
      map_height = _cogl_rectangle_map_get_height (atlas->map);

      /* If there is enough space in for the new rectangle in the
         existing atlas with at least 6% waste we'll start with the
         same size, otherwise we'll immediately double it */
      if ((map_width * map_height -
           _cogl_rectangle_map_get_remaining_space (atlas->map) +
           width * height) * 53 / 50 >
          map_width * map_height)
        _cogl_atlas_get_next_size (&map_width, &map_height);
    }
  else
    _cogl_atlas_get_initial_size (atlas, &map_width, &map_height);

  new_map = _cogl_atlas_create_map (atlas,
                                    map_width, map_height,
                                    data.n_textures, data.textures);

  /* If we can't create a map with the texture then give up */
  if (new_map == NULL)
    {
      COGL_NOTE (ATLAS, "%p: Could not fit texture in the atlas", atlas);
      ret = FALSE;
    }
  /* We need to migrate the existing textures into a new texture */
  else if ((new_tex = _cogl_atlas_create_texture
            (atlas,
             _cogl_rectangle_map_get_width (new_map),
             _cogl_rectangle_map_get_height (new_map))) == NULL)
    {
      COGL_NOTE (ATLAS, "%p: Could not create a CoglTexture2D", atlas);
      _cogl_rectangle_map_free (new_map);
      ret = FALSE;
    }
  else
    {
      int waste;

      COGL_NOTE (ATLAS,
                 "%p: Atlas %s with size %ix%i",
                 atlas,
                 atlas->map == NULL ||
                 _cogl_rectangle_map_get_width (atlas->map) !=
                 _cogl_rectangle_map_get_width (new_map) ||
                 _cogl_rectangle_map_get_height (atlas->map) !=
                 _cogl_rectangle_map_get_height (new_map) ?
                 "resized" : "reorganized",
                 _cogl_rectangle_map_get_width (new_map),
                 _cogl_rectangle_map_get_height (new_map));

      if (atlas->map)
        {
          /* Move all the textures to the right position in the new
             texture. This will also update the texture's rectangle */
          _cogl_atlas_migrate (atlas,
                               data.n_textures,
                               data.textures,
                               atlas->texture,
                               COGL_TEXTURE (new_tex),
                               allocation_data);
          _cogl_rectangle_map_free (atlas->map);
          cogl_object_unref (atlas->texture);
        }
      else
        {
          CoglAtlasAllocation *allocation =
            (CoglAtlasAllocation *)&data.textures[0].new_position;

          /* We know there's only one texture so we can just directly
             update the rectangle from its new position */
          _cogl_closure_list_invoke (&atlas->allocate_closures,
                                     CoglAtlasAllocateCallback,
                                     atlas,
                                     COGL_TEXTURE (new_tex),
                                     allocation,
                                     data.textures[0].allocation_data);
        }

      atlas->map = new_map;
      atlas->texture = COGL_TEXTURE (new_tex);

      waste = (_cogl_rectangle_map_get_remaining_space (atlas->map) *
               100 / (_cogl_rectangle_map_get_width (atlas->map) *
                      _cogl_rectangle_map_get_height (atlas->map)));

      COGL_NOTE (ATLAS, "%p: Atlas is %ix%i, has %i textures and is %i%% waste",
                 atlas,
                 _cogl_rectangle_map_get_width (atlas->map),
                 _cogl_rectangle_map_get_height (atlas->map),
                 _cogl_rectangle_map_get_n_rectangles (atlas->map),
                 waste);

      ret = TRUE;
    }

  u_free (data.textures);

  _cogl_closure_list_invoke (&atlas->pre_reorganize_closures,
                             CoglAtlasReorganizeCallback,
                             atlas);

  return ret;
}

void
_cogl_atlas_remove (CoglAtlas *atlas,
                    int x,
                    int y,
                    int width,
                    int height)
{
  CoglRectangleMapEntry rectangle = { x, y, width, height };

  _cogl_rectangle_map_remove (atlas->map, &rectangle);

  COGL_NOTE (ATLAS, "%p: Removed rectangle sized %ix%i",
             atlas,
             rectangle.width,
             rectangle.height);
  COGL_NOTE (ATLAS, "%p: Atlas is %ix%i, has %i textures and is %i%% waste",
             atlas,
             _cogl_rectangle_map_get_width (atlas->map),
             _cogl_rectangle_map_get_height (atlas->map),
             _cogl_rectangle_map_get_n_rectangles (atlas->map),
             _cogl_rectangle_map_get_remaining_space (atlas->map) *
             100 / (_cogl_rectangle_map_get_width (atlas->map) *
                    _cogl_rectangle_map_get_height (atlas->map)));
};

CoglTexture *
cogl_atlas_get_texture (CoglAtlas *atlas)
{
  return atlas->texture;
}

static CoglTexture *
create_migration_texture (CoglContext *ctx,
                          int width,
                          int height,
                          CoglPixelFormat internal_format)
{
  CoglTexture *tex;
  CoglError *skip_error = NULL;

  if ((_cogl_util_is_pot (width) && _cogl_util_is_pot (height)) ||
      (cogl_has_feature (ctx, COGL_FEATURE_ID_TEXTURE_NPOT_BASIC) &&
       cogl_has_feature (ctx, COGL_FEATURE_ID_TEXTURE_NPOT_MIPMAP)))
    {
      /* First try creating a fast-path non-sliced texture */
      tex = COGL_TEXTURE (cogl_texture_2d_new_with_size (ctx,
                                                         width, height));

      _cogl_texture_set_internal_format (tex, internal_format);

      /* TODO: instead of allocating storage here it would be better
       * if we had some api that let us just check that the size is
       * supported by the hardware so storage could be allocated
       * lazily when uploading data. */
      if (!cogl_texture_allocate (tex, &skip_error))
        {
          cogl_error_free (skip_error);
          cogl_object_unref (tex);
          tex = NULL;
        }
    }
  else
    tex = NULL;

  if (!tex)
    {
      CoglTexture2DSliced *tex_2ds =
        cogl_texture_2d_sliced_new_with_size (ctx,
                                              width,
                                              height,
                                              COGL_TEXTURE_MAX_WASTE);

      _cogl_texture_set_internal_format (COGL_TEXTURE (tex_2ds),
                                         internal_format);

      tex = COGL_TEXTURE (tex_2ds);
    }

  return tex;
}

CoglTexture *
_cogl_atlas_migrate_allocation (CoglAtlas *atlas,
                                int x,
                                int y,
                                int width,
                                int height,
                                CoglPixelFormat internal_format)
{
  CoglContext *ctx = atlas->context;
  CoglTexture *tex;
  CoglBlitData blit_data;
  CoglError *ignore_error = NULL;

  /* Create a new texture at the right size */
  tex = create_migration_texture (ctx, width, height, internal_format);
  if (!cogl_texture_allocate (tex, &ignore_error))
    {
      cogl_error_free (ignore_error);
      cogl_object_unref (tex);
      return NULL;
    }

  /* Blit the data out of the atlas to the new texture. If FBOs
     aren't available this will end up having to copy the entire
     atlas texture */
  _cogl_blit_begin (&blit_data, tex, atlas->texture);
  _cogl_blit (&blit_data,
                    x, y,
                    0, 0,
                    width, height);
  _cogl_blit_end (&blit_data);

  return tex;
}

CoglAtlasReorganizeClosure *
cogl_atlas_add_pre_reorganize_callback (CoglAtlas *atlas,
                                        CoglAtlasReorganizeCallback callback,
                                        void *user_data,
                                        CoglUserDataDestroyCallback destroy)
{
  _COGL_RETURN_VAL_IF_FAIL (callback != NULL, NULL);

  return _cogl_closure_list_add (&atlas->pre_reorganize_closures,
                                 callback,
                                 user_data,
                                 destroy);
}

void
cogl_atlas_remove_pre_reorganize_callback (CoglAtlas *atlas,
                                           CoglAtlasReorganizeClosure *closure)
{
  _cogl_closure_disconnect (closure);
}

CoglAtlasReorganizeClosure *
cogl_atlas_add_post_reorganize_callback (CoglAtlas *atlas,
                                         CoglAtlasReorganizeCallback callback,
                                         void *user_data,
                                         CoglUserDataDestroyCallback destroy)
{
  _COGL_RETURN_VAL_IF_FAIL (callback != NULL, NULL);

  return _cogl_closure_list_add (&atlas->post_reorganize_closures,
                                 callback,
                                 user_data,
                                 destroy);
}

void
cogl_atlas_remove_post_reorganize_callback (CoglAtlas *atlas,
                                            CoglAtlasReorganizeClosure *closure)
{
  _cogl_closure_disconnect (closure);
}

typedef struct _ForeachState
{
  CoglAtlas *atlas;
  CoglAtlasForeachCallback callback;
  void *user_data;
} ForeachState;

static void
foreach_rectangle_cb (const CoglRectangleMapEntry *entry,
                      void *rectangle_data,
                      void *user_data)
{
  ForeachState *state = user_data;

  state->callback (state->atlas,
                   (CoglAtlasAllocation *)entry,
                   rectangle_data,
                   state->user_data);
}

void
cogl_atlas_foreach (CoglAtlas *atlas,
                    CoglAtlasForeachCallback callback,
                    void *user_data)
{
  if (atlas->map)
    {
      ForeachState state;

      state.atlas = atlas;
      state.callback = callback;
      state.user_data = user_data;

      _cogl_rectangle_map_foreach (atlas->map, foreach_rectangle_cb, &state);
    }
}

int
cogl_atlas_get_n_allocations (CoglAtlas *atlas)
{
  if (atlas->map)
    return _cogl_rectangle_map_get_n_rectangles (atlas->map);
  else
    return 0;
}

