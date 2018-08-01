export const enum CONSTANTS {
  FRAMES_PER_WORKLET_PROCESS = 128,
  FRAMES_PER_F0_ESTIMATION_FRAME_PERIOD = 256,
  RING_BUFFER_LENGTH = 512,
  RING_BUFFER_BYTE_LENGTH = CONSTANTS.FRAMES_PER_WORKLET_PROCESS * CONSTANTS.RING_BUFFER_LENGTH * 8,
  F0_ESTIMATION_BUFFER_LENGTH = CONSTANTS.RING_BUFFER_LENGTH / 2,
  F0_ESTIMATION_BUFFER_BYTE_LENGTH = CONSTANTS.F0_ESTIMATION_BUFFER_LENGTH * CONSTANTS.FRAMES_PER_WORKLET_PROCESS * 8
}

export const enum STATE {
  REQUEST_RENDER,
  STREAM_INITIALIZED,
  INPUT_INDEX,
  OUTPUT_INDEX,
  SHIFT,
  RATIO
}
