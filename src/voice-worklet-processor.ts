import { STATE, CONSTANTS } from "./constants";

declare class AudioWorkletProcessor {
  process(inputs: Array<Array<Float32Array>>, outputs: Array<Array<Float32Array>>, params: any): boolean;
  port: any;
}
declare function registerProcessor(name: string, processor: any): void;

class VoiceWorkletProcessor extends AudioWorkletProcessor {
  private _initialized = false;
  private _canPlay = false;
  private _states!: Int32Array;
  private _inputRingBuffer!: Float64Array;
  private _outputRingBuffer!: Float64Array;

  constructor() {
    super();
    this.port.onmessage = this._initializedOnEvent.bind(this);
  }

  private _initializedOnEvent(event: MessageEvent) {
    const sharedBuffers = event.data;

    this._states = new Int32Array(sharedBuffers.states);
    this._inputRingBuffer = new Float64Array(sharedBuffers.inputRingBuffer);
    this._outputRingBuffer = new Float64Array(sharedBuffers.outputRingBuffer);

    this._initialized = true;
    this.port.postMessage({
      message: "PROCESSOR_READY"
    });
  }

  process(inputs: Array<Array<Float32Array>>, outputs: Array<Array<Float32Array>>) {
    if (!this._initialized) {
      return true;
    }

    const input = inputs[0];
    const output = outputs[0];

    this._putInputChannelData(input);
    if (this._states[STATE.INPUT_INDEX] % CONSTANTS.F0_ESTIMATION_BUFFER_LENGTH === 0) {
      Atomics.wake(this._states, STATE.REQUEST_RENDER, 1);
    }
    if (this._states[STATE.INPUT_INDEX] >= CONSTANTS.RING_BUFFER_LENGTH) {
      this._putOutputChannelData(output);
    }
    return true;
  }

  private _putInputChannelData(input: Array<Float32Array>) {
    const channel = input[0];
    const offset =
      (this._states[STATE.INPUT_INDEX] % CONSTANTS.RING_BUFFER_LENGTH) * CONSTANTS.FRAMES_PER_WORKLET_PROCESS;
    for (let i = 0; i < CONSTANTS.FRAMES_PER_WORKLET_PROCESS; ++i) {
      this._inputRingBuffer[i + offset] = channel[i];
    }
    this._states[STATE.INPUT_INDEX]++;
  }

  private _putOutputChannelData(output: Array<Float32Array>) {
    const offset =
      (this._states[STATE.OUTPUT_INDEX] % CONSTANTS.RING_BUFFER_LENGTH) * CONSTANTS.FRAMES_PER_WORKLET_PROCESS;
    for (let i = 0; i < output.length; ++i) {
      for (let j = 0; j < CONSTANTS.FRAMES_PER_WORKLET_PROCESS; ++j) {
        output[i][j] = this._outputRingBuffer[j + offset];
      }
    }
    this._states[STATE.OUTPUT_INDEX]++;
  }
}

registerProcessor("voice-worklet-processor", VoiceWorkletProcessor);
