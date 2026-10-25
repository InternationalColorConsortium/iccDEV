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
// CTest contract: the JSON reader refuses a sparse matrix with more sparseRows
// than its rows member declares (#2582).
//
// CIccTagJsonSparseMatrixArray::ParseJson() read only the first 'rows' entries
// of sparseRows and dropped the rest without a word, so a document whose rows
// disagree with its own row count parsed as a different matrix. The XML reader
// wrote the extra rows out of bounds and now refuses them; the JSON reader
// refuses them too.
//
// A row count equal to rows, and one below it (the missing rows are empty),
// are controls that must still parse.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccTagJson.h"

#include <cstdio>
#include <string>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_failures = 0;

void check(bool ok, const char *label)
{
  std::printf("%s %s\n", ok ? "ok:  " : "FAIL:", label);
  if (!ok)
    ++g_failures;
}

// A one-matrix array declaring 'declared' rows and carrying 'present' sparseRows.
IccJson doc(int declared, int present)
{
  IccJson rows = IccJson::array();
  for (int i = 0; i < present; i++)
    rows.push_back({ { "colIndices", { 0 } }, { "colData", { 1.0 } } });

  IccJson mtx = { { "rows", declared }, { "cols", 3 }, { "sparseRows", rows } };
  return { { "outputChannels", 12 }, { "matrixType", 1 },
           { "matrices", IccJson::array({ mtx }) } };
}

bool parse(const IccJson &j, std::string &err)
{
  CIccTagJsonSparseMatrixArray tag;
  return tag.ParseJson(j, err);
}

} // namespace

int main()
{
  std::string err;

  err.clear();
  check(parse(doc(2, 2), err), "two rows declared, two sparseRows: parses");

  err.clear();
  check(parse(doc(2, 1), err), "two rows declared, one sparseRows entry: parses (row 2 empty)");

  err.clear();
  bool ok = parse(doc(2, 3), err);
  check(!ok, "two rows declared, three sparseRows: refused");
  check(err.find("More SparseMatrix sparseRows than rows") != std::string::npos,
        "two rows declared, three sparseRows: says why");

  if (g_failures)
    std::printf("%d assertion(s) failed\n", g_failures);
  else
    std::printf("all assertions passed\n");
  return g_failures;
}
