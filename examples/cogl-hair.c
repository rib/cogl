#include <cogl/cogl.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define ROTATION_SPEED 30.0f
#define USE_COLOR_TEXTURE 1
//#define USE_SHAVE_TEXTURE 1

#ifdef USE_COLOR_TEXTURE

#define TEXTURE_SIZE 128
#define N_TEXTURES 10
#define N_HAIRS 9000
#define LAYER_RESOLUTION 100
#define HAIR_LENGTH 0.5
#define TIP_FLOP_STR "0.5"
#define BASE_DIAMETER 0.02f
#define TIP_DIAMETER 0.0f

#else

#define TEXTURE_SIZE 64
#define N_TEXTURES 100
#define N_HAIRS 300
#define LAYER_RESOLUTION 320
#define HAIR_LENGTH 1.0
#define TIP_FLOP_STR "1.0"
#define BASE_DIAMETER 0.1f
#define TIP_DIAMETER 0.0f
#endif

/* The state for this example... */
typedef struct _Data
{
  CoglContext *ctx;
  CoglFramebuffer *fb;

  int framebuffer_width;
  int framebuffer_height;

  gboolean swap_ready;

  CoglMatrix view;

  CoglAttribute *circle;
  CoglIndices *indices;
  CoglPrimitive *prim;
  CoglTexture *shave_texture;
  CoglTexture *color_texture;
  CoglTexture *circle_texture;
  CoglPipeline *cube_pipeline;
  CoglTexture *textures[N_TEXTURES];
  CoglPipeline *crate_pipelines[N_TEXTURES];

  int hair_pos_locations[N_TEXTURES];
  int progress_locations[N_TEXTURES];
  int light_dir_locations[N_TEXTURES];
  int slant_locations[N_TEXTURES];

  GTimer *timer;

} Data;

typedef struct _Vertex
{
  float x, y, z;
  float s, t;
  float normal[3];
} Vertex;

/* A cube modelled using 4 vertices for each face.
 *
 * We use an index buffer when drawing the cube later so the GPU will
 * actually read each face as 2 separate triangles.
 */
