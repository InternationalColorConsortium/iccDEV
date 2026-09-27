# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# This source file is licensed under the BSD 3-Clause "New" or "Revised"
# License used by ICC software projects.

FROM gcr.io/oss-fuzz-base/base-builder@sha256:d0d9fb8232fd746e487e6e8e6f8631060b4a391a64b19ad3d87bdf454d00da50

RUN apt-get update -qq && \
    apt-get install -y --no-install-recommends \
      libxml2-dev=2.9.10+dfsg-5ubuntu0.20.04.10 \
      nlohmann-json3-dev=3.7.3-1 && \
    rm -rf /var/lib/apt/lists/*

COPY . $SRC/iccDEV
WORKDIR $SRC/iccDEV
COPY .clusterfuzzlite/build.sh $SRC/build.sh
