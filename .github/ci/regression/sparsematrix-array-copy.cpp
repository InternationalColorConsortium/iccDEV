/*
 * Copyright (c) International Color Consortium.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 *     1. Redistributions of source code must retain the above copyright notice,
 *        this list of conditions and the following disclaimer.
 *
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *
 *     3. Neither the name of International Color Consortium (ICC) nor the names of
 *        its contributors may be used to endorse or promote products derived from
 *        this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "IccIO.h"
#include "IccSparseMatrix.h"
#include "IccTagBasic.h"

#include <cmath>
#include <cstdio>
#include <cstring>

static bool writeTag(CIccTagSparseMatrixArray &tag, CIccMemIO &io)
{
  return io.Alloc(128, true) && tag.Write(&io);
}

static bool sameBytes(CIccMemIO &a, CIccMemIO &b)
{
  return a.GetLength() == b.GetLength() &&
         !std::memcmp(a.GetData(), b.GetData(), a.GetLength());
}

int main()
{
  CIccTagSparseMatrixArray source(1, 16);
  source.SetMatrixType(icSparseMatrixFloat16);

  icFloatNumber value = 0.75f;
  CIccSparseMatrix matrix;
  if (!source.GetSparseMatrix(matrix, 0, false) ||
      !matrix.Init(1, 1, true) ||
      !matrix.FillFromFullMatrix(&value)) {
    std::fprintf(stderr, "FAIL: create source matrix\n");
    return 2;
  }

  CIccTagSparseMatrixArray constructed(source);
  CIccTagSparseMatrixArray assigned(1, 16);
  assigned.SetMatrixType(icSparseMatrixUInt8);
  assigned = source;

  if (constructed.GetMatrixType() != icSparseMatrixFloat16 ||
      assigned.GetMatrixType() != icSparseMatrixFloat16) {
    std::fprintf(stderr, "FAIL: copy must retain the source matrix encoding\n");
    return 1;
  }

  CIccMemIO sourceWire, constructedWire, assignedWire;
  if (!writeTag(source, sourceWire) ||
      !writeTag(constructed, constructedWire) ||
      !writeTag(assigned, assignedWire)) {
    std::fprintf(stderr, "FAIL: serialize populated matrix\n");
    return 2;
  }
  if (!sameBytes(sourceWire, constructedWire) ||
      !sameBytes(sourceWire, assignedWire)) {
    std::fprintf(stderr, "FAIL: copy must serialize identically to source\n");
    return 1;
  }

  CIccMemIO reader;
  CIccTagSparseMatrixArray restored;
  CIccSparseMatrix restoredMatrix;
  if (!reader.Attach(assignedWire.GetData(), assignedWire.GetLength()) ||
      !restored.Read(assignedWire.GetLength(), &reader) ||
      restored.GetMatrixType() != icSparseMatrixFloat16 ||
      !restored.GetSparseMatrix(restoredMatrix, 0) ||
      restoredMatrix.GetNumEntries() != 1 ||
      std::fabs(restoredMatrix.GetData()->get(0) - value) > 0.0001f) {
    std::fprintf(stderr, "FAIL: assigned tag must round-trip with source encoding\n");
    return 1;
  }

  CIccTagSparseMatrixArray *self = &assigned;
  assigned = *self;
  if (assigned.GetMatrixType() != icSparseMatrixFloat16) {
    std::fprintf(stderr, "FAIL: self-assignment must retain encoding\n");
    return 1;
  }

  std::puts("sparse matrix array copy preserves encoding and serialized values");
  return 0;
}
