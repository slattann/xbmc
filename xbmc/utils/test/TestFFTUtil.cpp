/*
 *  Copyright (C) 2015-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "utils/FFTUtil.h"

#include "gtest/gtest.h"

#include <cmath>

TEST(TestFFTUtil, SimpleSignal)
{
  const int size = 32;
  const int freq1 = 5;
  const int freq2[] = {1, 7};

  std::vector<float> input(2 * size);
  for (size_t i = 0; i < size; ++i)
  {
    // left channel
    input[2 * i] = (cos(freq1 * 2.0 * M_PI * i / size));
    // right channel
    input[2 * i + 1] = (cos(freq2[0] * 2.0 * M_PI * i / size) + cos(freq2[1] * 2.0 * M_PI * i / size));
  }

  std::array<float, size> output;

  CFFT<2, size> fft;
  fft.Calc(input, output);

  for (int i = 0; i < size / 2; ++i)
  {
    EXPECT_NEAR(output[2 * i], (i == freq1 ? 1.0 : 0.0), 1e-7);
    EXPECT_NEAR(output[2 * i + 1], ((i == freq2[0] || i == freq2[1]) ? 1.0 : 0.0), 1e-7);
  }
}
