const defaultVoiceWorkletNodeOptions = {
    shift: 100,
    ratio: 100
};
export class VoiceWorkletNode extends AudioWorkletNode {
    constructor(context, options = defaultVoiceWorkletNodeOptions) {
        super(context, "voice-worklet-processor", Object.assign({}, defaultVoiceWorkletNodeOptions, options));
        this._states = new Int32Array(new SharedArrayBuffer(7 * Int32Array.BYTES_PER_ELEMENT));
        this._inputRingBuffer = new Float64Array(new SharedArrayBuffer(262144 /* RING_BUFFER_BYTE_LENGTH */));
        this._outputRingBuffer = new Float64Array(new SharedArrayBuffer(262144 /* RING_BUFFER_BYTE_LENGTH */));
        this._sharedBuffers = {
            states: this._states.buffer,
            inputRingBuffer: this._inputRingBuffer.buffer,
            outputRingBuffer: this._outputRingBuffer.buffer
        };
        this._options = Object.assign({}, defaultVoiceWorkletNodeOptions, options);
        this._worker = new Worker("../dist/voice-worklet-worker.js");
        this._worker.onmessage = this._onWorkerInitialized.bind(this);
        this.port.onmessage = this._onProcessorInitilized.bind(this);
        this.setShift(this._options.shift);
        this.setRatio(this._options.ratio);
        this.setSampleRate(this.context.sampleRate);
        this._worker.postMessage(this._sharedBuffers);
    }
    _onWorkerInitialized(eventFromWorker) {
        this.port.postMessage(this._sharedBuffers);
    }
    _onProcessorInitilized(eventFromProcessor) {
        if (typeof this.onInitialized === "function")
            this.onInitialized();
    }
    setShift(shift) {
        this._states[4 /* SHIFT */] = shift;
    }
    setRatio(ratio) {
        this._states[5 /* RATIO */] = ratio;
    }
    setSampleRate(sampleRate) {
        this._states[6 /* SAMPLE_RATE */] =
            Math.ceil(sampleRate / 256 /* FRAMES_PER_F0_ESTIMATION_FRAME_PERIOD */) *
                256 /* FRAMES_PER_F0_ESTIMATION_FRAME_PERIOD */;
        // this._states[STATE.SAMPLE_RATE] = sampleRate;
    }
}
