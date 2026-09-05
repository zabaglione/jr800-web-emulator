// SPDX-License-Identifier: MIT

// E-030: nominal CPU E clock, after the external oscillator's divide by four.
export const JR800_CPU_CLOCK_HZ = 1_228_800;
export const RUN_TURN_MS = 8;
const MAX_BACKLOG_MS = 100;
const CYCLES_PER_MS = JR800_CPU_CLOCK_HZ / 1000;

export class RunPacer {
    constructor(cycle, now) {
        this.originCycle = cycle;
        this.originTime = now;
    }

    cycleBudget(cycle, now) {
        // A throttled/background tab must not accumulate unbounded catch-up work.
        if (now - this.originTime - (cycle - this.originCycle) / CYCLES_PER_MS
            > MAX_BACKLOG_MS) {
            this.originCycle = cycle;
            this.originTime = now;
        }
        return Math.max(0, Math.floor(
            (now - this.originTime + RUN_TURN_MS) * CYCLES_PER_MS
                - (cycle - this.originCycle),
        ));
    }

    delay(cycle, now) {
        return Math.max(0,
            (cycle - this.originCycle) / CYCLES_PER_MS - (now - this.originTime),
        );
    }
}
