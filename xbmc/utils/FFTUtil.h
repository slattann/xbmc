/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "utils/log.h"

#include <array>
#include <chrono>
#include <cmath>
#include <complex>
#include <vector>

template<size_t channels, size_t size>
class CFFT
{
public:
  CFFT();
  ~CFFT() = default;

  void Calc(std::vector<float>& input, std::array<float, size>& output);
  void CalcFFT(std::array<std::array<std::complex<float>, channels>, size>& input);

private:
  std::array<std::array<std::complex<float>, channels>, size / 2> m_even;
  std::array<std::array<std::complex<float>, channels>, size / 2> m_odd;
};

template<size_t channels, size_t size>
CFFT<channels, size>::CFFT()
{
  if (size % 2)
    throw std::logic_error("size count must be even");

  if (channels % 2)
    throw std::logic_error("channel count must be even");
}

template<size_t channels, size_t size>
void CFFT<channels, size>::CalcFFT(std::array<std::array<std::complex<float>, channels>, size>& input)
{
  for (size_t i = 0; i < size; i += 2)
  {
    std::array<std::complex<float>, channels> tmpEven;
    std::array<std::complex<float>, channels> tmpOdd;

    for (size_t channel = 0; channel < channels; channel++)
    {
      tmpEven[channel] = input[i][channel];
      tmpOdd[channel] = input[i + 1][channel];
    }

    m_even[i / 2] = tmpEven;
    m_odd[i / 2] = tmpOdd;
  }

  if (!((size / 2) % 2))
  {
    CFFT<channels, size / 2> fft;
    fft.CalcFFT(m_even);
    fft.CalcFFT(m_odd);
  }

  for (size_t channel = 0; channel < channels; channel++)
  {
    for (size_t i = 0; i < size / 2; i++)
    {
      float angle = -2.0 * M_PI * i / size;

      std::complex<float> w(cos(angle), sin(angle));

      w *= m_odd[i][channel];

      input[i][channel] = m_even[i][channel] + w;
      input[(size / 2) + i][channel] = m_even[i][channel] - w;
    }
  }
}

template<size_t channels, size_t size>
void CFFT<channels, size>::Calc(std::vector<float>& input, std::array<float, size>& output)
{
  auto start = std::chrono::high_resolution_clock::now();

  std::array<std::array<std::complex<float>, channels>, size> data;
  for (size_t i = 0; i < std::min(size, input.size()); ++i)
  {
    std::array<std::complex<float>, channels> freq;
    // left channel
    freq[0] = input[2 * i];
    // right channel
    freq[1] = input[2 * i + 1];
    data[i] = freq;
  }

  CalcFFT(data);

  auto&& filter = [&](std::complex<float> data)
  {
    return std::abs(data) * 2.0 / size;
  };

  for (size_t i = 0; i < size / 2; ++i)
  {
    output[2 * i] = filter(data[i][0]);
    output[2 * i + 1] = filter(data[i][1]);
  }

  auto elapsed = std::chrono::high_resolution_clock::now() - start;

  auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();

  CLog::Log(LOGDEBUG, "CFFT::{} execution time: {} ns", __FUNCTION__, nanoseconds);
}