static Vertex vertices[] =
{
  /* Front face */
  { /* pos = */ -1.0f, -1.0f,  1.0f, /* tex coords = */ 0.0f, 1.0f,
    /* normal */ {0.0f, 0.0f, 1.0f}},
  { /* pos = */  1.0f, -1.0f,  1.0f, /* tex coords = */ 1.0f, 1.0f,
    /* normal */ {0.0f, 0.0f, 1.0f}},
  { /* pos = */  1.0f,  1.0f,  1.0f, /* tex coords = */ 1.0f, 0.0f,
    /* normal */ {0.0f, 0.0f, 1.0f}},
  { /* pos = */ -1.0f,  1.0f,  1.0f, /* tex coords = */ 0.0f, 0.0f,
    /* normal */ {0.0f, 0.0f, 1.0f}},

  /* Back face */
  { /* pos = */ -1.0f, -1.0f, -1.0f, /* tex coords = */ 1.0f, 0.0f,
    /* normal */ {0.0f, 0.0f, -1.0f}},
  { /* pos = */ -1.0f,  1.0f, -1.0f, /* tex coords = */ 1.0f, 1.0f,
    /* normal */ {0.0f, 0.0f, -1.0f}},
  { /* pos = */  1.0f,  1.0f, -1.0f, /* tex coords = */ 0.0f, 1.0f,
    /* normal */ {0.0f, 0.0f, -1.0f}},
  { /* pos = */  1.0f, -1.0f, -1.0f, /* tex coords = */ 0.0f, 0.0f,
    /* normal */ {0.0f, 0.0f, -1.0f}},

  /* Top face */
  { /* pos = */ -1.0f,  1.0f, -1.0f, /* tex coords = */ 0.0f, 1.0f,
    /* normal */ {0.0f, 1.0f, 0.0f}},
  { /* pos = */ -1.0f,  1.0f,  1.0f, /* tex coords = */ 0.0f, 0.0f,
    /* normal */ {0.0f, 1.0f, 0.0f}},
  { /* pos = */  1.0f,  1.0f,  1.0f, /* tex coords = */ 1.0f, 0.0f,
    /* normal */ {0.0f, 1.0f, 0.0f}},
  { /* pos = */  1.0f,  1.0f, -1.0f, /* tex coords = */ 1.0f, 1.0f,
    /* normal */ {0.0f, 1.0f, 0.0f}},

  /* Bottom face */
  { /* pos = */ -1.0f, -1.0f, -1.0f, /* tex coords = */ 1.0f, 1.0f,
    /* normal */ {0.0f, -1.0f, 0.0f}},
  { /* pos = */  1.0f, -1.0f, -1.0f, /* tex coords = */ 0.0f, 1.0f,
    /* normal */ {0.0f, -1.0f, 0.0f}},
  { /* pos = */  1.0f, -1.0f,  1.0f, /* tex coords = */ 0.0f, 0.0f,
    /* normal */ {0.0f, -1.0f, 0.0f}},
  { /* pos = */ -1.0f, -1.0f,  1.0f, /* tex coords = */ 1.0f, 0.0f,
    /* normal */ {0.0f, -1.0f, 0.0f}},

  /* Right face */
  { /* pos = */ 1.0f, -1.0f, -1.0f, /* tex coords = */ 1.0f, 0.0f,
    /* normal */ {1.0f, 0.0f, 0.0f}},
  { /* pos = */ 1.0f,  1.0f, -1.0f, /* tex coords = */ 1.0f, 1.0f,
    /* normal */ {1.0f, 0.0f, 0.0f}},
  { /* pos = */ 1.0f,  1.0f,  1.0f, /* tex coords = */ 0.0f, 1.0f,
    /* normal */ {1.0f, 0.0f, 0.0f}},
  { /* pos = */ 1.0f, -1.0f,  1.0f, /* tex coords = */ 0.0f, 0.0f,
    /* normal */ {1.0f, 0.0f, 0.0f}},

  /* Left face */
  { /* pos = */ -1.0f, -1.0f, -1.0f, /* tex coords = */ 0.0f, 0.0f,
    /* normal */ {-1.0f, 0.0f, 0.0f}},
  { /* pos = */ -1.0f, -1.0f,  1.0f, /* tex coords = */ 1.0f, 0.0f,
    /* normal */ {-1.0f, 0.0f, 0.0f}},
  { /* pos = */ -1.0f,  1.0f,  1.0f, /* tex coords = */ 1.0f, 1.0f,
    /* normal */ {-1.0f, 0.0f, 0.0f}},
  { /* pos = */ -1.0f,  1.0f, -1.0f, /* tex coords = */ 0.0f, 1.0f,
    /* normal */ {-1.0f, 0.0f, 0.0f}}
};

static CoglFramebuffer *
create_framebuffer (Data *data, int width, int height)
{
  CoglOnscreen *onscreen = cogl_onscreen_new (data->ctx, width, height);
  CoglFramebuffer *fb;
  float fovy, aspect, z_near, z_far;
  GError *error = NULL;

  /* Explicitly allocate the framebuffer so if the frambuffer gets
   * forced to have a different size which we can check... */
  fb = COGL_FRAMEBUFFER (onscreen);
  if (!cogl_framebuffer_allocate (fb, &error)) {
      fprintf (stderr, "Failed to allocate framebuffer: %s\n", error->message);
      return NULL;
  }

  data->framebuffer_width = cogl_framebuffer_get_width (fb);
  data->framebuffer_height = cogl_framebuffer_get_height (fb);

  cogl_framebuffer_set_viewport (fb,
                                 0, 0,
                                 data->framebuffer_width,
                                 data->framebuffer_height);

  fovy = 60; /* y-axis field of view */
  aspect = (float)data->framebuffer_width/(float)data->framebuffer_height;
  z_near = 0.1; /* distance to near clipping plane */
  z_far = 2000; /* distance to far clipping plane */

  cogl_framebuffer_perspective (fb, fovy, aspect, z_near, z_far);

  cogl_onscreen_show (onscreen);

  return fb;
}

