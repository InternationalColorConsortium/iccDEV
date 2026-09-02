###############################################################
#
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# Intent: unified iccDEV container image.
#
###############################################################

FROM ubuntu:26.04@sha256:678c6550cc43645e08669028bc177f50be4e7c5b8cca677067b1914d4afc7a03

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

ARG GIT_COMMIT=unknown

ENV DEBIAN_FRONTEND=noninteractive

LABEL org.opencontainers.image.title="iccDEV" \
      org.opencontainers.image.description="Ubuntu 26.04 iccDEV tools, MCP runtime, and maintainer QA image" \
      org.opencontainers.image.licenses="BSD-3-Clause" \
      org.opencontainers.image.vendor="International Color Consortium" \
      org.opencontainers.image.source="https://github.com/InternationalColorConsortium/iccDEV"

# Pebble is inherited from the Ubuntu base but unused by the unified image.
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
    afl++=4.33c-1.1ubuntu1 \
    bash=5.3-2ubuntu1 \
    binutils=2.46-3ubuntu2 \
    build-essential=12.12ubuntu2.26.04.2 \
    ca-certificates=20260601~26.04.1 \
    clang-21=1:21.1.8-6ubuntu1 \
    clang-22=1:22.1.2-1ubuntu1 \
    clang-tidy-22=1:22.1.2-1ubuntu1 \
    clang-tools-22=1:22.1.2-1ubuntu1 \
    cmake=4.2.3-2ubuntu2 \
    cppcheck=2.19.0-3 \
    curl=8.18.0-1ubuntu2.4 \
    diffutils=1:3.12-1 \
    file=1:5.46-5build2 \
    g++=4:15.2.0-5ubuntu1 \
    gcc=4:15.2.0-5ubuntu1 \
    gdb=17.1-2ubuntu1 \
    gdbserver=17.1-2ubuntu1 \
    gh=2.46.0-4 \
    git=1:2.53.0-1ubuntu1 \
    gcovr=7.2+really-2 \
    gpgv=2.4.8-4ubuntu3 \
    jq=1.8.1-4ubuntu2 \
    libclang-rt-22-dev=1:22.1.2-1ubuntu1 \
    libclang-rt-21-dev=1:21.1.8-6ubuntu1 \
    libjpeg-dev=8c-2ubuntu12 \
    libtiff-tools=4.7.0-3ubuntu5 \
    liblzma-dev=5.8.3-1 \
    libpng-dev=1.6.57-1 \
    libssl-dev=3.5.5-1ubuntu3.5 \
    libtiff-dev=4.7.0-3ubuntu5 \
    libwxgtk3.2-dev=3.2.9+dfsg-1 \
    zlib1g=1:1.3.dfsg+really1.3.1-1ubuntu3 \
    lld-22=1:22.1.2-1ubuntu1 \
    lldb-22=1:22.1.2-1ubuntu1 \
    libxml2-16=2.15.2+dfsg-0.1ubuntu0.1 \
    libxml2-dev=2.15.2+dfsg-0.1ubuntu0.1 \
    lcov=2.4-3 \
    lsb-release=12.1-2build1 \
    llvm-22=1:22.1.2-1ubuntu1 \
    llvm-22-tools=1:22.1.2-1ubuntu1 \
    make=4.4.1-3 \
    nano=8.7.1-1ubuntu0.1 \
    nlohmann-json3-dev=3.12.0.really.3.12.0.really.3.11.3-3build1 \
    openssl-provider-legacy=3.5.5-1ubuntu3.5 \
    pkg-config=2.5.1-4 \
    python3=3.14.3-0ubuntu2 \
    python3-dev=3.14.3-0ubuntu2 \
    python3-pip=25.1.1+dfsg-1ubuntu2 \
    python3-venv=3.14.3-0ubuntu2 \
    shellcheck=0.11.0-2 \
    strace=6.19+ds-0ubuntu5 \
    time=1.9-0.4 \
    valgrind=1:3.26.0-0ubuntu1 \
    wx-common=3.2.9+dfsg-1 \
    yamllint=1.37.1-1 \
    zlib1g-dev=1:1.3.dfsg+really1.3.1-1ubuntu3 \
 && rm -f /usr/bin/pebble \
 && rm -rf /var/lib/apt/lists/*

RUN update-alternatives --install /usr/bin/clang clang /usr/bin/clang-22 100 \
 && update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-22 100 \
 && update-alternatives --install /usr/bin/clang-tidy clang-tidy /usr/bin/clang-tidy-22 100 \
 && update-alternatives --install /usr/bin/ld.lld ld.lld /usr/bin/ld.lld-22 100 \
 && update-alternatives --install /usr/bin/lld lld /usr/bin/lld-22 100 \
 && update-alternatives --install /usr/bin/lld-link lld-link /usr/bin/lld-link-22 100 \
 && update-alternatives --install /usr/bin/lldb lldb /usr/bin/lldb-22 100 \
 && update-alternatives --install /usr/bin/llvm-cov llvm-cov /usr/bin/llvm-cov-22 100 \
 && update-alternatives --install /usr/bin/llvm-profdata llvm-profdata /usr/bin/llvm-profdata-22 100 \
 && update-alternatives --install /usr/bin/scan-build scan-build /usr/bin/scan-build-22 100 \
 && update-alternatives --install /usr/bin/llvm-symbolizer llvm-symbolizer /usr/bin/llvm-symbolizer-22 100 \
 && ln -s /usr/bin/nano /usr/local/bin/pico

ENV CC=clang \
    CXX=clang++ \
    ASAN_SYMBOLIZER_PATH=/usr/bin/llvm-symbolizer \
    ICCDEV_ROOT=/workspace/iccDEV \
    ICCDEV_BUILD_DIR=/workspace/build \
    ICCDEV_TOOLS_DIR=/workspace/build/Tools \
    ICCDEV_TESTING_DIR=/workspace/iccDEV/Testing \
    ICCDEV_MCP_PYTHON=/opt/iccdev-mcp/bin/python \
    ICCDEV_SPECTRAL_PREVIEW_PYTHON=/opt/iccdev-spectral-preview/bin/python \
    ICCDEV_BUILD_LABEL="iccDEV unified image" \
    ICCDEV_IMAGE_PULL="docker pull ghcr.io/internationalcolorconsortium/iccdev:latest" \
    PATH="/opt/iccdev-mcp/bin:${PATH}"

RUN groupadd --system iccdev-ci \
 && useradd --system --gid iccdev-ci --home-dir /workspace --create-home --shell /bin/bash iccdev-ci

COPY --chown=iccdev-ci:iccdev-ci .github/ci/requirements /workspace/iccDEV/.github/ci/requirements

RUN python3 -m venv /opt/iccdev-spectral-preview \
 && /opt/iccdev-spectral-preview/bin/python -m pip install \
      --upgrade \
      'pip>=25.1.1' \
      'setuptools>=78.1.1' \
      --quiet \
 && /opt/iccdev-spectral-preview/bin/python -m pip install \
      --only-binary=:all: \
      --no-cache-dir \
      -r /workspace/iccDEV/.github/ci/requirements/docker-spectral-preview.txt \
      --quiet \
 && /opt/iccdev-spectral-preview/bin/python -m pip check \
 && /opt/iccdev-spectral-preview/bin/python -c 'import imagecodecs,numpy,PIL,tifffile; print(f"imagecodecs {imagecodecs.__version__}"); print(f"numpy {numpy.__version__}"); print(f"pillow {PIL.__version__}"); print(f"tifffile {tifffile.__version__}")' \
 && rm -rf \
      /opt/iccdev-spectral-preview/bin/pip* \
      /opt/iccdev-spectral-preview/lib/python*/site-packages/pip \
      /opt/iccdev-spectral-preview/lib/python*/site-packages/pip-*.dist-info

