###############################################################
# Copyright (c) 2025-2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# Intent: iccDEV ci-docker-build
#
# Last Updated: 2026-02-12 00:14:22 UTC by David Hoyt
#
###############################################################

FROM ubuntu:26.04@sha256:fed6ddb82c61194e1814e93b59cfcb6759e5aa33c4e41bb3782313c2386ed6df AS builder
SHELL ["/bin/bash", "-o", "pipefail", "-c"]
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
    build-essential=12.12ubuntu2 \
    cmake=4.2.3-2ubuntu2 \
    lsb-release=12.1-2build1 \
    make=4.4.1-3 \
    libxml2-dev=2.15.2+dfsg-0.1 \
    nlohmann-json3-dev=3.12.0.really.3.12.0.really.3.11.3-3build1 \
    libtiff-dev=4.7.0-3ubuntu4 \
    libjpeg-dev=8c-2ubuntu12 \
    libpng-dev=1.6.57-1 \
    zlib1g-dev=1:1.3.dfsg+really1.3.1-1ubuntu3 \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /opt/iccdev
COPY . .

RUN sed -i '/find_package(wxWidgets COMPONENTS core base REQUIRED)/,/endif()/ s/^/# /' Build/Cmake/CMakeLists.txt \
 && rm -f Build/CMakeCache.txt \
 && rm -rf Build/CMakeFiles \
 && rm -f Build/Cmake/CMakeCache.txt \
 && rm -rf Build/Cmake/CMakeFiles \
 && cmake -S Build/Cmake -B Build -DCMAKE_BUILD_TYPE=Release \
 && cmake --build Build --parallel "$(nproc)" \
 && rm -rf .git

RUN echo "=== Libraries ===" \
 && ls -lh /opt/iccdev/Build/IccProfLib/libIccProfLib2* \
 && ls -lh /opt/iccdev/Build/IccXML/libIccXML2* \
 && (ls -lh /opt/iccdev/Build/IccJSON/libIccJSON2* 2>/dev/null || echo "IccJSON: not built (nlohmann-json may be missing)") \
 && echo "=== Tools ===" \
 && find /opt/iccdev/Build/Tools -type f -executable | sort

FROM ubuntu:26.04@sha256:fed6ddb82c61194e1814e93b59cfcb6759e5aa33c4e41bb3782313c2386ed6df
SHELL ["/bin/bash", "-o", "pipefail", "-c"]
ENV DEBIAN_FRONTEND=noninteractive

LABEL org.opencontainers.image.title="iccDEV Build Container" \
      org.opencontainers.image.description="Container v2.0.0.82" \
      org.opencontainers.image.licenses="BSD-3-Clause" \
      org.opencontainers.image.vendor="International Color Consortium" \
      org.opencontainers.image.source="https://github.com/InternationalColorConsortium/iccDEV"

RUN apt-get update && apt-get install -y --no-install-recommends \
    libc6=2.43-2ubuntu2 \
    libxml2-16=2.15.2+dfsg-0.1 \
    libtiff6=4.7.0-3ubuntu4 \
    libjpeg8=8c-2ubuntu12 \
    libpng16-16t64=1.6.57-1 \
    zlib1g=1:1.3.dfsg+really1.3.1-1ubuntu3 \
    python3=3.14.3-0ubuntu2 \
 && rm -rf /var/lib/apt/lists/*

COPY --from=builder /opt/iccdev/Build /opt/iccdev/Build
COPY --from=builder /opt/iccdev/Testing /opt/iccdev/Testing
COPY --from=builder /opt/iccdev/LICENSE.md /opt/iccdev/LICENSE.md
COPY --from=builder /opt/iccdev/README.md /opt/iccdev/README.md

RUN groupadd -r iccdev \
 && useradd -r -g iccdev -d /opt/iccdev -s /bin/bash iccdev \
 && chown -R iccdev:iccdev /opt/iccdev

ENV PATH="/opt/iccdev/Build/Tools/IccToXml:/opt/iccdev/Build/Tools/IccFromXml:/opt/iccdev/Build/Tools/IccDumpProfile:/opt/iccdev/Build/Tools/IccPawgReport:/opt/iccdev/Build/Tools/IccApplyNamedCmm:/opt/iccdev/Build/Tools/IccRoundTrip:/opt/iccdev/Build/Tools/IccFromCube:/opt/iccdev/Build/Tools/IccApplyProfiles:/opt/iccdev/Build/Tools/IccApplySearch:/opt/iccdev/Build/Tools/IccApplyToLink:/opt/iccdev/Build/Tools/IccJpegDump:/opt/iccdev/Build/Tools/IccPngDump:/opt/iccdev/Build/Tools/IccSpecSepToTiff:/opt/iccdev/Build/Tools/IccTiffDump:/opt/iccdev/Build/Tools/IccV5DspObsToV4Dsp:/opt/iccdev/Build/Tools/IccToJson:/opt/iccdev/Build/Tools/IccFromJson:${PATH}"
ENV LD_LIBRARY_PATH="/opt/iccdev/Build/IccProfLib:/opt/iccdev/Build/IccXML:/opt/iccdev/Build/IccJSON"

USER iccdev
WORKDIR /opt/iccdev

HEALTHCHECK --interval=5m --timeout=10s --start-period=30s --retries=3 \
  CMD test -x /opt/iccdev/Build/Tools/IccDumpProfile/iccDumpProfile || exit 1

CMD ["bash"]
