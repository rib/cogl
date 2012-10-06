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
 *   Damien Lespiau <damien.lespiau@intel.com>
 */

using System;
using System.Runtime.InteropServices;

namespace Cogl
{
    public partial class Texture : Cogl.Object
    {
        public Texture(IntPtr h) : base(h) {}
        public Texture() {}

        [DllImport("cogl2.dll")]
        public static extern IntPtr
        cogl_texture_new_from_file(IntPtr s,
                                   TextureFlags flags,
                                   PixelFormat internal_format,
                                   out IntPtr error);

        public Texture(string filename,
                       TextureFlags flags = TextureFlags.None,
                       PixelFormat internal_format = PixelFormat.Any)
        {
            IntPtr filename_utf8, error;

            filename_utf8 = Marshaller.StringToUtf8Ptr(filename);
            handle = cogl_texture_new_from_file(filename_utf8, flags,
                                           internal_format, out error);
            Marshal.FreeHGlobal(filename_utf8);

            if (error != IntPtr.Zero)
                throw new Cogl.Exception(error);
        }
    }
}
