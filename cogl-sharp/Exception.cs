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
    [StructLayout(LayoutKind.Sequential)]
    internal struct Error
    {
        public uint domain;
        public int code;
        public IntPtr message;
    }

    public class Exception : System.Exception
    {
        IntPtr error;

        public Exception(IntPtr error) : base()
        {
            this.error = error;
        }

        public int Code
        {
            get
            {
                Error err = (Error)Marshal.PtrToStructure(error, typeof(Error));
                return err.code;
            }
        }

        public uint Domain
        {
            get
            {
                Error err = (Error)Marshal.PtrToStructure(error, typeof(Error));
                return err.domain;
            }
        }

        public override string Message
        {
            get
            {
                Error err = (Error)Marshal.PtrToStructure(error, typeof(Error));
                return Marshaller.Utf8PtrToString(err.message);
            }
        }

        [DllImport("cogl2.dll")]
        static extern void cogl_error_free(IntPtr error);
        ~Exception()
        {
            cogl_error_free(error);
        }
    }
}
