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
  private _data: any;
  private _options: VoiceWorkletNodeOptions;

  constructor(context: AudioContextBase, options: VoiceWorkletNodeOptions = defaultVoiceWorkletNodeOptions) {
    super(context, "voice-worklet-processor", { ...defaultVoiceWorkletNodeOptions, ...options });
    this._options = { ...defaultVoiceWorkletNodeOptions, ...options };

    this._worker = new Worker("../dist/voice-worklet-worker.js");
    this._worker.onmessage = this._onWorkerInitialized.bind(this);
    this.port.onmessage = this._onProcessorInitilized.bind(this);

    this._worker.postMessage({
      message: "INITIALIZE_WORKER"
    });
  }

  _onWorkerInitialized(eventFromWorker: MessageEvent) {
    const data = eventFromWorker.data;
    this._data = data;
    this.setShift(this._options.shift);
    this.setRatio(this._options.ratio);
    this.setSampleRate(this.context.sampleRate);
    this.port.postMessage(data.SharedBuffers);
  }

  _onProcessorInitilized(eventFromProcessor: MessageEvent) {
    if (typeof this.onInitialized === "function") this.onInitialized();
  }

  setShift(shift: number) {
    if (this._data) {
      const arr = new Int32Array(this._data.SharedBuffers.states);
      arr[STATE.SHIFT] = shift;
    }
  }

  setRatio(ratio: number) {
    if (this._data) {
      const arr = new Int32Array(this._data.SharedBuffers.states);
      arr[STATE.RATIO] = ratio;
    }
  }

  setSampleRate(sampleRate: number) {
    if (this._data) {
      const arr = new Int32Array(this._data.SharedBuffers.states);
      arr[STATE.SAMPLE_RATE] = Math.ceil(sampleRate / CONSTANTS.F0_ESTIMATION_BUFFER_LENGTH) * CONSTANTS.F0_ESTIMATION_BUFFER_LENGTH;
    }
  }
}
