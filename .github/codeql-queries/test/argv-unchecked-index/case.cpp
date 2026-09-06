/*
 * Copyright (c) 2026 International Color Consortium.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the conditions in the
 * ICC Software License are met.
 */

/*
 * The cases live in Tools/CmdLine/case.h, not here, because
 * argv-unchecked-index.ql restricts itself to files whose relative path
 * begins "Tools/" -- a case compiled at this directory's root is outside the
 * query's scope and the test passes while asserting nothing.  The qltest cpp
 * extractor does not descend into subdirectories on its own, so the header is
 * pulled in from a root-level .cpp it will compile; the functions' getFile()
 * is then the header, whose relative path is Tools/CmdLine/case.h.
 */
#include "Tools/CmdLine/case.h"
