// SPDX-License-Identifier: MIT

const TAPS = 64;
const PHASES = 256;
const AMPLITUDE = 0.12;
// Fractional-delay, windowed-sinc impulses. Integrating each edge's impulse
// band-limits the continuous port signal BEFORE sampling, avoiding folded harmonics.
function makeKernel() {
    const rows = [];
    const cutoff = 0.4;
    for (let phase = 0; phase <= PHASES; ++phase) {
        const row = new Float64Array(TAPS);
        let sum = 0;
        for (let tap = 0; tap < TAPS; ++tap) {
            const x = tap - (TAPS - 1) / 2 - phase / PHASES;
            const window = Math.abs(x) > TAPS / 2 ? 0
                : 0.42 + 0.5 * Math.cos(2 * Math.PI * x / TAPS)
                    + 0.08 * Math.cos(4 * Math.PI * x / TAPS);
            row[tap] = (Math.abs(x) < 1e-12 ? 2 * cutoff
                : Math.sin(2 * Math.PI * cutoff * x) / (Math.PI * x)) * window;
            sum += row[tap];
        }
        for (let tap = 0; tap < TAPS; ++tap) row[tap] /= sum;
        rows.push(row);
    }
    return rows;
}
const KERNEL = makeKernel();

export function validateAudioFrame({clockHz, startCycle, endCycle, initialLevel, transitions}) {
    if (!Number.isFinite(clockHz) || clockHz <= 0) {
        throw new RangeError("Audio clock must be positive");
    }
    if (!Number.isSafeInteger(startCycle) || startCycle < 0
        || !Number.isSafeInteger(endCycle) || endCycle < startCycle
        || (initialLevel !== null && typeof initialLevel !== "boolean")
        || !Array.isArray(transitions)) {
        throw new TypeError("Invalid audio frame");
    }
    let previousCycle = startCycle;
    for (const {cycle, level} of transitions) {
        if (!Number.isSafeInteger(cycle) || cycle < previousCycle
            || cycle > endCycle || typeof level !== "boolean") {
            throw new TypeError("Audio transitions must be ordered cycle levels within the frame");
        }
        previousCycle = cycle;
    }
}

export class Jr800AudioSignal {
    constructor(clockHz, sampleRate, startCycle, initialLevel) {
        this.clockHz = clockHz;
        this.sampleRate = sampleRate;
        this.originCycle = startCycle;
        this.endCycle = startCycle;
        this.frame = 0;
        this.level = initialLevel === null ? 0 : initialLevel ? AMPLITUDE : -AMPLITUDE;
        this.value = 0;
        this.impulses = new Float64Array(TAPS);
        this.pending = [];
    }

    render(packet) {
        validateAudioFrame(packet);
        if (packet.clockHz !== this.clockHz || packet.startCycle !== this.endCycle) {
            throw new RangeError("Audio frames must be contiguous");
        }
        this.pending.push(...packet.transitions);
        const endFrame = Math.floor(
            (packet.endCycle - this.originCycle) * this.sampleRate / this.clockHz,
        );
        const samples = new Float32Array(endFrame - this.frame);
        let edge = 0;
        for (let index = 0; index < samples.length; ++index, ++this.frame) {
            while (edge < this.pending.length) {
                const transition = this.pending[edge];
                const position = (transition.cycle - this.originCycle)
                    * this.sampleRate / this.clockHz;
                if (Math.floor(position) > this.frame) break;
                const level = transition.level ? AMPLITUDE : -AMPLITUDE;
                const delta = level - this.level;
                this.level = level;
                const phase = (position - Math.floor(position)) * PHASES;
                const row = Math.floor(phase);
                const blend = phase - row;
                for (let tap = 0; tap < TAPS; ++tap) {
                    this.impulses[(this.frame + tap) % TAPS] += delta
                        * (KERNEL[row][tap] * (1 - blend) + KERNEL[row + 1][tap] * blend);
                }
                ++edge;
            }
            this.value += this.impulses[this.frame % TAPS];
            this.impulses[this.frame % TAPS] = 0;
            samples[index] = this.value;
        }
        this.pending.splice(0, edge);
        this.endCycle = packet.endCycle;
        return samples;
    }
}
