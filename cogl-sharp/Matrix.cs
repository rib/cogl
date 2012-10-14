/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2012 Intel Corporation.
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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Aaron Bockover <abockover@novell.com>
 *   Damien Lespiau <damien.lespiau@intel.com>
 */

using System;
using System.Runtime.InteropServices;

namespace Cogl
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Matrix
    {
        /* column 0 */
        public float xx;
        public float yx;
        public float zx;
        public float wx;

        /* column 1 */
        public float xy;
        public float yy;
        public float zy;
        public float wy;

        /* column 2 */
        public float xz;
        public float yz;
        public float zz;
        public float wz;

        /* column 3 */
        public float xw;
        public float yw;
        public float zw;
        public float ww;

        /* Those fields are only accessed from the native side, so let's disable the
         * unused warning */
#pragma warning disable 169
        private UInt16 type;
        private UInt16 flags;
#pragma warning restore 169

        public float[] GetArray()
        {
            throw new NotImplementedException();
        }

        [DllImport("cogl2.dll")]
        private static extern void cogl_matrix_init_identity(ref Matrix matrix);

        public void InitIdentity()
        {
            cogl_matrix_init_identity(ref this);
        }

        [DllImport("cogl2.dll")]
        private static extern void cogl_matrix_init_translation(ref Matrix matrix,
                                                                float tx,
                                                                float ty,
                                                                float tz);

        public void InitTranslation(float tx, float ty, float tz)
        {
            cogl_matrix_init_translation(ref this, tx, ty, tz);
        }

        [DllImport("cogl2.dll")]
        private static extern void cogl_matrix_init_from_array(ref Matrix matrix,
                                                               float[] array);

        public void InitFromArray(float[] array)
        {
            cogl_matrix_init_from_array(ref this, array);
        }

        [DllImport("cogl2.dll")]
        private static extern void cogl_matrix_rotate(ref Matrix matrix,
                                                      float angle,
                                                      float x, float y, float z);

        public void Rotate(float angle, float x, float y, float z)
        {
            cogl_matrix_rotate(ref this, angle, x, y, z);
        }

        [DllImport("cogl2.dll")]
        private static extern void cogl_matrix_frustum(ref Matrix matrix,
                                                       float left, float right,
                                                       float bottom, float top,
                                                       float z_near, float z_far);

        public void Frustum(float left, float right,
                            float bottom, float top,
                            float z_near, float z_far)
        {
            cogl_matrix_frustum(ref this, left, right, bottom, top, z_near, z_far);
        }

        [DllImport("cogl2.dll")]
        private static extern void cogl_matrix_transform_point(ref Matrix matrix,
                                                               out float x,
                                                               out float y,
                                                               out float z,
                                                               out float w);

        public void TransformPoint(out float x, out float y, out float z, out float w)
        {
            cogl_matrix_transform_point(ref this, out x, out y, out z, out w);
        }

        [DllImport("cogl2.dll")]
        private static extern void cogl_matrix_multiply(ref Matrix matrix,
                                                        ref Matrix a,
                                                        ref Matrix b);

        public void Multiply(Matrix a, Matrix b)
        {
            cogl_matrix_multiply(ref this, ref a, ref b);
        }

        [DllImport("cogl2.dll")]
        private static extern void cogl_matrix_translate(ref Matrix matrix,
                                                         float x,
                                                         float y,
                                                         float z);

        public void Translate(float x, float y, float z)
        {
            cogl_matrix_translate(ref this, x, y, z);
        }

        [DllImport ("cogl2.dll")]
        private static extern void cogl_matrix_scale(ref Matrix matrix,
                                                     float sx, float sy, float sz);

        public void Scale(float sx, float sy, float sz)
        {
            cogl_matrix_scale(ref this, sx, sy, sz);
        }

        [DllImport ("cogl2.dll")]
        private static extern int cogl_matrix_get_inverse(ref Matrix matrix,
                                                          ref Matrix inverse);

        public bool GetInverse(ref Matrix inverse)
        {
           return  cogl_matrix_get_inverse(ref this, ref inverse) == 0 ? false : true;
        }
    }
}
