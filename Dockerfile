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
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
    build-essential cmake make \
    libxml2-dev nlohmann-json3-dev \
    libtiff-dev libjpeg-dev libpng-dev zlib1g-dev \
 && rm -rf /var/lib/apt/lists/*

COPY . /opt/iccdev
RUN cd /opt/iccdev \
 && sed -i '/find_package(wxWidgets COMPONENTS core base REQUIRED)/,/endif()/ s/^/# /' Build/Cmake/CMakeLists.txt \
 && cd Build \
 && rm -f CMakeCache.txt \
 && rm -rf CMakeFiles \
 && rm -f Cmake/CMakeCache.txt \
 && rm -rf Cmake/CMakeFiles \
 && cmake -DCMAKE_BUILD_TYPE=Release Cmake \
 && make -j"$(nproc)" \
 && rm -rf /opt/iccdev/.git

RUN echo "=== Libraries ===" \
 && ls -lh /opt/iccdev/Build/IccProfLib/libIccProfLib2* \
 && ls -lh /opt/iccdev/Build/IccXML/libIccXML2* \
 && (ls -lh /opt/iccdev/Build/IccJSON/libIccJSON2* 2>/dev/null || echo "IccJSON: not built (nlohmann-json may be missing)") \
 && echo "=== Tools ===" \
 && find /opt/iccdev/Build/Tools -type f -executable | sort

FROM ubuntu:26.04@sha256:fed6ddb82c61194e1814e93b59cfcb6759e5aa33c4e41bb3782313c2386ed6df
ENV DEBIAN_FRONTEND=noninteractive

LABEL org.opencontainers.image.title="iccDEV Build Container" \
      org.opencontainers.image.description="Container v2.0.0.82" \
      org.opencontainers.image.licenses="BSD-3-Clause" \
      org.opencontainers.image.vendor="International Color Consortium" \
      org.opencontainers.image.source="https://github.com/InternationalColorConsortium/iccDEV"

RUN apt-get update && apt-get install -y --no-install-recommends \
    libc6 libxml2-16 libtiff6 libjpeg8 libpng16-16t64 zlib1g \
 && rm -rf /var/lib/apt/lists/*

COPY --from=builder /opt/iccdev /opt/iccdev

RUN groupadd -r iccdev \
 && useradd -r -g iccdev -d /opt/iccdev -s /bin/bash iccdev \
 && chown -R iccdev:iccdev /opt/iccdev

ENV PATH="/opt/iccdev/Build/Tools/IccToXml:/opt/iccdev/Build/Tools/IccFromXml:/opt/iccdev/Build/Tools/IccDumpProfile:/opt/iccdev/Build/Tools/IccApplyNamedCmm:/opt/iccdev/Build/Tools/IccRoundTrip:/opt/iccdev/Build/Tools/IccFromCube:/opt/iccdev/Build/Tools/IccApplyProfiles:/opt/iccdev/Build/Tools/IccApplySearch:/opt/iccdev/Build/Tools/IccApplyToLink:/opt/iccdev/Build/Tools/IccJpegDump:/opt/iccdev/Build/Tools/IccPngDump:/opt/iccdev/Build/Tools/IccSpecSepToTiff:/opt/iccdev/Build/Tools/IccTiffDump:/opt/iccdev/Build/Tools/IccV5DspObsToV4Dsp:/opt/iccdev/Build/Tools/IccToJson:/opt/iccdev/Build/Tools/IccFromJson:${PATH}"
ENV LD_LIBRARY_PATH="/opt/iccdev/Build/IccProfLib:/opt/iccdev/Build/IccXML:/opt/iccdev/Build/IccJSON"

USER iccdev
WORKDIR /opt/iccdev

CMD ["bash"]