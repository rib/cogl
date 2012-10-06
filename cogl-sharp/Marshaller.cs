/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2012 Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;Vjg
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

namespace Cogl {

    internal sealed class Marshaller
    {
        private Marshaller () {}

        private static unsafe int strlen(IntPtr s)
        {
            int len = 0;
            byte *b = (byte *)s;

            while (*b != 0)
            {
                b++;
                len++;
            }

            return len;
        }

        public static string Utf8PtrToString(IntPtr ptr)
        {
            if (ptr == IntPtr.Zero)
                return null;

            int len = strlen(ptr);
            byte[] bytes = new byte[len];
            Marshal.Copy(ptr, bytes, 0, len);
            return System.Text.Encoding.UTF8.GetString(bytes);
        }

        /* Caller needs to free the newly allocated buffer with
         * System.Marshal.FreeHGlobal() */
        public static IntPtr StringToUtf8Ptr(string str)
        {
            byte[] bytes = System.Text.Encoding.UTF8.GetBytes(str);
            IntPtr utf8 = Marshal.AllocHGlobal(bytes.Length + 1 /* '\0 */);
            Marshal.Copy(bytes, 0, utf8, bytes.Length);
            Marshal.WriteByte(utf8, bytes.Length, 0);
            return utf8;
        }

    }
}
