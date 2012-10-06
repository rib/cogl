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
    public struct Quaternion
    {
        public float w;
        public float x;
        public float y;
        public float z;

        /* private */
	/* Those fields are only accessed from the native side, so let's
	 * disable the unused warning */
#pragma warning disable 169
        private float padding0;
        private float padding1;
        private float padding2;
        private float padding3;
#pragma warning restore 169

        [DllImport("cogl2.dll")]
        private static extern void cogl_quaternion_init_identity(ref Quaternion q);

        public void InitIdentity()
        {
            cogl_quaternion_init_identity(ref this);
        }

        [DllImport("cogl2.dll")]
        private static extern void cogl_quaternion_init(ref Quaternion q,
							float angle,
							float x, float y, float z);

        public void Init(float angle, float x, float y, float z)
        {
            cogl_quaternion_init(ref this, angle, x, y, z);
        }

        [DllImport("cogl2.dll")]
        private static extern void cogl_quaternion_init_from_x_rotation(ref Quaternion q, float angle);

        public void FromXRotation(float angle)
        {
            cogl_quaternion_init_from_x_rotation(ref this, angle);
        }

        [DllImport("cogl2.dll")]
        private static extern void cogl_quaternion_init_from_y_rotation(ref Quaternion q, float angle);

        public void FromYRotation(float angle)
        {
            cogl_quaternion_init_from_y_rotation(ref this, angle);
        }

        [DllImport("cogl2.dll")]
        private static extern void cogl_quaternion_init_from_z_rotation(ref Quaternion q, float angle);

        public void FromZRotation(float angle)
        {
            cogl_quaternion_init_from_z_rotation(ref this, angle);
        }

        [DllImport("cogl2.dll")]
        private static extern void cogl_quaternion_init_from_matrix(ref Quaternion q, ref Matrix m);

        public void FromMatrix(ref Matrix m)
        {
            cogl_quaternion_init_from_matrix(ref this, ref m);
        }

        [DllImport("cogl2.dll")]
        private static extern float cogl_quaternion_get_rotation_angle(ref Quaternion q);

        public float GetRotationAngle()
        {
            return cogl_quaternion_get_rotation_angle(ref this);
        }

        [DllImport("cogl2.dll")]
        private static extern float cogl_quaternion_normalize(ref Quaternion q);

	public void Normalize()
	{
	    cogl_quaternion_normalize(ref this);
	}

        [DllImport("cogl2.dll")]
        private static extern float cogl_quaternion_invert(ref Quaternion q);

	public void Invert()
	{
	    cogl_quaternion_invert(ref this);
	}

        [DllImport("cogl2.dll")]
        private static extern float cogl_quaternion_multiply(out Quaternion r,
							     ref Quaternion l,
							     ref Quaternion r);

	public static void Multiply(out Quaternion result,
				    ref Quaternion left,
				    ref Quaternion right)
	{
	    cogl_quaternion_multiply(out result, ref left, ref right);
	}

        [DllImport("cogl2.dll")]
        private static extern float cogl_quaternion_pow(ref Quaternion q,
							float exponent);

	public void Pow(float exponent)
	{
	    cogl_quaternion_pow(ref this, exponent);
	}
    }
}
