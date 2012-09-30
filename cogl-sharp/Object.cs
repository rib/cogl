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
    public class Object : IDisposable
    {
        protected IntPtr handle;

        [DllImport("cogl2.dll")]
        private static extern IntPtr cogl_object_ref(IntPtr o);

        [DllImport("cogl2.dll")]
        private static extern void cogl_object_unref(IntPtr o);

        public Object() { }

        public Object(IntPtr h)
        {
            handle = h;
        }

        public void Dispose()
        {
            cogl_object_unref(handle);
        }

        public IntPtr Handle
        {
            get { return handle; }
        }
    }
}