static void
init_shell_textures (Data *data)
{
  CoglPipeline *pipeline = cogl_pipeline_new (data->ctx);
  float diameter;
  float hair_taper;
  int i;

  cogl_pipeline_set_layer_texture (pipeline, 0, data->circle_texture);
  cogl_set_source (pipeline);

  diameter = BASE_DIAMETER;
  hair_taper = (BASE_DIAMETER - TIP_DIAMETER) / N_TEXTURES;

  for (i = 0; i < N_TEXTURES; i++)
    {
      CoglOffscreen *offscreen;
      CoglTexture2D *tex2d;
      int j;

      tex2d = cogl_texture_2d_new_with_size (data->ctx, 256, 256,
                                             COGL_PIXEL_FORMAT_RGBA_8888,
                                             NULL);
      data->textures[i] = COGL_TEXTURE (tex2d);

      offscreen = cogl_offscreen_new_to_texture (data->textures[i]);
      cogl_push_framebuffer (COGL_FRAMEBUFFER (offscreen));

      cogl_framebuffer_clear4f (COGL_FRAMEBUFFER (offscreen),
                                COGL_BUFFER_BIT_COLOR, 0, 0, 0, 0);
#ifdef USE_COLOR_TEXTURE
      cogl_pipeline_set_color4f (pipeline, 0.3, 0.3, 0.3, 1.0);
#else
      cogl_pipeline_set_color4ub (pipeline, 0x7f, 0x4e, 0x00, 0xff);
#endif

      srand (7025);
      for (j = 0; j < N_HAIRS; j++)
        {
          float x = ((rand() / (float)RAND_MAX) * 2) - 1;
          float y = ((rand() / (float)RAND_MAX) * 2) - 1;

          cogl_rectangle (x, y, x + diameter, y + diameter);
        }

#ifdef USE_COLOR_TEXTURE
      cogl_pipeline_set_color4f (pipeline, 1, 1, 1, 1);
#else
      cogl_pipeline_set_color4ub (pipeline, 0xf8, 0x98, 0x00, 0xff);
#endif

      srand (7025);
      for (j = 0; j < N_HAIRS; j++)
        {
          float x = ((rand() / (float)RAND_MAX) * 2) - 1;
          float y = ((rand() / (float)RAND_MAX) * 2) - 1;
          float shadow_offset = diameter / 4.0f;
          x += shadow_offset;
          y += shadow_offset;

          cogl_rectangle (x, y, x + diameter, y + diameter);
        }

      cogl_pop_framebuffer ();
      cogl_object_unref (offscreen);

      diameter -= hair_taper;
    }
}

