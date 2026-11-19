# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# This source file is licensed under the BSD 3-Clause "New" or "Revised"
# License used by ICC software projects.

FROM gcr.io/oss-fuzz-base/base-builder@sha256:1f9cf74bc3d7029b528494542f9d9851b256dceed38d3f420989434dc5ae0760

RUN apt-get update -qq && \
    apt-get install -y --no-install-recommends \
      libxml2-dev=2.9.10+dfsg-5ubuntu0.20.04.10 \
      nlohmann-json3-dev=3.7.3-1 && \
    rm -rf /var/lib/apt/lists/*

COPY . $SRC/iccDEV
WORKDIR $SRC/iccDEV
COPY .clusterfuzzlite/build.sh $SRC/build.sh
