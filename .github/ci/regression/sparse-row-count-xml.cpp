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
// CTest contract: the XML reader refuses a SparseMatrix with more SparseRow
// elements than its rows attribute declares (#2582).
//
// CIccTagXmlSparseMatrixArray::ParseXml() sized the matrix from rows/cols with
// CIccSparseMatrix::Init() -- a row-start table of rows+1 entries and column
// storage for rows rows -- then wrote rowstart[iRow] and row iRow's columns for
// every SparseRow child with no bound on iRow. One extra SparseRow wrote past
// both. The reader now refuses the document.
//
// A row count equal to rows, and one below it (the missing rows are empty),
// are controls that must still parse. On an unfixed library the excess case
// parses and returns true (and writes out of bounds, which ASan reports), so
// the refusal assertion fails in any build.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccTagXml.h"
#include "IccUtilXml.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

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

std::string row()
{
  return "<SparseRow><ColIndices>0</ColIndices><ColData>1</ColData></SparseRow>";
}

// A one-matrix array declaring 'declared' rows and carrying 'present' SparseRows.
std::string doc(int declared, int present)
{
  std::string s = "<SparseMatrixArray outputChannels=\"12\" matrixType=\"1\">"
                  "<SparseMatrix rows=\"" + std::to_string(declared) + "\" cols=\"3\">";
  for (int i = 0; i < present; i++)
    s += row();
  s += "</SparseMatrix></SparseMatrixArray>";
  return s;
}

bool parse(const std::string &xml, std::string &err)
{
  xmlDocPtr pDoc = xmlReadMemory(xml.data(), (int)xml.size(), "sparse.xml", NULL, 0);
  if (!pDoc) {
    err = "libxml2 could not read the fixture";
    return false;
  }
  CIccTagXmlSparseMatrixArray tag;
  bool ok = tag.ParseXml(xmlDocGetRootElement(pDoc), err);
  xmlFreeDoc(pDoc);
  return ok;
}

} // namespace

int main()
{
  std::string err;

  err.clear();
  check(parse(doc(2, 2), err), "two rows declared, two SparseRow: parses");

  err.clear();
  check(parse(doc(2, 1), err), "two rows declared, one SparseRow: parses (row 2 empty)");

  err.clear();
  bool ok = parse(doc(2, 3), err);
  check(!ok, "two rows declared, three SparseRow: refused");
  check(err.find("More SparseRow elements than SparseMatrix rows") != std::string::npos,
        "two rows declared, three SparseRow: says why");

  xmlCleanupParser();
  if (g_failures)
    std::printf("%d assertion(s) failed\n", g_failures);
  else
    std::printf("all assertions passed\n");
  return g_failures;
}