static void
paint (CoglFramebuffer *fb, Data *data)
{
  float rotation;
  int i;
  int group_count;
  float slant[4] = { 1, 0.1, 1, 0.05};
  float slant_scale = 0.2;
  int slant_dir = 0;

  cogl_framebuffer_clear4f (fb, COGL_BUFFER_BIT_COLOR|COGL_BUFFER_BIT_DEPTH,
                            0, 0, 0, 1);

  cogl_framebuffer_push_matrix (fb);

  cogl_framebuffer_translate (fb, 0, 0, -5);

  /* Update the rotation based on the time the application has been
     running so that we get a linear animation regardless of the frame
     rate */
  rotation = g_timer_elapsed (data->timer, NULL) * ROTATION_SPEED;

  /* Rotate the cube separately around each axis.
   *
   * Note: Cogl matrix manipulation follows the same rules as for
   * OpenGL. We use column-major matrices and - if you consider the
   * transformations happening to the model - then they are combined
   * in reverse order which is why the rotation is done last, since
   * we want it to be a rotation around the origin, before it is
   * scaled and translated.
   */
  cogl_framebuffer_rotate (fb, rotation, 0, 0, 1);
  //cogl_framebuffer_rotate (fb, 90, 0, 1, 0);
  cogl_framebuffer_rotate (fb, rotation, 0, 1, 0);
  cogl_framebuffer_rotate (fb, rotation, 1, 0, 0);

  cogl_framebuffer_draw_primitive (fb, data->cube_pipeline, data->prim);

  group_count = LAYER_RESOLUTION / N_TEXTURES;
  for (i = 0; i < N_TEXTURES; i++)
    {
      int j;
      for (j = 0; j < group_count; j++)
        {
          float progress = (group_count * i + j) / (float)LAYER_RESOLUTION;
          float hair_pos = HAIR_LENGTH * progress;
          float light_dir[4] = { 0, 1, 0, 0};

          cogl_pipeline_set_uniform_1f (data->crate_pipelines[i],
                                        data->progress_locations[i], progress);
          cogl_pipeline_set_uniform_1f (data->crate_pipelines[i],
                                        data->hair_pos_locations[i], hair_pos);
          cogl_pipeline_set_uniform_float (data->crate_pipelines[i],
                                           data->light_dir_locations[i],
                                           4, /* n_components */
                                           1, /* count */
                                           light_dir);
          cogl_pipeline_set_uniform_float (data->crate_pipelines[i],
                                           data->slant_locations[i],
                                           4, /* n_components */
                                           1, /* count */
                                           slant);

          /* Give Cogl some geometry to draw. */
          cogl_framebuffer_draw_primitive (fb, data->crate_pipelines[i],
                                           data->prim);

#if 0
          slant[0] =  1 - slant[0];
          slant[1] = - slant[1];
          slant[2] =  1 - slant[2];
          slant[3] = - slant[3];
#endif
          slant_dir = (slant_dir + 1) % 4;
          //slant_dir = 3;
          switch (slant_dir)
            {
            case 0:
              slant[0] = 1;
              slant[1] = slant_scale;
              slant[2] = 0;
              slant[3] = 0;
              break;
            case 1:
              slant[0] = 0;
              slant[1] = -slant_scale;
              slant[2] = 0;
              slant[3] = 0;
              break;
            case 2:
              slant[0] = 0;
              slant[1] = 0;
              slant[2] = 1;
              slant[3] = slant_scale;
              break;
            case 3:
              slant[0] = 0;
              slant[1] = 0;
              slant[2] = 0;
              slant[3] = -slant_scale;
              break;
            default:
              g_warn_if_reached ();
            }

        }
    }

  cogl_framebuffer_pop_matrix (fb);
}

CoglPrimitive *
primitive_new_p3t2t2n3 (CoglContext *ctx,
                        CoglVerticesMode mode,
                        int n_vertices,
                        const Vertex *data)
{
  CoglAttributeBuffer *attribute_buffer =
    cogl_attribute_buffer_new (ctx, n_vertices * sizeof (Vertex), data);
  CoglAttribute *attributes[3];
  CoglPrimitive *prim;
  int i;

  attributes[0] = cogl_attribute_new (attribute_buffer,
                                      "cogl_position_in",
                                      sizeof (Vertex),
                                      offsetof (Vertex, x),
                                      3,
                                      COGL_ATTRIBUTE_TYPE_FLOAT);
  attributes[1] = cogl_attribute_new (attribute_buffer,
                                      "cogl_tex_coord0_in",
                                      sizeof (Vertex),
                                      offsetof (Vertex, s),
                                      2,
                                      COGL_ATTRIBUTE_TYPE_FLOAT);
  attributes[2] = cogl_attribute_new (attribute_buffer,
                                      "cogl_tex_coord1_in",
                                      sizeof (Vertex),
                                      offsetof (Vertex, s),
                                      2,
                                      COGL_ATTRIBUTE_TYPE_FLOAT);
  attributes[3] = cogl_attribute_new (attribute_buffer,
                                      "cogl_normal_in",
                                      sizeof (Vertex),
                                      offsetof (Vertex, normal),
                                      3,
                                      COGL_ATTRIBUTE_TYPE_FLOAT);

  cogl_object_unref (attribute_buffer);

  prim = cogl_primitive_new_with_attributes (mode, n_vertices,
                                             attributes,
                                             4);

  for (i = 0; i < 3; i++)
    cogl_object_unref (attributes[i]);

  return prim;
}

