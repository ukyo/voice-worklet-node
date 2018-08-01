//-----------------------------------------------------------------------------
// Copyright 2012 Masanori Morise
// Author: mmorise [at] yamanashi.ac.jp (Masanori Morise)
// Last update: 2017/02/01
//
// test.exe input.wav outout.wav f0 spec
// input.wav  : Input file
//
// output.wav : Output file
// f0         : F0 scaling (a positive number)
// spec       : Formant scaling (a positive number)
//
// Note: This version output three speech synthesized by different algorithms.
//       When the filename is "output.wav", "01output.wav", "02output.wav" and
//       "03output.wav" are generated. They are almost all the same.
//-----------------------------------------------------------------------------

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>
#include <emscripten/emscripten.h>

// WORLD core functions.
#include "world/d4c.h"
#include "world/dio.h"
#include "world/harvest.h"
#include "world/matlabfunctions.h"
#include "world/cheaptrick.h"
#include "world/stonemask.h"
#include "world/synthesis.h"
#include "world/synthesisrealtime.h"

//-----------------------------------------------------------------------------
// struct for WORLD
// This struct is an option.
// Users are NOT forced to use this struct.
//-----------------------------------------------------------------------------
typedef struct
{
  double frame_period;
  int fs;

  double *f0;
  double *time_axis;
  int f0_length;

  double **spectrogram;
  double **aperiodicity;
  int fft_size;
} WorldParameters;

static void F0EstimationDio(double *x, int x_length,
                            WorldParameters *world_parameters)
{
  DioOption option = {0};
  InitializeDioOption(&option);

  // Modification of the option
  option.frame_period = world_parameters->frame_period;

  // Valuable option.speed represents the ratio for downsampling.
  // The signal is downsampled to fs / speed Hz.
  // If you want to obtain the accurate result, speed should be set to 1.
  option.speed = 1;

  // You can set the f0_floor below world::kFloorF0.
  option.f0_floor = 40.0;

  // You can give a positive real number as the threshold.
  // Most strict value is 0, but almost all results are counted as unvoiced.
  // The value from 0.02 to 0.2 would be reasonable.
  option.allowed_range = 0.1;

  // Parameters setting and memory allocation.
  world_parameters->f0_length = GetSamplesForDIO(world_parameters->fs,
                                                 x_length, world_parameters->frame_period);
  world_parameters->f0 =
      (double *)malloc(sizeof(double) * (world_parameters->f0_length));
  world_parameters->time_axis =
      (double *)malloc(sizeof(double) * (world_parameters->f0_length));
  double *refined_f0 =
      (double *)malloc(sizeof(double) * (world_parameters->f0_length));

  Dio(x, x_length, world_parameters->fs, &option, world_parameters->time_axis,
      world_parameters->f0);

  // StoneMask is carried out to improve the estimation performance.
  StoneMask(x, x_length, world_parameters->fs, world_parameters->time_axis,
            world_parameters->f0, world_parameters->f0_length, refined_f0);

  for (int i = 0; i < world_parameters->f0_length; ++i)
    world_parameters->f0[i] = refined_f0[i];

  if (refined_f0 != NULL)
  {
    free(refined_f0);
    refined_f0 = NULL;
  }
}

static void SpectralEnvelopeEstimation(double *x, int x_length,
                                       WorldParameters *world_parameters)
{
  CheapTrickOption option = {0};
  // Note (2017/01/02): fs is added as an argument.
  InitializeCheapTrickOption(world_parameters->fs, &option);

  // Default value was modified to -0.15.
  // option.q1 = -0.15;

  // Important notice (2017/01/02)
  // You can set the fft_size.
  // Default is GetFFTSizeForCheapTrick(world_parameters->fs, &option);
  // When fft_size changes from default value,
  // a replaced f0_floor will be used in CheapTrick().
  // The lowest F0 that WORLD can work as expected is determined
  // by the following : 3.0 * fs / fft_size.
  option.f0_floor = 71.0;
  option.fft_size = GetFFTSizeForCheapTrick(world_parameters->fs, &option);
  // We can directly set fft_size.
  //   option.fft_size = 1024;

  // Parameters setting and memory allocation.
  world_parameters->fft_size =
      GetFFTSizeForCheapTrick(world_parameters->fs, &option);
  world_parameters->spectrogram = (double **)malloc(sizeof(double *) * (world_parameters->f0_length));
  for (int i = 0; i < world_parameters->f0_length; ++i)
    world_parameters->spectrogram[i] =
        (double *)malloc(sizeof(double) * (world_parameters->fft_size / 2 + 1));

  CheapTrick(x, x_length, world_parameters->fs, world_parameters->time_axis,
             world_parameters->f0, world_parameters->f0_length, &option,
             world_parameters->spectrogram);
}

static void AperiodicityEstimation(double *x, int x_length,
                                   WorldParameters *world_parameters)
{
  D4COption option = {0};
  InitializeD4COption(&option);

  // This parameter is used to determine the aperiodicity at 0 Hz.
  // If you want to use the conventional D4C, please set the threshold to 0.0.
  // Unvoiced section is counted by using this parameter.
  // Aperiodicity indicates high value when the frame is the unvoiced section.
  option.threshold = 0.85;

  // Parameters setting and memory allocation.
  world_parameters->aperiodicity = (double **)malloc(sizeof(double *) * (world_parameters->f0_length));
  for (int i = 0; i < world_parameters->f0_length; ++i)
    world_parameters->aperiodicity[i] =
        (double *)malloc(sizeof(double) * (world_parameters->fft_size / 2 + 1));

  D4C(x, x_length, world_parameters->fs, world_parameters->time_axis,
      world_parameters->f0, world_parameters->f0_length,
      world_parameters->fft_size, &option, world_parameters->aperiodicity);
}