RUN hadolint_version="2.15.1" \
 && hadolint_sha256="c7187db94eeeeca956519a6af171adc31453941a1e777961f6e680f697c8c507" \
 && curl -fsSLo /tmp/hadolint \
      "https://github.com/hadolint/hadolint/releases/download/v${hadolint_version}/hadolint-linux-x86_64" \
 && printf '%s  %s\n' "$hadolint_sha256" /tmp/hadolint | sha256sum -c - \
 && install -m 0755 /tmp/hadolint /usr/local/bin/hadolint \
 && rm -f /tmp/hadolint \
 && hadolint --version

RUN python3 -m venv /opt/iccdev-workflow-qa \
 && /opt/iccdev-workflow-qa/bin/python -m pip install \
      --upgrade \
      'pip>=25.1.1' \
      'setuptools>=78.1.1' \
      --quiet \
 && /opt/iccdev-workflow-qa/bin/python -m pip install \
      --only-binary=:all: \
      --no-cache-dir \
      -r /workspace/iccDEV/.github/ci/requirements/docker-workflow-qa.txt \
      --quiet \
 && /opt/iccdev-workflow-qa/bin/python -m pip check \
 && ln -s /opt/iccdev-workflow-qa/bin/zizmor /usr/local/bin/zizmor \
 && zizmor --version \
 && yamllint --version \
 && rm -rf \
      /opt/iccdev-workflow-qa/bin/pip* \
      /opt/iccdev-workflow-qa/lib/python*/site-packages/pip \
      /opt/iccdev-workflow-qa/lib/python*/site-packages/pip-*.dist-info

