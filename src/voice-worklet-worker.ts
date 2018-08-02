import { STATE, CONSTANTS, SharedBuffers } from "./constants";

declare var Module: any;

function init(sharedBuffers: SharedBuffers) {
  const states = new Int32Array(sharedBuffers.states);
  const inputRingBuffer = new Float64Array(sharedBuffers.inputRingBuffer);
  const outputRingBuffer = new Float64Array(sharedBuffers.outputRingBuffer);
  importScripts("../dist/voice-transformer.js");

  Module.onRuntimeInitialized = () => {
    let count = 0;
    const len = CONSTANTS.F0_ESTIMATION_BUFFER_LENGTH * CONSTANTS.FRAMES_PER_WORKLET_PROCESS;

    const worldParameters = Module._InitializeWorldParameters(
      len,
      states[STATE.SAMPLE_RATE],
      CONSTANTS.FRAMES_PER_F0_ESTIMATION_FRAME_PERIOD
    );
    const x = Module._malloc(CONSTANTS.F0_ESTIMATION_BUFFER_BYTE_LENGTH);
    const y = Module._malloc(CONSTANTS.F0_ESTIMATION_BUFFER_BYTE_LENGTH);

    function transform() {
      const shift = states[STATE.SHIFT] / 100;
      const ratio = states[STATE.RATIO] / 100;
      const xF64arr = new Float64Array(Module.buffer, x, len);
      const yF64arr = new Float64Array(Module.buffer, y, len);
      const inputOffset = (count % 2) * len;
      xF64arr.set(inputRingBuffer.subarray(inputOffset, inputOffset + len));
      Module._transform(x, worldParameters, shift, ratio, y);
      outputRingBuffer.set(yF64arr, (count % 2) * len);
      count++;
    }

    function waitOnRenderRequest() {
      while (Atomics.wait(states, STATE.REQUEST_RENDER, 0) === "ok") {
        transform();
        Atomics.store(states, STATE.REQUEST_RENDER, 0);
      }
    }

    (postMessage as any)({ message: "WORKER_READY" });

    waitOnRenderRequest();
  };
}

onmessage = eventFromMain => {
  init(eventFromMain.data);
};