static CoglAttribute *
create_circle (CoglContext *ctx, int subdivisions)
{
  int n_verts = subdivisions + 1;
  struct CircleVert {
      float x, y;
  } *verts;
  size_t buffer_size = sizeof (struct CircleVert) * n_verts;
  int i;
  CoglAttributeBuffer *attribute_buffer;
  CoglAttribute *attribute;

  verts = alloca (buffer_size);

  verts[0].x = 0;
  verts[0].y = 0;
  for (i = 1; i < n_verts; i++)
    {
      float angle =
        2.0f * (float)G_PI * (1.0f/(float)subdivisions) * (float)i;
      verts[i].x = sinf (angle);
      verts[i].y = cosf (angle);
    }
  attribute_buffer = cogl_attribute_buffer_new (ctx, buffer_size, verts);

  attribute = cogl_attribute_new (attribute_buffer,
                                  "cogl_position_in",
                                  sizeof (struct CircleVert),
                                  offsetof (struct CircleVert, x),
                                  2,
                                  COGL_ATTRIBUTE_TYPE_FLOAT);
  return attribute;
}

static CoglTexture *
create_circle_texture (Data *data)
{
  CoglTexture2D *tex2d;
  CoglOffscreen *offscreen;
  CoglFramebuffer *fb;
  CoglPipeline *white_pipeline;

  tex2d = cogl_texture_2d_new_with_size (data->ctx,
                                         TEXTURE_SIZE, TEXTURE_SIZE,
                                         COGL_PIXEL_FORMAT_RGBA_8888,
                                         NULL);
  offscreen = cogl_offscreen_new_to_texture (COGL_TEXTURE (tex2d));
  fb = COGL_FRAMEBUFFER (offscreen);

  data->circle = create_circle (data->ctx, 360);

  cogl_framebuffer_clear4f (fb, COGL_BUFFER_BIT_COLOR, 0, 0, 0, 0);

  cogl_framebuffer_identity_matrix (fb);

  white_pipeline = cogl_pipeline_new (data->ctx);
  cogl_pipeline_set_color4f (white_pipeline, 1, 1, 1, 1);

  cogl_framebuffer_draw_attributes (fb,
                                    white_pipeline,
                                    COGL_VERTICES_MODE_TRIANGLE_FAN,
                                    0, /* first vertex */
                                    361, /* n_vertices */
                                    &data->circle,
                                    1); /* n_attributes */

  cogl_object_unref (white_pipeline);
  cogl_object_unref (offscreen);

  return COGL_TEXTURE (tex2d);
}

static gboolean
paint_cb (void *user_data)
{
  Data *data = user_data;

  paint (data->fb, user_data);

  cogl_onscreen_swap_buffers (COGL_ONSCREEN (data->fb));
  /* If the driver can deliver swap complete events then we can remove
   * the idle paint callback until we next get a swap complete event
   * otherwise we keep the idle paint callback installed and simply
   * paint as fast as the driver will allow... */
  if (cogl_has_feature (data->ctx, COGL_FEATURE_ID_SWAP_BUFFERS_EVENT))
    return FALSE; /* remove the callback */
  else
    return TRUE;
}

static void
swap_complete_cb (CoglFramebuffer *framebuffer, void *user_data)
{
  g_idle_add (paint_cb, user_data);
}