COPY --chown=iccdev-ci:iccdev-ci . /workspace/iccDEV
COPY --chmod=0755 .github/ci/docker/iccdev-banner.sh /usr/local/bin/iccdev-banner
COPY --chmod=0755 .github/ci/docker/iccdev-fuzz-env.sh /usr/local/bin/iccdev-fuzz-env
COPY --chmod=0755 .github/ci/docker/iccdev-generate-profiles.sh /usr/local/bin/iccdev-generate-profiles
COPY --chmod=0755 iccdev-mcp/docker/docker-entrypoint.sh /usr/local/bin/iccdev-mcp-entrypoint

RUN find /workspace/iccDEV/.github/scripts -maxdepth 1 -type f -name '*.sh' -exec chmod 0755 '{}' +

RUN python3 -m venv /opt/iccdev-mcp \
 && /opt/iccdev-mcp/bin/python -m pip install \
      --upgrade \
      'pip>=25.1.1' \
      'setuptools>=78.1.1' \
      'msgpack>=1.2.1' \
      --quiet \
 && /opt/iccdev-mcp/bin/python -m pip install \
      --no-cache-dir \
      "/workspace/iccDEV/iccdev-mcp[rest]" \
      --quiet \
 && /opt/iccdev-mcp/bin/python -m pip check \
 && /opt/iccdev-mcp/bin/python -c 'import iccdev_mcp; print(f"iccdev-mcp {iccdev_mcp.__version__}")' \
 && rm -rf \
      /opt/iccdev-mcp/bin/pip* \
      /opt/iccdev-mcp/lib/python*/site-packages/pip \
      /opt/iccdev-mcp/lib/python*/site-packages/pip-*.dist-info

USER iccdev-ci
WORKDIR /workspace/iccDEV

