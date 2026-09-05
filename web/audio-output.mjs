// SPDX-License-Identifier: MIT

import {Jr800AudioSignal, validateAudioFrame} from "./audio-signal.mjs";

const PLAYBACK_LEAD_SECONDS = 0.025;
const MINIMUM_LEAD_SECONDS = 0.003;
const MAXIMUM_LEAD_SECONDS = 0.1;

export class Jr800AudioOutput {
    constructor(contextFactory = () => {
        const AudioContextClass = globalThis.AudioContext;
        return AudioContextClass
            ? new AudioContextClass({latencyHint: "interactive"}) : null;
    }) {
        this.contextFactory = contextFactory;
        this.context = null;
        this.enabled = true;
        this.signal = null;
        this.filter = null;
        this.nextStartTime = 0;
        this.sources = new Set();
    }

    async activate() {
        if (!this.enabled) return false;
        if (this.context === null) this.context = this.contextFactory();
        if (this.context === null) return false;
        if (this.context.state === "suspended") {
            this.reset();
            await this.context.resume();
        }
        return this.context.state === "running";
    }

    setEnabled(enabled) {
        this.enabled = Boolean(enabled);
        if (!this.enabled) this.reset();
    }

    append(packet) {
        if (!this.enabled) return;
        validateAudioFrame(packet);
        if (this.context === null || this.context.state !== "running") {
            this.reset();
            return;
        }
        const now = this.context.currentTime;
        if (this.signal === null || packet.clockHz !== this.signal.clockHz
            || packet.startCycle !== this.signal.endCycle
            || this.nextStartTime < now + MINIMUM_LEAD_SECONDS
            || this.nextStartTime > now + MAXIMUM_LEAD_SECONDS) {
            // A stalled or unpaced host must not accumulate a playback backlog.
            this.reset();
            this.signal = new Jr800AudioSignal(packet.clockHz,
                this.context.sampleRate, packet.startCycle, packet.initialLevel);
            this.nextStartTime = now + PLAYBACK_LEAD_SECONDS;
            this.filter = this.context.createBiquadFilter();
            this.filter.type = "highpass";
            this.filter.frequency.value = 20;
            this.filter.Q.value = Math.SQRT1_2;
            this.filter.connect(this.context.destination);
        }
        const samples = this.signal.render(packet);
        if (samples.length === 0) return;
        const buffer = this.context.createBuffer(1, samples.length, this.context.sampleRate);
        buffer.copyToChannel(samples, 0);
        const source = this.context.createBufferSource();
        source.buffer = buffer;
        source.connect(this.filter);
        // Adjacent packets share both the resampler history and sample timeline.
        source.start(this.nextStartTime);
        this.nextStartTime += buffer.duration;
        this.sources.add(source);
        source.addEventListener("ended", () => {
            source.disconnect();
            this.sources.delete(source);
        }, {once: true});
    }

    reset() {
        for (const source of this.sources) {
            source.stop();
            source.disconnect();
        }
        this.sources.clear();
        this.filter?.disconnect();
        this.filter = null;
        this.signal = null;
        this.nextStartTime = 0;
    }
}
