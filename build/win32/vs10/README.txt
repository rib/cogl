Note that all this is rather experimental.

This VS10 solution and the projects it includes are intented to be used
in a Cogl source tree unpacked from a tarball. In a git checkout you
first need to use some Unix-like environment or manual work to expand
the files needed, like config.h.win32.in into config.h.win32 and the
.vcprojin files here into corresponding actual .vcproj files.

You will need the parts from GNOME: GDK-Pixbuf, Pango** and GLib.
External dependencies are at least zlib, libpng,
gettext-runtime** and Cairo**, and glext.h from
http://www.opengl.org/registry/api/glext.h (which need to be in the GL folder
in your include directories or in <root>\vs10\<PlatformName>\include\GL).

Please see the README file in the root directory of this Cogl source package
for the versions of the dependencies required.  See also
build/win32/vs10/README.txt in the GLib source package for details
where to unpack them.  It is recommended that at least the dependencies
from GNOME are also built with VS10 to avoid crashes caused by mixing different
CRTs-please see also the build/win32/vs10/README.txt in those respective packages.

The recommended build sequence of the dependencies are as follows (the non-GNOME
packages that are not downloaded as binaries from ftp://ftp.gnome.org have
makefiles and/or VS project files that can be used to compile with VS directly,
except the optional PCRE, which is built on VS using CMake; GLib & ATK-2.x have
VS10 project files in the latest stable versions, GDK-Pixbuf have VS10 project files
in the latest unstable version, and Pango should have VS10 project files
in the next unstable release):
-Unzip the binary packages for gettext-runtime, freetype, expat and fontconfig
 downloaded from ftp://ftp.gnome.org**
-zlib
-libpng
-(optional for GLib) PCRE (8.12 or later, building PCRE using CMake is
 recommended-please see build/win32/vs10/README.txt in the GLib source package)
-(for gdk-pixbuf, if GDI+ is not to be used) IJG JPEG
-(for gdk-pixbuf, if GDI+ is not to be used) jasper [JPEG-2000 library]
-(for gdk-pixbuf, if GDI+ is not to be used, requires zlib and IJG JPEG) libtiff
-GLib
-Cairo
-Pango
-GDK-Pixbuf

The "install" project will copy build results and headers into their
appropriate location under <root>\vs10\<PlatformName>. For instance,
built DLLs go into <root>\vs10\<PlatformName>\bin, built LIBs into
<root>\vs10\<PlatformName>\lib and Cogl headers into
<root>\vs10\<PlatformName>\include\Cogl-2.0.

*Regarding ATK-2.x: prior to compiling ATK-2.0.0, please open atkprops
 in VS under "Properties Manager" view (it is under any one of the
 build configurations, right-click on atkprops and select "Properties").
 Navigate to "User Macros", and edit the following fields:
 AtkApiVersion -> 2.0
 AtkLibToolCompatibleDllSuffix -> -2.0-0
 AtkSeperateVS10DLLSuffix -> -2-vs10
 Sorry this change did not make it upstream prior to ATK-2.0.0 release-
 this will be in the subsequent releases of ATK-2.x and was committed
 upstream.
 
**There is no known official VS10 build support for fontconfig
  (required for Pango and Pango at the moment-I will see whether this
  requirement can be made optional for VS builds)
  (along with freetype and expat) and gettext-runtime, so
  please use the binaries from: 

  ftp://ftp.gnome.org/pub/GNOME/binaries/win32/dependencies/ (32 bit)
  ftp://ftp.gnome.org/pub/GNOME/binaries/win64/dependencies/ (64 bit)

--Chun-wei Fan <fanc999@yahoo.com.tw>
  (Adopted from the GTK+ Win32 VS README.txt file originally by Tor Lillqvist)