RUN rm -rf .git \
 && git init --quiet \
 && git config --global --add safe.directory /workspace/iccDEV \
 && (git remote set-url origin https://github.com/InternationalColorConsortium/iccDEV.git 2>/dev/null \
      || git remote add origin https://github.com/InternationalColorConsortium/iccDEV.git) \
 && if [ "$GIT_COMMIT" != "unknown" ]; then \
      git fetch --depth=1 origin "$GIT_COMMIT" \
      && git checkout --quiet --force --detach "$GIT_COMMIT"; \
    else \
      git config user.name "iccDEV CI" \
      && git config user.email "iccdev-ci@invalid" \
      && git add --all \
      && git commit --quiet -m "Initialize regression container source"; \
    fi \
 && test -z "$(git status --porcelain --untracked-files=all)" \
 && mkdir -p /workspace/build \
 && SAN_FLAGS="-fsanitize=address,undefined,float-divide-by-zero,float-cast-overflow" \
 && GIT_COMMIT="$GIT_COMMIT" CC=clang CXX=clang++ cmake -S Build/Cmake -B /workspace/build \
     -DCMAKE_BUILD_TYPE=Debug \
     -DCMAKE_C_COMPILER=clang \
     -DCMAKE_CXX_COMPILER=clang++ \
     -DCMAKE_C_FLAGS="-Wall -Wextra -Wpedantic -Werror -fno-omit-frame-pointer -g -O0" \
     -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -fno-omit-frame-pointer -g -O0 -std=c++17" \
     -DCMAKE_EXE_LINKER_FLAGS="$SAN_FLAGS" \
     -DCMAKE_SHARED_LINKER_FLAGS="$SAN_FLAGS" \
     -DENABLE_SANITIZERS=ON \
     -DSANITIZER_RECOVER=ON \
     -DENABLE_TOOLS=ON \
     -DENABLE_TESTS=ON \
     -DENABLE_WXWIDGETS=OFF \
     -DENABLE_SHARED_LIBS=ON \
     -DENABLE_STATIC_LIBS=ON \
     -Wno-dev \
 && cmake --build /workspace/build --parallel "$(nproc)"

RUN cmake --build /workspace/build --target build-test-binaries --parallel "$(nproc)"

# Regression helpers are EXCLUDE_FROM_ALL, so this has to follow
# build-test-binaries: before it the executable does not exist and ctest
# fails the image build with "Unable to find executable" (#2276).
RUN ctest --test-dir /workspace/build -R '^iccdev\.c-validation-dlopen$' \
    --output-on-failure --no-tests=error

WORKDIR /workspace/iccDEV/Testing
RUN iccdev-generate-profiles /workspace/build/Tools
RUN git checkout -- silence.txt \
 && test -z "$(git status --porcelain --untracked-files=all)"

USER root
RUN chmod 0755 /usr/local/bin/iccdev-banner \
 && chmod 0755 /usr/local/bin/iccdev-fuzz-env \
 && chmod 0755 /usr/local/bin/iccdev-generate-profiles \
 && chmod 0755 /usr/local/bin/iccdev-mcp-entrypoint \
 && find /workspace/build/Tools -mindepth 2 -maxdepth 2 -type f -executable \
     -exec ln -sf '{}' /usr/local/bin/ ';' \
 && printf '%s\n' \
     "if [ -z \"\${ICCDEV_BANNER_SHOWN:-}\" ]; then" \
     "  export ICCDEV_BANNER_SHOWN=1" \
     "  case \"\$-\" in *i*) /usr/local/bin/iccdev-banner ;; esac" \
     "fi" > /etc/profile.d/iccdev-banner.sh \
 && printf '%s\n' \
     "if [ -z \"\${ICCDEV_BANNER_SHOWN:-}\" ]; then" \
     "  export ICCDEV_BANNER_SHOWN=1" \
     "  case \"\$-\" in *i*) /usr/local/bin/iccdev-banner ;; esac" \
     "fi" >> /workspace/.bashrc \
 && chown iccdev-ci:iccdev-ci /workspace/.bashrc

HEALTHCHECK --interval=5m --timeout=10s --start-period=30s --retries=3 \
  CMD clang --version >/dev/null && cmake --version >/dev/null && command -v iccDumpProfile >/dev/null && command -v iccdev-mcp-rest >/dev/null && command -v iccdev-fuzz-env >/dev/null || exit 1

LABEL org.opencontainers.image.revision="${GIT_COMMIT}"
ENV ICCDEV_SOURCE_REVISION="${GIT_COMMIT}"

USER iccdev-ci
WORKDIR /workspace/iccDEV

EXPOSE 8080

CMD ["bash"]
