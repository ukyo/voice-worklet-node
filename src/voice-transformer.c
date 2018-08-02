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
  int x_length;

  double *f0;
  double *refined_f0;
  double *time_axis;
  int f0_length;

  double **spectrogram;
  double **aperiodicity;
  int fft_size;

  DioOption *dio_option;
  CheapTrickOption *cheap_trick_option;
  D4COption *d4c_option;

  double *freq_axis1;
  double *freq_axis2;
  double *spectrum1;
  double *spectrum2;
} WorldParameters;

WorldParameters EMSCRIPTEN_KEEPALIVE *InitializeWorldParameters(int x_length, int fs, int sample_length)
{
  WorldParameters *world_parameters = (WorldParameters *)malloc(sizeof(WorldParameters));
  world_parameters->dio_option = (DioOption *)malloc(sizeof(DioOption));
  world_parameters->cheap_trick_option = (CheapTrickOption *)malloc(sizeof(CheapTrickOption));
  world_parameters->d4c_option = (D4COption *)malloc(sizeof(D4COption));

  world_parameters->x_length = x_length;
  world_parameters->fs = fs;
  world_parameters->frame_period = (double)sample_length / (double)fs * 1000.0;

  InitializeDioOption(world_parameters->dio_option);
  world_parameters->dio_option->frame_period = world_parameters->frame_period;
  world_parameters->dio_option->speed = 1;
  world_parameters->dio_option->f0_floor = 40.0;
  world_parameters->dio_option->allowed_range = 0.1;
  world_parameters->f0_length = GetSamplesForDIO(world_parameters->fs, x_length, world_parameters->frame_period);
  world_parameters->f0 = (double *)malloc(sizeof(double) * (world_parameters->f0_length));
  world_parameters->time_axis = (double *)malloc(sizeof(double) * (world_parameters->f0_length));
  world_parameters->refined_f0 = (double *)malloc(sizeof(double) * (world_parameters->f0_length));

  InitializeCheapTrickOption(world_parameters->fs, world_parameters->cheap_trick_option);
  world_parameters->cheap_trick_option->f0_floor = 71.0;
  world_parameters->fft_size = world_parameters->cheap_trick_option->fft_size = GetFFTSizeForCheapTrick(world_parameters->fs, world_parameters->cheap_trick_option);
  ;
  world_parameters->spectrogram = (double **)malloc(sizeof(double *) * (world_parameters->f0_length));
  for (int i = 0; i < world_parameters->f0_length; ++i)
    world_parameters->spectrogram[i] =
        (double *)malloc(sizeof(double) * (world_parameters->fft_size / 2 + 1));

  InitializeD4COption(world_parameters->d4c_option);
  world_parameters->d4c_option->threshold = 0.05;
  world_parameters->aperiodicity = (double **)malloc(sizeof(double *) * (world_parameters->f0_length));
  for (int i = 0; i < world_parameters->f0_length; ++i)
    world_parameters->aperiodicity[i] =
        (double *)malloc(sizeof(double) * (world_parameters->fft_size / 2 + 1));

  world_parameters->freq_axis1 = (double *)malloc(sizeof(double) * (world_parameters->fft_size));
  world_parameters->freq_axis2 = (double *)malloc(sizeof(double) * (world_parameters->fft_size));
  world_parameters->spectrum1 = (double *)malloc(sizeof(double) * (world_parameters->fft_size));
  world_parameters->spectrum2 = (double *)malloc(sizeof(double) * (world_parameters->fft_size));

  return world_parameters;
}

static void F0EstimationDio(double *x, WorldParameters *world_parameters)
{

  Dio(x, world_parameters->x_length, world_parameters->fs, world_parameters->dio_option, world_parameters->time_axis,
      world_parameters->f0);

  // StoneMask is carried out to improve the estimation performance.
  StoneMask(x, world_parameters->x_length, world_parameters->fs, world_parameters->time_axis,
            world_parameters->f0, world_parameters->f0_length, world_parameters->refined_f0);

  for (int i = 0; i < world_parameters->f0_length; ++i)
    world_parameters->f0[i] = world_parameters->refined_f0[i];
}

static void SpectralEnvelopeEstimation(double *x, WorldParameters *world_parameters)
{
  CheapTrick(x, world_parameters->x_length, world_parameters->fs, world_parameters->time_axis,
             world_parameters->f0, world_parameters->f0_length, world_parameters->cheap_trick_option,
             world_parameters->spectrogram);
}

static void AperiodicityEstimation(double *x, WorldParameters *world_parameters)
{
  D4C(x, world_parameters->x_length, world_parameters->fs, world_parameters->time_axis,
      world_parameters->f0, world_parameters->f0_length,
      world_parameters->fft_size, world_parameters->d4c_option, world_parameters->aperiodicity);
}

