/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifndef __COGL_JOURNAL_PRIVATE_H
#define __COGL_JOURNAL_PRIVATE_H

#include "cogl.h"
#include "cogl-handle.h"
#include "cogl-clip-stack.h"
#include "cogl-queue.h"
#include "cogl-loose-region.h"

#define COGL_JOURNAL_VBO_POOL_SIZE 8

typedef struct _CoglJournalEntry CoglJournalEntry;

COGL_TAILQ_HEAD (CoglJournalEntryList, CoglJournalEntry);

typedef struct _CoglJournal
{
  CoglObject _parent;

  size_t needed_vbo_len;
  size_t journal_len;

  /* An array of batches of journal entries that have an equal
     pipeline. Each entry is a CoglJournalBatch */
  GArray *batches;

  /* A pool of attribute buffers is used so that we can avoid repeatedly
     reallocating buffers. Only one of these buffers at a time will be
     used by Cogl but we keep more than one alive anyway in case the
     GL driver is internally using the buffer and it would have to
     allocate a new one when we start writing to it */
  CoglAttributeBuffer *vbo_pool[COGL_JOURNAL_VBO_POOL_SIZE];
  /* The next vbo to use from the pool. We just cycle through them in
     order */
  unsigned int next_vbo_in_pool;

  int fast_read_pixel_count;

} CoglJournal;

typedef struct _CoglJournalBatch
{
  /* The pipeline used for these entries */
  CoglPipeline *pipeline;
  /* List of entries */
  CoglJournalEntryList entries;
  /* The region covered by this batch in screen space */
  CoglLooseRegion region;
} CoglJournalBatch;

/* To improve batching of geometry when submitting vertices to OpenGL we
 * log the texture rectangles we want to draw to a journal, so when we
 * later flush the journal we aim to batch data, and gl draw calls. */
struct _CoglJournalEntry
{
  /* Each entry stored as a singly-linked list of entries within the
     same batch */
  COGL_TAILQ_ENTRY (CoglJournalEntry) batch;

  CoglClipStack *clip_stack;

  CoglMatrix model_view;

  int n_layers;

  guint8 color[4];
  float position[4];

  /* Transformed vertex positions. These are needed to calculate the
     bounding box of the primitive and also to upload the vertices
     seeing as we do software transformation. By storing them we avoid
     calculating the them twice but take a hit in memory
     consumption. */
  float transformed_verts[4 * 3];

  /* Texture coordinates. The structure is over allocated to include a
     variable number of these. This must be the last entry in the
     struct */
  float tex_coords[1 /* (4 * n_layers) */];

  /* XXX: These entries are pretty big now considering the padding in
   * CoglPipelineFlushOptions and CoglMatrix, so we might need to optimize this
   * later. */
};

CoglJournal *
_cogl_journal_new (void);

void
_cogl_journal_log_quad (CoglJournal  *journal,
                        const float  *position,
                        CoglPipeline *pipeline,
                        int           n_layers,
                        CoglTexture  *layer0_override_texture,
                        const float  *tex_coords,
                        unsigned int  tex_coords_len);

void
_cogl_journal_flush (CoglJournal *journal,
                     CoglFramebuffer *framebuffer);

void
_cogl_journal_discard (CoglJournal *journal);

gboolean
_cogl_journal_all_entries_within_bounds (CoglJournal *journal,
                                         float clip_x0,
                                         float clip_y0,
                                         float clip_x1,
                                         float clip_y1);

gboolean
_cogl_journal_try_read_pixel (CoglJournal *journal,
                              int x,
                              int y,
                              CoglPixelFormat format,
                              guint8 *pixel,
                              gboolean *found_intersection);

#endif /* __COGL_JOURNAL_PRIVATE_H */
