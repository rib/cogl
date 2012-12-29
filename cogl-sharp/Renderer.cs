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

        /* XXX: XlibRenderer subclass? */
#if COGL_HAS_XLIB_SUPPORT
        [DllImport("cogl2.dll")]
        private static extern IntPtr
        cogl_xlib_renderer_get_display(IntPtr renderer);

        public IntPtr XlibGetDisplay()
        {
            return cogl_xlib_renderer_get_display(handle);
        }

        [DllImport("cogl2.dll")]
        private static extern void
        cogl_xlib_renderer_set_foreign_display(IntPtr renderer, IntPtr display);

        public void XlibSetForeignDisplay(IntPtr display)
        {
            cogl_xlib_renderer_set_foreign_display(handle, display);
        }

        [DllImport("cogl2.dll")]
        private static extern IntPtr
        cogl_xlib_renderer_get_foreign_display(IntPtr renderer);

        public IntPtr XlibGetForeignDisplay()
        {
            return cogl_xlib_renderer_get_foreign_display(handle);
        }

        [DllImport("cogl2.dll")]
        private static extern void
        cogl_xlib_renderer_set_event_retrieval_enabled(IntPtr renderer,
                                                       bool enable);

        public void XlibSetEventRetrievalEnabled(bool enable)
        {
            cogl_xlib_renderer_set_event_retrieval_enabled(handle, enable);
        }

        [DllImport("cogl2.dll")]
        private static extern void
        cogl_xlib_renderer_handle_event(IntPtr renderer, IntPtr _event);

        public void XlibHandleEvent(IntPtr _event)
        {
            cogl_xlib_renderer_handle_event(handle, _event);
        }
#endif

    }
}