static void ParameterModification(double shift, double ratio, int fs,
                                  int f0_length, int fft_size, double *f0, double **spectrogram)
{
  for (int i = 0; i < f0_length; ++i)
    f0[i] *= shift;

  // Spectral stretching
  double *freq_axis1 = (double *)malloc(sizeof(double) * (fft_size));
  double *freq_axis2 = (double *)malloc(sizeof(double) * (fft_size));
  double *spectrum1 = (double *)malloc(sizeof(double) * (fft_size));
  double *spectrum2 = (double *)malloc(sizeof(double) * (fft_size));

  for (int i = 0; i <= fft_size / 2; ++i)
  {
    freq_axis1[i] = ratio * i / fft_size * fs;
    freq_axis2[i] = (double)(i) / fft_size * fs;
  }
  for (int i = 0; i < f0_length; ++i)
  {
    for (int j = 0; j <= fft_size / 2; ++j)
      spectrum1[j] = log(spectrogram[i][j]);
    interp1(freq_axis1, spectrum1, fft_size / 2 + 1, freq_axis2,
            fft_size / 2 + 1, spectrum2);
    for (int j = 0; j <= fft_size / 2; ++j)
      spectrogram[i][j] = exp(spectrum2[j]);
    if (ratio >= 1.0)
      continue;
    for (int j = (int)(fft_size / 2.0 * ratio);
         j <= fft_size / 2; ++j)
      spectrogram[i][j] =
          spectrogram[i][(int)(fft_size / 2.0 * ratio) - 1];
  }
  if (spectrum1 != NULL)
  {
    free(spectrum1);
    spectrum1 = NULL;
  }
  if (spectrum2 != NULL)
  {
    free(spectrum2);
    spectrum2 = NULL;
  }
  if (freq_axis1 != NULL)
  {
    free(freq_axis1);
    freq_axis1 = NULL;
  }
  if (freq_axis2 != NULL)
  {
    free(freq_axis2);
    freq_axis2 = NULL;
  }
}

static void WaveformSynthesis(WorldParameters *world_parameters, int fs,
                              int y_length, double *y)
{
  // Synthesis by the aperiodicity
  Synthesis(world_parameters->f0, world_parameters->f0_length,
            world_parameters->spectrogram, world_parameters->aperiodicity,
            world_parameters->fft_size, world_parameters->frame_period, fs,
            y_length, y);
}

static void DestroyMemory(WorldParameters *world_parameters)
{
  if (world_parameters->time_axis != NULL)
  {
    free(world_parameters->time_axis);
    world_parameters->time_axis = NULL;
  }
  if (world_parameters->f0 != NULL)
  {
    free(world_parameters->f0);
    world_parameters->f0 = NULL;
  }
  for (int i = 0; i < world_parameters->f0_length; ++i)
  {
    if (world_parameters->spectrogram[i] != NULL)
    {
      free(world_parameters->spectrogram[i]);
      world_parameters->spectrogram[i] = NULL;
    }
    if (world_parameters->aperiodicity[i] != NULL)
    {
      free(world_parameters->aperiodicity[i]);
      world_parameters->aperiodicity[i] = NULL;
    }
  }
  if (world_parameters->spectrogram != NULL)
  {
    free(world_parameters->spectrogram);
    world_parameters->spectrogram = NULL;
  }
  if (world_parameters->aperiodicity != NULL)
  {
    free(world_parameters->aperiodicity);
    world_parameters->aperiodicity = NULL;
  }
}

//-----------------------------------------------------------------------------
// Test program.
// test.exe input.wav outout.wav f0 spec flag
// input.wav  : argv[1] Input file
// output.wav : argv[2] Output file
// f0         : argv[3] F0 scaling (a positive number)
// spec       : argv[4] Formant shift (a positive number)
//-----------------------------------------------------------------------------
void EMSCRIPTEN_KEEPALIVE transform(double *x, int x_length, int fs, int sample_length, double shift, double ratio, double *y)
{
  //---------------------------------------------------------------------------
  // Analysis part
  //---------------------------------------------------------------------------
  // A new struct is introduced to implement safe program.
  WorldParameters world_parameters = {0};
  // You must set fs and frame_period before analysis/synthesis.
  world_parameters.fs = fs;
  // 5.0 ms is the default value.
  world_parameters.frame_period = (double)sample_length / (double)fs * 1000.0;

  // F0 estimation
  // DIO
  F0EstimationDio(x, x_length, &world_parameters);

  // Spectral envelope estimation
  SpectralEnvelopeEstimation(x, x_length, &world_parameters);

  // Aperiodicity estimation by D4C
  AperiodicityEstimation(x, x_length, &world_parameters);

  // Note that F0 must not be changed until all parameters are estimated.
  ParameterModification(shift, ratio, fs, world_parameters.f0_length,
                        world_parameters.fft_size, world_parameters.f0,
                        world_parameters.spectrogram);

  //---------------------------------------------------------------------------
  // Synthesis part
  // There are three samples in speech synthesis
  // 1: Conventional synthesis
  // 2: Example of real-time synthesis
  // 3: Example of real-time synthesis (Ring buffer is efficiently used)
  //---------------------------------------------------------------------------

  // Synthesis 1 (conventional synthesis)
  for (int i = 0; i < x_length; ++i)
    y[i] = 0.0;
  WaveformSynthesis(&world_parameters, fs, x_length, y);

  if (y != NULL)
  {
    free(y);
    y = NULL;
  }
  if (x != NULL)
  {
    free(x);
    x = NULL;
  }
  DestroyMemory(&world_parameters);
}
