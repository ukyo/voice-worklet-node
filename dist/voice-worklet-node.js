const defaultVoiceWorkletNodeOptions = {
    shift: 100,
    ratio: 100
};
export class VoiceWorkletNode extends AudioWorkletNode {
    constructor(context, options = defaultVoiceWorkletNodeOptions) {
        super(context, "voice-worklet-processor", Object.assign({}, defaultVoiceWorkletNodeOptions, options));
        this._options = Object.assign({}, defaultVoiceWorkletNodeOptions, options);
        this._worker = new Worker("../dist/voice-worklet-worker.js");
        this._worker.onmessage = this._onWorkerInitialized.bind(this);
        this.port.onmessage = this._onProcessorInitilized.bind(this);
        this._worker.postMessage({
            message: "INITIALIZE_WORKER"
        });
    }
    _onWorkerInitialized(eventFromWorker) {
        const data = eventFromWorker.data;
        this._data = data;
        this.setShift(this._options.shift);
        this.setRatio(this._options.ratio);
        this.port.postMessage(data.SharedBuffers);
    }
    _onProcessorInitilized(eventFromProcessor) {
        if (typeof this.onInitialized === "function")
            this.onInitialized();
    }
    setShift(shift) {
        if (this._data) {
            const arr = new Int32Array(this._data.SharedBuffers.states);
            arr[4 /* SHIFT */] = shift;
        }
    }
    setRatio(ratio) {
        if (this._data) {
            const arr = new Int32Array(this._data.SharedBuffers.states);
            arr[5 /* RATIO */] = ratio;
        }
    }
}
