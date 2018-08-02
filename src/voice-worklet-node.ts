import { STATE, CONSTANTS } from "./constants";

declare class AudioWorkletNode {
  constructor(context: AudioContextBase, processorName: string, options: any);
  port: any;
  onInitialized(): void;
  context: AudioContext;
}

export interface VoiceWorkletNodeOptions {
  shift: number; // unsigned number
  ratio: number; // unsigned number
}

const defaultVoiceWorkletNodeOptions: VoiceWorkletNodeOptions = {
  shift: 100,
  ratio: 100
};

export class VoiceWorkletNode extends AudioWorkletNode {
  private _worker: Worker;
  private _options: VoiceWorkletNodeOptions;

  private _states = new Int32Array(new SharedArrayBuffer(7 * Int32Array.BYTES_PER_ELEMENT));
  private _inputRingBuffer = new Float64Array(new SharedArrayBuffer(CONSTANTS.RING_BUFFER_BYTE_LENGTH));
  private _outputRingBuffer = new Float64Array(new SharedArrayBuffer(CONSTANTS.RING_BUFFER_BYTE_LENGTH));
  private _sharedBuffers = {
    states: this._states.buffer,
    inputRingBuffer: this._inputRingBuffer.buffer,
    outputRingBuffer: this._outputRingBuffer.buffer
  };

  constructor(context: AudioContextBase, options: VoiceWorkletNodeOptions = defaultVoiceWorkletNodeOptions) {
    super(context, "voice-worklet-processor", { ...defaultVoiceWorkletNodeOptions, ...options });
    this._options = { ...defaultVoiceWorkletNodeOptions, ...options };

    this._worker = new Worker("../dist/voice-worklet-worker.js");
    this._worker.onmessage = this._onWorkerInitialized.bind(this);
    this.port.onmessage = this._onProcessorInitilized.bind(this);

    this.setShift(this._options.shift);
    this.setRatio(this._options.ratio);
    this.setSampleRate(this.context.sampleRate);

    this._worker.postMessage(this._sharedBuffers);
  }

  _onWorkerInitialized(eventFromWorker: MessageEvent) {
    this.port.postMessage(this._sharedBuffers);
  }

  _onProcessorInitilized(eventFromProcessor: MessageEvent) {
    if (typeof this.onInitialized === "function") this.onInitialized();
  }

  setShift(shift: number) {
    this._states[STATE.SHIFT] = shift;
  }

  setRatio(ratio: number) {
    this._states[STATE.RATIO] = ratio;
  }

  setSampleRate(sampleRate: number) {
    this._states[STATE.SAMPLE_RATE] =
      Math.ceil(sampleRate / CONSTANTS.FRAMES_PER_F0_ESTIMATION_FRAME_PERIOD) *
      CONSTANTS.FRAMES_PER_F0_ESTIMATION_FRAME_PERIOD;
    // this._states[STATE.SAMPLE_RATE] = sampleRate;
  }
}
