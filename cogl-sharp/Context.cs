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
    public sealed partial class Context : Cogl.Object
    {

        public Context(IntPtr h) : base(h) {}

        [DllImport("cogl2.dll")]
        private static extern IntPtr cogl_context_new(IntPtr display,
                                                      out IntPtr error);

        public Context()
        {
            IntPtr error;

            handle = cogl_context_new(IntPtr.Zero, out error);
            if (error != IntPtr.Zero)
                throw new Cogl.Exception(error);
        }

        public Context(Display renderer)
        {
            IntPtr r, error;

            if (renderer == null)
                r = IntPtr.Zero;
            else
                r = renderer.Handle;

            handle = cogl_context_new(r, out error);
            if (error != IntPtr.Zero)
                throw new Cogl.Exception(error);
        }
    }
}
