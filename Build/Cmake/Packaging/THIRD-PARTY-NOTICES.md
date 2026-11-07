# Third-party notices

Native packages use system libraries declared by the package manager.

The Linux and Windows tool bundles and the WASM archive include matching
dependency notices under `third-party-licenses/`, collected from the installed
Debian or vcpkg packages or from the pinned WASM dependency sources. The
dependency set may include libheif and its codec libraries, libtiff (including
BigTIFF support), libjpeg-turbo or libjpeg, libpng, zlib, libxml2,
nlohmann-json, and wxWidgets when the GUI tool is built. The macOS archive does
not redistribute Homebrew packages; their licenses remain with the separately
installed formulae.

Redistributors must keep those upstream notices with any bundled third-party
binaries.
