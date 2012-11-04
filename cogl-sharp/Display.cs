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
    public partial class Display : Cogl.Object
    {
        public Display(IntPtr h) : base(h) {}

        [DllImport("cogl2.dll")]
        private static extern IntPtr cogl_display_new(IntPtr renderer,
                                                      IntPtr template);

        public Display()
        {
            handle = cogl_display_new(IntPtr.Zero, IntPtr.Zero);
        }

        public Display(Renderer renderer, OnScreenTemplate template)
        {
            IntPtr r, t;

            if (renderer == null)
                r = IntPtr.Zero;
            else
                r = renderer.Handle;

            if (template == null)
                t = IntPtr.Zero;
            else
                t = template.Handle;

            handle = cogl_display_new(r, t);
        }

        [DllImport("cogl2.dll")]
        private static extern bool cogl_display_setup(IntPtr display,
                                                      out IntPtr error);

        public void Setup()
        {
            IntPtr error;

            cogl_display_setup(handle, out error);
            if (error != IntPtr.Zero)
                throw new Cogl.Exception(error);
        }
    }
}