int
main (int argc, char **argv)
{
  GError *error = NULL;
  Data data;
  CoglDepthState depth_state;
  CoglSnippet *snippet;
  int i;
  CoglPipeline *template_pipeline;
  GSource *cogl_source;
  GMainLoop *loop;

  data.ctx = cogl_context_new (NULL, &error);
  if (!data.ctx) {
      fprintf (stderr, "Failed to create context: %s\n", error->message);
      return 1;
  }

  data.fb = create_framebuffer (&data, 640, 480);
  if (!data.fb)
    return 1;

  /* Rectangle indices allow us to feed the GPU triangles even though
   * our vertex data is for quads... */
  data.indices = cogl_get_rectangle_indices (data.ctx, 6 /* n_rectangles */);
  data.prim = primitive_new_p3t2t2n3 (data.ctx,
                                      COGL_VERTICES_MODE_TRIANGLES,
                                      G_N_ELEMENTS (vertices),
                                      vertices);
  /* Each face will have 6 indices (2 triangles) so we have 6 * 6
   * indices in total... */
  cogl_primitive_set_indices (data.prim,
                              data.indices,
                              6 * 6);

  data.color_texture = cogl_texture_new_from_file (COGL_EXAMPLES_DATA "leopard.jpg",
                                                   COGL_TEXTURE_NO_SLICING,
                                                   COGL_PIXEL_FORMAT_ANY,
                                                   NULL);
  if (!data.color_texture)
    g_error ("Failed to load texture");

#ifdef USE_SHAVE_TEXTURE
  data.shave_texture = cogl_texture_new_from_file (COGL_EXAMPLES_DATA "shave.jpg",
                                                    COGL_TEXTURE_NO_SLICING,
                                                    COGL_PIXEL_FORMAT_ANY,
                                                    NULL);
  if (!data.shave_texture)
    g_error ("Failed to load texture");
#endif

  data.circle_texture = create_circle_texture (&data);

  init_shell_textures (&data);

  data.cube_pipeline = cogl_pipeline_new (data.ctx);
  cogl_pipeline_set_color4ub (data.cube_pipeline, 0x53, 0x32, 0x09, 0xff);

  cogl_depth_state_init (&depth_state);
  cogl_depth_state_set_test_enabled (&depth_state, TRUE);
  cogl_depth_state_set_write_enabled (&depth_state, TRUE);
  cogl_pipeline_set_depth_state (data.cube_pipeline, &depth_state, NULL);

  template_pipeline = cogl_pipeline_new (data.ctx);

  /* TODO: Consider adding a mechanism that lets developers request
   * that specific attributes be read-write. */
  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_VERTEX_TRANSFORM,
                              /* declarations */
                              "uniform float hair_pos;\n" /* range [0,length] */
                              "uniform float progress;\n" /* range [0,1] */
                              "uniform vec4 light_dir;\n"
                              "uniform vec4 slant;\n"

                              "float tip_flop = " TIP_FLOP_STR ";\n"
                              "vec4 normal4;\n"
                              "vec4 hair_force_dir = vec4 (0.0, -1.0, 0.0, 0.0);\n"
                              "float force_factor;\n"

                              "varying vec4 world_normal;\n",
                              NULL); /* pre */

  cogl_snippet_set_replace (snippet,
                            "  normal4 = vec4 (cogl_normal_in, 0.0);\n"
                            "  cogl_position_out = cogl_position_in + normal4 * "
                            "    (hair_pos + "
                            "      ((slant.x - cogl_tex_coord0_in.s) * slant.y) +"
                            "      ((slant.z - cogl_tex_coord0_in.t) * slant.w));\n"
                            "  cogl_position_out = cogl_modelview_projection_matrix * cogl_position_out;\n"
#if 1
                            "  force_factor = pow (progress, 3.0) * tip_flop;\n"
                            "  cogl_position_out += hair_force_dir * force_factor;\n"
#endif
                            "  world_normal = cogl_modelview_matrix * normal4;\n");

  cogl_pipeline_add_snippet (template_pipeline, snippet);
  cogl_object_unref (snippet);

  /* We need to make sure that we don't write transparent fragments to the
   * depth buffer... */
  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
                              /* declarations */
