#!/bin/bash
###############################################################################
# Build an MSan-instrumented libc++ and libc++abi runtime for iccDEV QA.
###############################################################################

set -euo pipefail

usage()
{
  echo "Usage: $0 --prefix DIR [--llvm-commit SHA] [--jobs N]"
}

prefix=""
llvm_commit="1ab49a973e210e97d61e5db6557180dcb92c3e98"
jobs="${BUILD_JOBS:-$(nproc)}"

while [ "$#" -gt 0 ]; do
  case "$1" in
    --prefix)
      [ "$#" -ge 2 ] || { usage >&2; exit 2; }
      prefix="$2"
      shift 2
      ;;
    --llvm-commit)
      [ "$#" -ge 2 ] || { usage >&2; exit 2; }
      llvm_commit="$2"
      shift 2
      ;;
    --jobs)
      [ "$#" -ge 2 ] || { usage >&2; exit 2; }
      jobs="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "[FAIL] unknown argument: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

if [ -z "$prefix" ] || [ "$prefix" = "/" ]; then
  echo "[FAIL] --prefix must name a non-root installation directory" >&2
  exit 2
fi
case "$llvm_commit" in
  *[!0-9a-f]*|'')
    echo "[FAIL] --llvm-commit must be a lowercase hexadecimal commit" >&2
    exit 2
    ;;
esac
if [ "${#llvm_commit}" -ne 40 ]; then
  echo "[FAIL] --llvm-commit must contain exactly 40 hexadecimal characters" >&2
  exit 2
fi
case "$jobs" in
  ''|0*|*[!0-9]*)
    echo "[FAIL] --jobs must be a positive integer" >&2
    exit 2
    ;;
esac

for required_tool in clang clang++ cmake git ninja nproc; do
  if ! command -v "$required_tool" >/dev/null 2>&1; then
    echo "[FAIL] required tool is unavailable: $required_tool" >&2
    exit 127
  fi
done

work_dir="$(mktemp -d "${TMPDIR:-/tmp}/iccdev-msan-libcxx.XXXXXX")"
cleanup()
{
  rm -rf -- "$work_dir"
}
trap cleanup EXIT

source_dir="$work_dir/llvm-project"
build_dir="$work_dir/build"

git init --quiet "$source_dir"
git -C "$source_dir" remote add origin https://github.com/llvm/llvm-project.git
git -C "$source_dir" sparse-checkout init --cone
git -C "$source_dir" sparse-checkout set \
  runtimes libcxx libcxxabi libc cmake llvm/cmake \
  llvm/utils/gn/secondary llvm/utils/llvm-lit
git -C "$source_dir" fetch --quiet --depth=1 --filter=blob:none origin "$llvm_commit"
git -C "$source_dir" checkout --quiet --detach FETCH_HEAD
if [ "$(git -C "$source_dir" rev-parse HEAD)" != "$llvm_commit" ]; then
  echo "[FAIL] fetched LLVM commit does not match the requested revision" >&2
  exit 2
fi

cmake -G Ninja -S "$source_dir/runtimes" -B "$build_dir" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_INSTALL_PREFIX="$prefix" \
  -DLLVM_ENABLE_RUNTIMES='libcxx;libcxxabi' \
  -DLLVM_USE_SANITIZER=MemoryWithOrigins \
  -DLIBCXX_ENABLE_SHARED=ON \
  -DLIBCXX_ENABLE_STATIC=OFF \
  -DLIBCXXABI_ENABLE_SHARED=ON \
  -DLIBCXXABI_ENABLE_STATIC=OFF \
  -DLIBCXXABI_USE_LLVM_UNWINDER=OFF \
  -DLIBCXX_INCLUDE_TESTS=OFF \
  -DLIBCXXABI_INCLUDE_TESTS=OFF \
  -DLIBCXX_INCLUDE_BENCHMARKS=OFF

cmake --build "$build_dir" --target install-cxx install-cxxabi \
  --parallel "$jobs"

if [ ! -f "$prefix/include/c++/v1/string" ] ||
   [ ! -f "$prefix/lib/libc++.so.1" ] ||
   [ ! -f "$prefix/lib/libc++abi.so.1" ]; then
  echo "[FAIL] instrumented libc++ installation is incomplete: $prefix" >&2
  exit 2
fi
for runtime_library in libc++.so.1 libc++abi.so.1; do
  nm -D "$prefix/lib/$runtime_library" > "$work_dir/$runtime_library.symbols"
  if ! grep -Fq '__msan_' "$work_dir/$runtime_library.symbols"; then
    echo "[FAIL] $runtime_library does not reference MemorySanitizer" >&2
    exit 2
  fi
done

echo "[PASS] MSan libc++/libc++abi installed at $prefix"
echo "[EVIDENCE] llvm_commit=$llvm_commit"
