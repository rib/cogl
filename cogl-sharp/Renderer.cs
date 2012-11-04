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
    public partial class Renderer : Cogl.Object
    {
        public Renderer(IntPtr h) : base(h) {}

        [DllImport("cogl2.dll")]
        private static extern IntPtr cogl_renderer_new();

        public Renderer()
        {
            handle = cogl_renderer_new();
        }

        [DllImport("cogl2.dll")]
        private static extern bool
        cogl_renderer_check_onscreen_template(IntPtr renderer,
                                              IntPtr template,
                                              out IntPtr error);

        public void CheckOnScreenTemplate(OnScreenTemplate template)
        {
            IntPtr error;

            cogl_renderer_check_onscreen_template(handle,
                                                  template.Handle,
                                                  out error);
            if (error != IntPtr.Zero)
                throw new Cogl.Exception(error);
        }

        [DllImport("cogl2.dll")]
        private static extern bool cogl_renderer_connect(IntPtr renderer,
                                                         out IntPtr error);

        public void Connect()
        {
            IntPtr error;

            cogl_renderer_connect(handle, out error);
            if (error != IntPtr.Zero)
                throw new Cogl.Exception(error);
        }

    }
}
