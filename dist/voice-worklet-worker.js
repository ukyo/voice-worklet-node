importScripts("../dist/foo.js");
const States = new Int32Array(new SharedArrayBuffer(6 * Int32Array.BYTES_PER_ELEMENT));
const InputRingBuffer = new Float64Array(new SharedArrayBuffer(524288 /* RING_BUFFER_BYTE_LENGTH */));
const OutputRingBuffer = new Float64Array(new SharedArrayBuffer(524288 /* RING_BUFFER_BYTE_LENGTH */));
function init() {
    const SharedBuffers = {
        states: States.buffer,
        inputRingBuffer: InputRingBuffer.buffer,
        outputRingBuffer: OutputRingBuffer.buffer
    };
    postMessage({
        message: "WORKER_READY",
        SharedBuffers
    });
    waitOnRenderRequest();
}
let inputCount = 0;
let outputCount = 0;
function doHeavyTask() {
    const x = Module._malloc(262144 /* F0_ESTIMATION_BUFFER_BYTE_LENGTH */);
    const y = Module._malloc(262144 /* F0_ESTIMATION_BUFFER_BYTE_LENGTH */);
    const shift = States[4 /* SHIFT */] / 100;
    const ratio = States[5 /* RATIO */] / 100;
    const len = 256 /* F0_ESTIMATION_BUFFER_LENGTH */ * 128 /* FRAMES_PER_WORKLET_PROCESS */;
    const xF64arr = new Float64Array(Module.buffer, x, len);
    const yF64arr = new Float64Array(Module.buffer, y, len);
    const inputOffset = (inputCount % 2) * len;
    xF64arr.set(InputRingBuffer.subarray(inputOffset, inputOffset + len));
    inputCount++;
    Module._transform(x, 256 /* F0_ESTIMATION_BUFFER_LENGTH */ * 128 /* FRAMES_PER_WORKLET_PROCESS */, 48000, 256 /* F0_ESTIMATION_BUFFER_LENGTH */, shift, ratio, y);
    OutputRingBuffer.set(yF64arr, (outputCount % 2) * len);
    outputCount++;
    Module._free(x);
    Module._free(y);
}
function through() {
    const len = 256 /* F0_ESTIMATION_BUFFER_LENGTH */ * 128 /* FRAMES_PER_WORKLET_PROCESS */;
    const inputOffset = (inputCount % 2) * len;
    const inputBuff = InputRingBuffer.subarray(inputOffset, inputOffset + len);
    inputCount++;
    OutputRingBuffer.set(inputBuff, (outputCount % 2) * len);
    outputCount++;
}
function waitOnRenderRequest() {
    while (Atomics.wait(States, 0 /* REQUEST_RENDER */, 0) === "ok") {
        doHeavyTask();
        // through();
        Atomics.store(States, 0 /* REQUEST_RENDER */, 0);
    }
}
// onmessage = eventFromMain => {
//   initialize();
// };
Module.onRuntimeInitialized = () => {
    init();
    // console.log("hello");
};