static void ParameterModification(double shift, double ratio, WorldParameters *world_parameters)
{
  for (int i = 0; i < world_parameters->f0_length; ++i)
    world_parameters->f0[i] *= shift;

  for (int i = 0; i <= world_parameters->fft_size / 2; ++i)
  {
    world_parameters->freq_axis1[i] = ratio * i / world_parameters->fft_size * world_parameters->fs;
    world_parameters->freq_axis2[i] = (double)(i) / world_parameters->fft_size * world_parameters->fs;
  }
  for (int i = 0; i < world_parameters->f0_length; ++i)
  {
    for (int j = 0; j <= world_parameters->fft_size / 2; ++j)
      world_parameters->spectrum1[j] = log(world_parameters->spectrogram[i][j]);
    interp1(world_parameters->freq_axis1, world_parameters->spectrum1, world_parameters->fft_size / 2 + 1, world_parameters->freq_axis2,
            world_parameters->fft_size / 2 + 1, world_parameters->spectrum2);
    for (int j = 0; j <= world_parameters->fft_size / 2; ++j)
      world_parameters->spectrogram[i][j] = exp(world_parameters->spectrum2[j]);
    if (ratio >= 1.0)
      continue;
    for (int j = (int)(world_parameters->fft_size / 2.0 * ratio);
         j <= world_parameters->fft_size / 2; ++j)
      world_parameters->spectrogram[i][j] =
          world_parameters->spectrogram[i][(int)(world_parameters->fft_size / 2.0 * ratio) - 1];
  }
}

static void WaveformSynthesis(WorldParameters *world_parameters, double *y)
{
  // Synthesis by the aperiodicity
  Synthesis(world_parameters->f0, world_parameters->f0_length,
            world_parameters->spectrogram, world_parameters->aperiodicity,
            world_parameters->fft_size, world_parameters->frame_period, world_parameters->fs,
            world_parameters->x_length, y);
}

void WaveformSynthesis2(WorldParameters *world_parameters, double *y)
{

  WorldSynthesizer synthesizer = {0};
  int buffer_size = 64;
  InitializeSynthesizer(world_parameters->fs, world_parameters->frame_period,
                        world_parameters->fft_size, buffer_size, 1, &synthesizer);

  // All parameters are added at the same time.
  AddParameters(world_parameters->f0, world_parameters->f0_length,
                world_parameters->spectrogram, world_parameters->aperiodicity,
                &synthesizer);

  int index;
  for (int i = 0; Synthesis2(&synthesizer) != 0; ++i)
  {
    index = i * buffer_size;
    for (int j = 0; j < buffer_size; ++j)
      y[j + index] = synthesizer.buffer[j];
  }

  DestroySynthesizer(&synthesizer);
}

void WaveformSynthesis3(WorldParameters *world_parameters, double *y)
{

  WorldSynthesizer synthesizer = {0};
  int buffer_size = 64;
  InitializeSynthesizer(world_parameters->fs, world_parameters->frame_period,
                        world_parameters->fft_size, buffer_size, 100, &synthesizer);

  int offset = 0;
  int index = 0;
  for (int i = 0; i < world_parameters->f0_length;)
  {
    // Add one frame (i shows the frame index that should be added)
    if (AddParameters(&world_parameters->f0[i], 1,
                      &world_parameters->spectrogram[i], &world_parameters->aperiodicity[i],
                      &synthesizer) == 1)
      ++i;

    // Synthesize speech with length of buffer_size sample.
    // It is repeated until the function returns 0
    // (it suggests that the synthesizer cannot generate speech).
    while (Synthesis2(&synthesizer) != 0)
    {
      index = offset * buffer_size;
      for (int j = 0; j < buffer_size; ++j)
        y[j + index] = synthesizer.buffer[j];
      offset++;
    }

    // Check the "Lock" (Please see synthesisrealtime.h)
    if (IsLocked(&synthesizer) == 1)
    {
      break;
    }
  }

  DestroySynthesizer(&synthesizer);
}

void EMSCRIPTEN_KEEPALIVE transform(double *x, WorldParameters *world_parameters, double shift, double ratio, double *y)
{
  F0EstimationDio(x, world_parameters);
  // Spectral envelope estimation
  SpectralEnvelopeEstimation(x, world_parameters);
  // Aperiodicity estimation by D4C
  AperiodicityEstimation(x, world_parameters);
  // Note that F0 must not be changed until all parameters are estimated.
  ParameterModification(shift, ratio, world_parameters);
  // Synthesis 1 (conventional synthesis)
  WaveformSynthesis(world_parameters, y);
}