#ifdef USE_SHAVE_TEXTURE
                              "unform float progress;\n"
#endif
                              "uniform vec4 light_dir;\n"
                              "varying vec4 world_normal;\n"
                              "vec4 ambient = vec4 (0.2, 0.2, 0.2, 0.0);\n",

                              /* code */
                              "  if (cogl_color_out.a < 0.25) discard;\n"
#ifdef USE_SHAVE_TEXTURE
                              /* FIXME: this relies in internal detail of cogl's codegen... */
                              "  if (texture2D (cogl_sampler2, cogl_tex_coord_in[2].st).r > progress))\n"
                              "    discard;\n"
#endif
                              "  cogl_color_out += dot (light_dir, world_normal) * ambient;\n");

  cogl_pipeline_add_snippet (template_pipeline, snippet);
  cogl_object_unref (snippet);


  /* Since the box is made of multiple triangles that will overlap
   * when drawn and we don't control the order they are drawn in, we
   * enable depth testing to make sure that triangles that shouldn't
   * be visible get culled by the GPU. */
  cogl_depth_state_init (&depth_state);
  cogl_depth_state_set_test_enabled (&depth_state, TRUE);

  cogl_pipeline_set_depth_state (template_pipeline, &depth_state, NULL);


#ifdef USE_COLOR_TEXTURE
  cogl_pipeline_set_layer_texture (template_pipeline, 1, data.color_texture);
#endif

#ifdef USE_SHAVE_TEXTURE
  cogl_pipeline_set_layer_texture (template_pipeline, 2, data.shave_texture);
  /* We want the layer combining to skip this layer since we just want to
   * refer to the texture in our glsl snippet... */
  /* XXX: double check that cogl-pipeline-fragend-glsl.c doesn't emit code
   * for REPLACE (PREVIOUS) */
  /* FIXME: we get a very unhelpful error if we put a semicolon at the end
   * of a blend string... */
  cogl_pipeline_set_layer_combine (template_pipeline,
                                   2, "RGBA=REPLACE(PREVIOUS)", NULL);
#endif

  /* Create a pipeline for each texture we have to avoid modifying pipelines... */
  /* XXX: Can we define that the semantics for copying a pipeline are that
   * uniform locations for the new pipeline are the same as for the parent
   * except they might change if anything other than uniform values are
   * changed. */
  for (i = 0; i < N_TEXTURES; i++)
    {
      data.crate_pipelines[i] = cogl_pipeline_copy (template_pipeline);
      cogl_pipeline_set_layer_texture (data.crate_pipelines[i], 0, data.textures[i]);
      data.hair_pos_locations[i] =
        cogl_pipeline_get_uniform_location (data.crate_pipelines[i], "hair_pos");
      data.progress_locations[i] =
        cogl_pipeline_get_uniform_location (data.crate_pipelines[i], "progress");
      data.light_dir_locations[i] =
        cogl_pipeline_get_uniform_location (data.crate_pipelines[i], "light_dir");
      data.slant_locations[i] =
        cogl_pipeline_get_uniform_location (data.crate_pipelines[i], "slant");
    }




  cogl_source = cogl_glib_source_new (data.ctx, G_PRIORITY_DEFAULT);

  g_source_attach (cogl_source, NULL);

  if (cogl_has_feature (data.ctx, COGL_FEATURE_ID_SWAP_BUFFERS_EVENT))
    cogl_onscreen_add_swap_buffers_callback (COGL_ONSCREEN (data.fb),
                                             swap_complete_cb, &data);

  g_idle_add (paint_cb, &data);

  data.timer = g_timer_new ();

  loop = g_main_loop_new (NULL, TRUE);
  g_main_loop_run (loop);

  return 0;
}

