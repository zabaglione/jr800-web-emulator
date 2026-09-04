// SPDX-License-Identifier: MIT

const DEFAULT_AMPLITUDE = 0.12;
const RELEASE_SECONDS = 0.004;
const MAX_PENDING_TRANSITIONS = 65_536;

function requirePositive(value, label) {
    if (!Number.isFinite(value) || value <= 0) {
        throw new RangeError(`${label} must be positive`);
    }
    return value;
}

export function renderAudioTransitions(
    transitions,
    clockHz,
    sampleRate,
    amplitude = DEFAULT_AMPLITUDE,
) {
    requirePositive(clockHz, "Audio clock");
    requirePositive(sampleRate, "Audio sample rate");
    requirePositive(amplitude, "Audio amplitude");
    if (!Array.isArray(transitions) || transitions.length < 2) {
        return new Float32Array();
    }
    let previousCycle = -1;
    for (const transition of transitions) {
        if (!Number.isSafeInteger(transition.cycle)
            || transition.cycle < 0
            || transition.cycle < previousCycle
            || typeof transition.level !== "boolean") {
            throw new TypeError("Audio transitions must be ordered cycle levels");
        }
        previousCycle = transition.cycle;
    }

    const firstCycle = transitions[0].cycle;
    const finalCycle = transitions.at(-1).cycle;
    const signalSamples = Math.max(
        1,
        Math.ceil((finalCycle - firstCycle) * sampleRate / clockHz),
    );
    const releaseSamples = Math.max(1, Math.ceil(RELEASE_SECONDS * sampleRate));
    const samples = new Float32Array(signalSamples + releaseSamples);
    let transitionIndex = 0;
    let level = transitions[0].level;
    for (let sample = 0; sample < signalSamples; ++sample) {
        const cycle = firstCycle + sample * clockHz / sampleRate;
        while (transitionIndex + 1 < transitions.length
            && transitions[transitionIndex + 1].cycle <= cycle) {
            ++transitionIndex;
            level = transitions[transitionIndex].level;
        }
        samples[sample] = level ? amplitude : -amplitude;
    }
    const finalValue = samples[signalSamples - 1];
    for (let index = 0; index < releaseSamples; ++index) {
        samples[signalSamples + index] = finalValue
            * (1 - (index + 1) / releaseSamples);
    }
    return samples;
}

export class Jr800AudioOutput {
    constructor(contextFactory = () => {
        const AudioContextClass = globalThis.AudioContext
            ?? globalThis.webkitAudioContext;
        return AudioContextClass ? new AudioContextClass() : null;
    }) {
        this.contextFactory = contextFactory;
        this.context = null;
        this.enabled = true;
        this.clockHz = null;
        this.transitions = [];
        this.nextStartTime = 0;
        this.sources = new Set();
    }

    async activate() {
        if (!this.enabled) {
            return false;
        }
        if (this.context === null) {
            this.context = this.contextFactory();
        }
        if (this.context === null) {
            return false;
        }
        if (this.context.state === "suspended") {
            await this.context.resume();
        }
        return this.context.state === "running";
    }

    setEnabled(enabled) {
        this.enabled = Boolean(enabled);
        if (!this.enabled) {
            this.reset();
        }
    }

    append({clockHz, transitions}) {
        if (!this.enabled || !Array.isArray(transitions)
            || transitions.length === 0) {
            return;
        }
        requirePositive(clockHz, "Audio clock");
        if (this.clockHz !== null && this.clockHz !== clockHz) {
            this.transitions = [];
        }
        this.clockHz = clockHz;
        this.transitions.push(...transitions);
        if (this.transitions.length > MAX_PENDING_TRANSITIONS) {
            this.transitions.splice(
                0,
                this.transitions.length - MAX_PENDING_TRANSITIONS,
            );
        }
    }

    flush() {
        const transitions = this.transitions;
        this.transitions = [];
        if (!this.enabled || this.context === null
            || this.context.state !== "running"
            || transitions.length < 2) {
            return false;
        }
        const samples = renderAudioTransitions(
            transitions,
            this.clockHz,
            this.context.sampleRate,
        );
        if (samples.length === 0) {
            return false;
        }
        const buffer = this.context.createBuffer(
            1,
            samples.length,
            this.context.sampleRate,
        );
        buffer.copyToChannel(samples, 0);
        const source = this.context.createBufferSource();
        source.buffer = buffer;
        source.connect(this.context.destination);
        const startTime = Math.max(
            this.context.currentTime + 0.01,
            this.nextStartTime,
        );
        source.start(startTime);
        this.nextStartTime = startTime + buffer.duration;
        this.sources.add(source);
        source.addEventListener("ended", () => this.sources.delete(source), {
            once: true,
        });
        return true;
    }

    reset() {
        this.transitions = [];
        this.clockHz = null;
        this.nextStartTime = 0;
        for (const source of this.sources) {
            try {
                source.stop();
            } catch {
                // The source has already stopped.
            }
        }
        this.sources.clear();
    }
}
