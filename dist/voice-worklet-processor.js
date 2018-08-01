class VoiceWorkletProcessor extends AudioWorkletProcessor {
    constructor() {
        super();
        this._initialized = false;
        this._canPlay = false;
        this.port.onmessage = this._initializedOnEvent.bind(this);
    }
    _initializedOnEvent(event) {
        const sharedBuffers = event.data;
        this._states = new Int32Array(sharedBuffers.states);
        this._inputRingBuffer = new Float64Array(sharedBuffers.inputRingBuffer);
        this._outputRingBuffer = new Float64Array(sharedBuffers.outputRingBuffer);
        this._initialized = true;
        this.port.postMessage({
            message: "PROCESSOR_READY"
        });
    }
    static get parameterDescriptors() {
        return [{ name: "gain", defaultValue: 1 }];
    }
    process(inputs, outputs, params) {
        if (!this._initialized) {
            return true;
        }
        const input = inputs[0];
        const output = outputs[0];
        this._putInputChannelData(input);
        if (this._states[2 /* INPUT_INDEX */] % 256 /* F0_ESTIMATION_BUFFER_LENGTH */ === 0) {
            Atomics.wake(this._states, 0 /* REQUEST_RENDER */, 1);
        }
        if (this._states[2 /* INPUT_INDEX */] >= 512 /* RING_BUFFER_LENGTH */) {
            this._putOutputChannelData(output);
        }
        return true;
    }
    _putInputChannelData(input) {
        const channel = input[0];
        const offset = (this._states[2 /* INPUT_INDEX */] % 512 /* RING_BUFFER_LENGTH */) * 128 /* FRAMES_PER_WORKLET_PROCESS */;
        for (let i = 0; i < 128 /* FRAMES_PER_WORKLET_PROCESS */; ++i) {
            this._inputRingBuffer[i + offset] = channel[i];
        }
        this._states[2 /* INPUT_INDEX */]++;
    }
    _putOutputChannelData(output) {
        const offset = (this._states[3 /* OUTPUT_INDEX */] % 512 /* RING_BUFFER_LENGTH */) * 128 /* FRAMES_PER_WORKLET_PROCESS */;
        for (let i = 0; i < output.length; ++i) {
            for (let j = 0; j < 128 /* FRAMES_PER_WORKLET_PROCESS */; ++j) {
                output[i][j] = this._outputRingBuffer[j + offset];
            }
        }
        this._states[3 /* OUTPUT_INDEX */]++;
    }
}
registerProcessor("voice-worklet-processor", VoiceWorkletProcessor);
