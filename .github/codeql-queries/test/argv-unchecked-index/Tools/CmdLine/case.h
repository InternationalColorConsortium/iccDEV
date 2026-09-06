/*
 * Copyright (c) 2026 International Color Consortium.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the conditions in the
 * ICC Software License are met.
 */

int strcmp(const char *a, const char *b);
int puts(const char *s);

// --- A. same-line short-circuit guard: the `-h`/`--help` idiom every
//        Tools/CmdLine main() uses.  argv[1] is read only when argc == 2.
//        MUST NOT be reported.
int helpGuardSameLine(int argc, char **argv)
{
  if (argc == 2 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")))
    return 0;
  return 1;
}

// --- B. array-spelled parameter, genuinely unguarded.  MUST be reported.
int arraySpellingUnguarded(int argc, char *argv[])
{
  return puts(argv[1]);
}

// --- C. array-spelled, const-qualified, genuinely unguarded.  MUST be reported.
int constArraySpellingUnguarded(int argc, const char *argv[])
{
  return puts(argv[2]);
}

// --- D. pointer-spelled, genuinely unguarded.  MUST be reported (existing TP).
int pointerUnguarded(int argc, char **argv)
{
  return puts(argv[1]);
}

// --- E. pointer-spelled, guarded on an earlier line.  MUST NOT be reported.
int pointerGuardedEarlier(int argc, char **argv)
{
  if (argc < 2)
    return 1;
  return puts(argv[1]);
}

// --- F. array-spelled, guarded on an earlier line.  MUST NOT be reported.
int arrayGuardedEarlier(int argc, char *argv[])
{
  if (argc < 3)
    return 1;
  return puts(argv[2]);
}

// --- G. argv[0] is always valid.  MUST NOT be reported.
int programName(int argc, char *argv[])
{
  return puts(argv[0]);
}

// --- H. the combination that actually occurs: array-spelled parameter *and*
//        the same-line help guard.  Four of the five iccApply* tools are
//        written this way, so widening isArgv() without also relaxing the
//        line test would have turned one false positive into nine.
//        MUST NOT be reported.
int arrayHelpGuardSameLine(int argc, char *argv[])
{
  if (argc == 2 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")))
    return 0;
  return 1;
}

// --- I. the same-line case that is NOT a guard: argv[1] is read before argc
//        is ever compared, so at argc == 1 this reads out of bounds.  Without
//        this case the suite cannot tell a correct same-line relaxation from
//        one that simply stops looking at the line -- every other MUST-NOT
//        case here is one where the comparison really does dominate.
//        MUST be reported.
int readBeforeGuardSameLine(int argc, char *argv[])
{
  if (!strcmp(argv[1], "-x") && argc > 2)
    return 0;
  return 1;
}
