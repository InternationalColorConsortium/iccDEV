/*
 * Copyright (c) 2026 International Color Consortium.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names "ICC" and "International Color Consortium" must not be used to
 *    imply endorsement without prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND.
 */

#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "IccTagLut.h"

int main(int argc, char **argv)
{
  const int outputs = argc > 1 ? std::atoi(argv[1]) : 8;
  const int iterations = argc > 2 ? std::atoi(argv[2]) : 5000000;
  const int repetitions = argc > 3 ? std::atoi(argv[3]) : 7;
  const int affinityCpu = argc > 4 ? std::atoi(argv[4]) : 0;

  if (outputs < 8 || outputs > 16 || iterations < 1 || repetitions < 1 ||
      affinityCpu < 0 || affinityCpu > 63)
    return 2;

  if (!SetThreadAffinityMask(
        GetCurrentThread(), static_cast<DWORD_PTR>(1ull << affinityCpu)))
    return 4;
  SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

  CIccCLUT clut(3, static_cast<icUInt16Number>(outputs), 4);
  if (!clut.Init(2))
    return 3;

  icFloatNumber *data = clut.GetData(0);
  for (int i = 0; i < 8 * outputs; i++)
    data[i] = static_cast<icFloatNumber>((i % 64) * 0.01f);
  clut.Begin();

  const icFloatNumber src[3] = {0.25f, 0.50f, 0.75f};
  icFloatNumber dst[16] = {};
  for (int i = 0; i < 10000; i++)
    clut.Interp3d(dst, src);

  std::vector<double> samples;
  samples.reserve(static_cast<size_t>(repetitions));
  for (int sample = 0; sample < repetitions; sample++) {
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; i++)
      clut.Interp3d(dst, src);
    const auto end = std::chrono::steady_clock::now();
    samples.push_back(
        std::chrono::duration<double, std::nano>(end - start).count() /
        static_cast<double>(iterations));
  }

  std::sort(samples.begin(), samples.end());
  const double median = samples[samples.size() / 2];
  double checksum = 0.0;
  for (int i = 0; i < outputs; i++)
    checksum += dst[i];
  std::printf(
      "outputs=%d iterations=%d repetitions=%d affinity_cpu=%d median_ns=%.3f calls_per_s=%.3f checksum=%.6f outputs_hex=",
      outputs, iterations, repetitions, affinityCpu, median,
      1000000000.0 / median, checksum);
  for (int i = 0; i < outputs; i++) {
    std::uint32_t bits;
    std::memcpy(&bits, &dst[i], sizeof(bits));
    std::printf("%s%08x", i ? "," : "", static_cast<unsigned int>(bits));
  }
  std::printf("\n");
  return 0;
}
