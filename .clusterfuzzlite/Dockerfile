# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# This source file is licensed under the BSD 3-Clause "New" or "Revised"
# License used by ICC software projects.

FROM gcr.io/oss-fuzz-base/base-builder@sha256:d0d9fb8232fd746e487e6e8e6f8631060b4a391a64b19ad3d87bdf454d00da50

COPY . $SRC/iccDEV
WORKDIR $SRC/iccDEV
COPY .clusterfuzzlite/build.sh $SRC/build.sh
