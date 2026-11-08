# Third-party notices

Native packages use system libraries declared by the package manager.

The Linux and Windows tool bundles and the WASM archive include matching
dependency notices under `third-party-licenses/`, collected from the installed
Debian or vcpkg packages or from the pinned WASM dependency sources. The
dependency set may include libheif and its codec libraries, libtiff (including
BigTIFF support), libjpeg-turbo or libjpeg, libpng, zlib, libxml2,
nlohmann-json, and wxWidgets when the GUI tool is built. Linux bundles also
include the Debian common-license texts referenced by package copyright files.

The macOS tool archive does not copy Homebrew dynamic libraries; their
licenses remain with the separately installed formulae. The Apple mobile
archive contains static cores without Homebrew dynamic libraries. Both
include nlohmann-json notices because that header-only implementation is
compiled into IccJSON. The macOS, Apple mobile, Windows, and WASM bundles
retain nlohmann-json's root license and the attributed runtime headers that
identify additional copyright holders.

Redistributors must keep those upstream notices with any bundled third-party
binaries.

The Doxygen HTML archive includes browser assets emitted by Doxygen, including
MIT-licensed Doxygen scripts and jQuery. Their upstream copyright and license
notices remain in the generated JavaScript files.
