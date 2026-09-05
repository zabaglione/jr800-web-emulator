// SPDX-License-Identifier: MIT

import assert from "node:assert/strict";
import {readFile} from "node:fs/promises";

const [indexPath, appPath, stylesPath] = process.argv.slice(2);
if (!indexPath || !appPath || !stylesPath) {
    throw new Error(
        "Usage: node web_ui_contract_test.mjs <index.html> <app.mjs> <styles.css>",
    );
}

const [html, app, styles] = await Promise.all([
    readFile(indexPath, "utf8"),
    readFile(appPath, "utf8"),
    readFile(stylesPath, "utf8"),
]);

const htmlIds = [...html.matchAll(/\bid="([^"]+)"/g)].map((match) => match[1]);
assert.equal(
    new Set(htmlIds).size,
    htmlIds.length,
    "index.html contains duplicate element ids",
);
assert.ok(
    html.indexOf('id="lcd-panel-card"') < html.indexOf('id="load-heading"'),
    "LCD panel must be the first main-content panel",
);
assert.ok(
    html.indexOf('id="lcd-panel-card"')
        < html.indexOf('id="virtual-keyboard-card"')
        && html.indexOf('id="virtual-keyboard-card"')
            < html.indexOf('id="load-heading"'),
    "Virtual keyboard must sit directly below the LCD and before loading tools",
);
assert.match(
    html,
    /<section id="machine-console" class="machine-console" data-layout="device"[\s\S]*id="lcd-panel-card"[\s\S]*id="virtual-keyboard-card"[\s\S]*<\/section>/,
    "LCD and virtual keyboard must share one machine console",
);
for (const id of ["layout-workbench", "layout-device"]) {
    assert.ok(htmlIds.includes(id), `Layout control is missing: ${id}`);
}
assert.match(
    html,
    /id="layout-workbench"[^>]*aria-pressed="false"[\s\S]*id="layout-device"[^>]*aria-pressed="true"/,
    "Device layout must be the initial selection",
);
assert.match(
    html,
    /id="power-off"[^>]*class="virtual-power-key"[^>]*disabled[^>]*title="Suspend emulation and retain memory"[\s\S]*id="power-on"[^>]*class="virtual-power-key virtual-power-key-on"[^>]*disabled/,
    "Power controls must describe retained-session suspension and resumption",
);
for (const [left, right] of [
    ['class="virtual-power-key"', 'class="virtual-power-key virtual-power-key-on"'],
    ['class="virtual-power-key virtual-power-key-on"', 'class="virtual-speaker-gap"'],
    ['class="virtual-speaker-gap"', 'data-jr800-key="menu"'],
    ['data-jr800-key="menu"', 'data-legend-key="pf-6"'],
    ['data-legend-key="pf-10"', 'data-legend-key="pf-1"'],
    ['data-legend-key="pf-5"', 'data-jr800-key="main-1"'],
]) {
    assert.ok(
        html.indexOf(left) < html.indexOf(right),
        `Top-control order is incorrect: ${left} before ${right}`,
    );
}
assert.match(
    html,
    /class="virtual-speaker-gap" aria-hidden="true"><\/div>/,
    "The omitted speaker region must remain an unpainted layout gap",
);
assert.ok(
    html.indexOf('id="force-power-off"')
        < html.indexOf('class="virtual-main-keyboard"')
        && html.indexOf('class="virtual-main-keyboard"')
            < html.indexOf('class="virtual-side-keyboard"'),
    "The RESET pin must occupy the space above the main keyboard",
);
assert.match(
    html,
    /id="force-power-off"[^>]*class="virtual-reset-pin"[^>]*disabled[^>]*aria-label="RESET pin; force power off"/,
    "The RESET pin must start disabled and expose its destructive action",
);
assert.match(
    html,
    /data-jr800-key="keypad-divide"[^>]*><span>\/<\/span>/,
    "The keypad divide key must retain the documented slash legend",
);
assert.match(
    html,
    /data-jr800-key="keypad-multiply"[^>]*><span>\*<\/span>/,
    "The keypad multiply key must retain the documented asterisk legend",
);
assert.doesNotMatch(html, /[÷×✖]/, "Keypad legends must not be substituted");
assert.doesNotMatch(
    html,
    /<img\b[^>]*(?:logo|national)|class="[^"]*logo/i,
    "Machine console must not embed a manufacturer logo",
);
assert.doesNotMatch(
    html,
    /id="lcd-panel-card"[^>]*\bhidden\b/,
    "LCD panel must remain visible before a target is loaded",
);
assert.doesNotMatch(
    app,
    /elements\["lcd-panel-card"\]\.hidden/,
    "LCD visibility must not depend on the selected target kind",
);
assert.match(
    html,
    /<details id="debugger-menu" class="debugger-menu">[\s\S]*<summary>Developer debugger<\/summary>[\s\S]*id="sdk-load-heading"[\s\S]*id="application-file"[\s\S]*aria-label="Execution controls"[\s\S]*aria-label="Debugger workspace"[\s\S]*<\/details>/,
    "Developer debugger must contain the SDK loader, controls, and workspace",
);
assert.ok(
    html.indexOf('id="application-file"') > html.indexOf('id="debugger-menu"'),
    "The SDK application loader must not appear in the user-facing target panel",
);
assert.doesNotMatch(
    html,
    /<details id="debugger-menu"[^>]*\bopen\b/,
    "Developer debugger must be collapsed by default",
);

const elementTable = app.match(
    /const elements = Object\.fromEntries\([\s\S]*?\[([\s\S]*?)\]\.map\(/,
);
assert.ok(elementTable, "app.mjs element table was not found");
const appIds = [...elementTable[1].matchAll(/"([a-z0-9-]+)"/g)]
    .map((match) => match[1]);
assert.ok(appIds.length > 0, "app.mjs element table is empty");

const missingIds = appIds.filter((id) => !htmlIds.includes(id));
assert.deepEqual(
    missingIds,
    [],
    `app.mjs references missing HTML ids: ${missingIds.join(", ")}`,
);

for (const id of [
    "language-toggle",
    "sound-toggle",
    "application-file",
    "load",
    "jr8rom-file",
    "raw-rom-warning",
    "hardware-program-file",
    "load-program",
    "boot-basic",
    "load-rom",
    "resume-machine",
    "pause-basic",
    "power-on",
    "power-off",
    "reset-sp-enabled",
    "reset-sp-value",
    "reset-x-enabled",
    "reset-x-value",
    "reset-a-enabled",
    "reset-a-value",
    "reset-b-enabled",
    "reset-b-value",
    "reset-cc-enabled",
    "reset-cc-value",
    "reset-cc-known-mask",
    "internal-ram-enabled",
    "internal-ram-value",
    "lcd-panel-card",
    "lcd-panel",
    "lcd-summary",
    "lcd-indicator-summary",
    "machine-console",
    "layout-workbench",
    "layout-device",
    "virtual-keyboard-card",
    "virtual-keyboard-summary",
    "suspended-cycle-limit",
    "calendar-oscillator-ticks",
    "advance-calendar",
    "adjust-calendar-seconds",
    "calendar-cpu-cycle-ratio-enabled",
    "calendar-alarm-terminal",
    "keyboard-address",
    "keyboard-value",
    "keyboard-known",
    "set-keyboard-response",
    "keyboard-summary",
    "breakpoint-condition",
    "watchpoint-mode",
    "watch-expression",
    "add-expression-watch",
    "expression-watch-body",
    "watch-symbol",
    "add-symbol-watch",
    "symbol-watch-body",
    "step-over",
    "step-out",
    "run-to",
    "run-to-address",
    "run-to-source",
    "run-to-source-location",
    "run-to-symbol",
    "run-to-symbol-name",
    "trace-first-address",
    "trace-last-address",
    "trace-kind",
    "apply-trace-filter",
]) {
    assert.ok(appIds.includes(id), `Required browser control is missing: ${id}`);
}

const lcdIndicators = [
    ...html.matchAll(/data-lcd-indicator="([^"]+)"/g),
].map((match) => match[1]);
assert.deepEqual(lcdIndicators, [
    "page-1",
    "page-2",
    "page-3",
    "page-4",
    "page-5",
    "page-6",
    "page-7",
    "page-8",
    "capital-lock",
    "graphics-input",
    "kana-input",
    "insert-mode",
    "control-mode",
    "radian-mode",
    "degree-mode",
    "battery-warning",
]);

const virtualKeys = [
    ...html.matchAll(/data-jr800-key="([^"]+)"/g),
].map((match) => match[1]);
assert.deepEqual(virtualKeys, [
    "menu",
    "pf-6",
    "pf-7",
    "pf-8",
    "pf-9",
    "pf-10",
    "pf-1",
    "pf-2",
    "pf-3",
    "pf-4",
    "pf-5",
    "main-1",
    "main-2",
    "main-3",
    "main-4",
    "main-5",
    "main-6",
    "main-7",
    "main-8",
    "main-9",
    "main-0",
    "main-caret",
    "break",
    "letter-q",
    "letter-w",
    "letter-e",
    "letter-r",
    "letter-t",
    "letter-y",
    "letter-u",
    "letter-i",
    "letter-o",
    "letter-p",
    "return",
    "control",
    "letter-a",
    "letter-s",
    "letter-d",
    "letter-f",
    "letter-g",
    "letter-h",
    "letter-j",
    "letter-k",
    "letter-l",
    "semicolon",
    "colon",
    "shift",
    "letter-z",
    "letter-x",
    "letter-c",
    "letter-v",
    "letter-b",
    "letter-n",
    "letter-m",
    "comma",
    "period",
    "space",
    "keypad-insert-rub",
    "keypad-vertical-arrows",
    "keypad-horizontal-arrows",
    "home-cls",
    "keypad-7",
    "keypad-8",
    "keypad-9",
    "keypad-divide",
    "keypad-4",
    "keypad-5",
    "keypad-6",
    "keypad-multiply",
    "keypad-1",
    "keypad-2",
    "keypad-3",
    "keypad-subtract",
    "keypad-0",
    "keypad-decimal",
    "keypad-equal",
    "keypad-add",
]);
assert.equal(new Set(virtualKeys).size, 77);
const legendKeys = [
    ...html.matchAll(/data-legend-key="([^"]+)"/g),
].map((match) => match[1]);
assert.equal(legendKeys.length, 57);
assert.equal(new Set(legendKeys).size, legendKeys.length);
assert.doesNotMatch(
    html,
    />\s*(?:LCD matrix|Virtual keyboard)\s*</,
    "Self-evident LCD and keyboard headings must not consume console space",
);
assert.doesNotMatch(
    html,
    /JR8ROM required|mapped keys|<small>|>Latch</,
    "Keyboard diagnostics and static alternate legends must stay out of view",
);
for (const className of [
    "virtual-key-menu virtual-key-light",
    "virtual-key virtual-key-light virtual-key-wide",
    "virtual-key-modifier virtual-key-light virtual-key-wide",
]) {
    assert.ok(
        html.includes(className),
        `Light control-key class is missing: ${className}`,
    );
}
assert.doesNotMatch(
    html,
    /virtual-key-pending|Pending keyboard-matrix evidence/,
    "Every physical key position must use the structured keyboard transport",
);
assert.match(
    app,
    /function setMachineLayout\(layout\)[\s\S]*dataset\.layout = layout;[\s\S]*layout === "workbench"[\s\S]*layout === "device"/,
    "Layout switch state is not implemented",
);
assert.match(
    app,
    /elements\["layout-workbench"\]\.addEventListener\("click"[\s\S]*setMachineLayout\("workbench"\)[\s\S]*elements\["layout-device"\]\.addEventListener\("click"[\s\S]*setMachineLayout\("device"\)/,
    "Both layout controls must update the machine layout",
);
assert.match(
    app,
    /function renderVirtualKeyboardLegends\(\)[\s\S]*virtualKeyboardState\.isPressed\("shift"\)[\s\S]*virtualKeyboardState\.isPressed\("control"\)[\s\S]*virtualKeyboardLegend\(key, modifiers\)[\s\S]*target\.textContent = label/,
    "Modifier state must drive the visible keyboard legends",
);
assert.match(
    styles,
    /\.virtual-key:disabled\s*\{\s*opacity:\s*1;\s*\}/,
    "Disabled ordinary keys must retain the same face color as modeled keys",
);
assert.doesNotMatch(
    styles,
    /\.virtual-key-pending/,
    "The obsolete partial-keyboard style must be removed",
);
assert.match(
    app,
    /const modeledKeyboardKeys = Object\.keys\(Jr800KeyboardKey\);[\s\S]*virtualKeyboardButtonElements\.length !== modeledKeyboardKeys\.length[\s\S]*modeledKeyboardKeys\.some\(\(key\) => !virtualKeyboardButtons\.has\(key\)\)/,
    "The DOM keyboard must stay in exact agreement with the ABI key set",
);
assert.match(
    app,
    /function hostKeyboardTargetIsInteractive\(target\)[\s\S]*input, textarea, select, button, a, summary, \[contenteditable\]/,
    "Host keyboard capture must exclude interactive page controls",
);
assert.match(
    app,
    /document\.addEventListener\("keydown", \(event\) => \{[\s\S]*event\.defaultPrevented[\s\S]*!virtualKeyboardAvailable\(\)[\s\S]*hostKeyboardTargetIsInteractive\(event\.target\)[\s\S]*jr800KeyForHostCode\(event\.code\)[\s\S]*event\.preventDefault\(\);[\s\S]*virtualKeyboardState\.press\([\s\S]*`host:\$\{event\.code\}`/,
    "Recognized host keydown must use the shared source aggregator",
);
assert.match(
    app,
    /document\.addEventListener\("keyup", \(event\) => \{[\s\S]*jr800KeyForHostCode\(event\.code\)[\s\S]*virtualKeyboardState\.release\(`host:\$\{event\.code\}`\)[\s\S]*transition !== null[\s\S]*sendVirtualKeyboardTransition\(transition\)/,
    "Host keyup must release the matching shared source",
);
assert.match(
    app,
    /elements\["force-power-off"\]\.disabled = keyboardDisabled;/,
    "RESET must be available only for a loaded JR8ROM session",
);
assert.match(
    app,
    /elements\["force-power-off"\]\.addEventListener\("click"[\s\S]*window\.confirm\([\s\S]*client\.request\("power-off"\)[\s\S]*loaded = false;[\s\S]*renderPoweredOff\(\);[\s\S]*setStatus\("Power off", "idle"\)/,
    "RESET must require confirmation and discard the active session",
);
assert.match(
    styles,
    /\.virtual-power-cluster\s*\{[\s\S]*?display:\s*grid;[\s\S]*?\}/,
    "Power controls must be visible in Workbench layout",
);
assert.match(
    styles,
    /\.virtual-system-strip\s*\{[\s\S]*?grid-template-columns:\s*repeat\(5, minmax\(0, 1fr\)\);[\s\S]*?\}/,
    "The system strip must share the five-column PF alignment",
);
assert.match(
    styles,
    /\.virtual-power-cluster\s*\{[\s\S]*?grid-column:\s*span 2;[\s\S]*?\}[\s\S]*\.virtual-speaker-gap\s*\{[\s\S]*?grid-column:\s*span 2;[\s\S]*?\}[\s\S]*\.virtual-key-menu\s*\{[\s\S]*?grid-column:\s*5;/,
    "OFF, ON, the speaker gap, and MENU must align to the PF columns",
);
assert.match(
    styles,
    /\.virtual-power-key\s*\{[\s\S]*?min-height:\s*2\.7rem;[\s\S]*?\}[\s\S]*\.virtual-key-menu\s*\{[\s\S]*?min-height:\s*2\.7rem;[\s\S]*?\}[\s\S]*\.virtual-function-strip \.virtual-key\s*\{\s*min-height:\s*2\.7rem;/,
    "Power, MENU, and PF keys must share one control height",
);
assert.match(
    styles,
    /\.virtual-keyboard-layout\s*\{[\s\S]*?align-items:\s*stretch;[\s\S]*?\}[\s\S]*\.virtual-main-keyboard-column\s*\{[\s\S]*?display:\s*flex;[\s\S]*?flex-direction:\s*column;[\s\S]*?\}[\s\S]*\.virtual-reset-row\s*\{[\s\S]*?flex:\s*1 1 auto;/,
    "The main keyboard must bottom-align with the numeric keypad",
);
assert.match(
    styles,
    /\.virtual-key-light\s*\{[\s\S]*?color:\s*var\(--key-face\);[\s\S]*?background:[^;]*#fff/,
    "CTRL, BREAK, and MENU must use the light control-key face",
);
assert.match(
    styles,
    /\.machine-console\[data-layout="device"\][\s\S]*grid-template-columns:[^;]+;[\s\S]*\.machine-console\[data-layout="device"\] \.lcd-panel-card\s*\{[\s\S]*grid-column:\s*1;[\s\S]*grid-row:\s*2;[\s\S]*\.machine-console\[data-layout="device"\] \.virtual-top-controls\s*\{[\s\S]*grid-column:\s*2;[\s\S]*grid-row:\s*2;[\s\S]*\.machine-console\[data-layout="device"\] \.virtual-keyboard-layout\s*\{[\s\S]*grid-column:\s*1 \/ -1;[\s\S]*grid-row:\s*3;/,
    "Device layout must place LCD and controls above the full keyboard",
);
assert.match(
    app,
    /request\("set-keyboard-key-state", fields\)/,
    "Virtual keyboard transitions are not connected to the Worker",
);
assert.match(
    app,
    /window\.addEventListener\("blur", \(\) => releaseAllVirtualKeys\(\)\)/,
    "Virtual keyboard does not release held keys on focus loss",
);
assert.match(
    app,
    /document\.visibilityState === "hidden"[\s\S]*releaseAllVirtualKeys\(\)/,
    "Virtual keyboard does not release held keys when hidden",
);

assert.match(
    html,
    /id="application-file"[^>]*accept="\.j8a,application\/octet-stream"/,
    "J8A file selection contract is missing",
);
assert.match(
    html,
    /id="debug-file"[^>]*accept="\.j8d,application\/octet-stream"/,
    "J8D file selection contract is missing",
);
assert.match(
    app,
    /"application-file", "debug-file",/,
    "The debug-file control must be included in the application element map",
);
assert.match(
    html,
    /id="jr8rom-file"[^>]*accept="\.j8r,\.rom,application\/octet-stream"/,
    "J8R and raw ROM file selection contract is missing",
);
assert.match(
    html,
    /id="raw-rom-warning" hidden>[^<]*WAV[^<]*\.j8r[^<]*<\/span>/,
    "Raw ROM warning text is missing",
);
const rawRomWarning = "&#12489;&#12461;&#12517;&#12513;&#12531;&#12488;&#12395;&#24467;&#12387;&#12390;&#23455;&#27231;&#12391;&#37682;&#38899;&#12375;&#12383;WAV&#12501;&#12449;&#12452;&#12523;&#12434; .j8r &#12395;&#22793;&#25563;&#12375;&#12390;&#12367;&#12384;&#12373;&#12356;&#12290;&#20966;&#29702;&#12399;&#32154;&#34892;&#12375;&#12414;&#12377;&#12290;";
assert.ok(
    html.includes(`id="raw-rom-warning" hidden>${rawRomWarning}</span>`),
    "Raw ROM warning text must match the required Japanese message",
);
assert.equal(
    [...app.matchAll(/window\.confirm\(\s*translate\(/g)].length,
    2,
    "Both confirmations must use the shared localizer",
);
assert.match(
    app,
    /function rawRomLoadApproved\(\)[\s\S]*endsWith\("\.rom"\)[\s\S]*window\.confirm\(translate\([\s\S]*Follow the documentation/,
    "Raw ROM loading must use the localized confirmation text",
);
assert.match(
    app,
    /preferredWebUiLanguage\(navigator\.languages \?\? navigator\.language\)/,
    "The initial interface language must follow the browser language",
);
assert.match(
    app,
    /elements\["language-toggle"\]\.addEventListener\("click", switchLanguage\)/,
    "The interface language toggle is not connected",
);
assert.match(
    app,
    /client\.on\("audio-transitions", \(message\) => audioOutput\.append\(message\)\)/,
    "Port 1 audio transitions are not connected to the Web Audio output",
);
assert.match(
    app,
    /elements\["sound-toggle"\]\.addEventListener\("click"[\s\S]*set-audio-enabled/,
    "The sound toggle is not connected to the Worker",
);
assert.match(
    html,
    /window\.location\.protocol === "file:"[\s\S]*file-protocol-ja[\s\S]*status\.dataset\.tone = "error"/,
    "Direct local-file opening must show actionable guidance",
);
assert.match(
    app,
    /elements\["load-rom"\]\.addEventListener\("click"[\s\S]*if \(!rawRomLoadApproved\(\)\)[\s\S]*return;[\s\S]*elements\["boot-basic"\]\.addEventListener\("click"[\s\S]*if \(!rawRomLoadApproved\(\)\)[\s\S]*return;/,
    "Both raw ROM actions must stop before loading when confirmation is cancelled",
);
assert.match(
    app,
    /const command = format === "jr8rom" \? "load-jr800" : "load-jr800-raw";/,
    "ROM file extensions must select an explicit Worker loading command",
);
assert.match(
    html,
    /id="hardware-program-file"[^>]*accept="\.wav,\.j8a,audio\/wav,application\/octet-stream"[^>]*disabled/,
    "JR-800 WAV and JR8APP RAM-program selection contract is missing",
);
assert.match(
    app,
    /elements\["load-program"\]\.addEventListener\("click"[\s\S]*"load-native-program-wav"[\s\S]*"load-program"[\s\S]*client\.request\(command[\s\S]*startBasicRun\("RAM program running"\)/,
    "JR-800 WAV and JR8APP loading must continue from the recorded entry",
);
assert.match(
    app,
    /nativeProgramWavIssueMessages[\s\S]*"checksum-mismatch"[\s\S]*localizedErrorMessage\(error\)/,
    "WAV conversion failures must be converted to visible user messages",
);
assert.match(
    app,
    /WEB_KEY_MINIMUM_HOLD_CYCLES = 49_152[\s\S]*let keyboardTransitionTail = Promise\.resolve\(\)[\s\S]*minimumHoldCycles: WEB_KEY_MINIMUM_HOLD_CYCLES[\s\S]*keyboardTransitionTail = keyboardTransitionTail/,
    "Web keyboard input must be held and serialized across BASIC scan intervals",
);
assert.doesNotMatch(
    html,
    /\.(?:jr8app|jr8dbg|jr8deb|jr8rom|jr8link)\b/i,
    "Obsolete project filename extension remains in the browser host",
);
assert.match(
    html,
    /id="boot-basic"[^>]*>Boot BASIC experiment<\/button>[\s\S]*id="load-rom"[^>]*>Load only<\/button>[\s\S]*id="resume-machine"[^>]*disabled>Resume emulation<\/button>[\s\S]*id="pause-basic"[^>]*disabled>Pause<\/button>/,
    "The visible BASIC boot and run controls are incomplete",
);
assert.match(
    app,
    /function applyBasicBootExperimentControls\(\)[\s\S]*jr800BasicBootExperimentConfiguration\(\)[\s\S]*return hardwareConfiguration\(\);/,
    "The BASIC experiment must pass through the visible hardware controls",
);
assert.match(
    app,
    /elements\["boot-basic"\]\.addEventListener\("click"[\s\S]*applyBasicBootExperimentControls\(\)[\s\S]*loadJr800Machine\(romFile, configuration\)[\s\S]*startBasicRun\(\)/,
    "Boot BASIC must apply the explicit profile, load the ROM, and start execution",
);
assert.match(
    app,
    /client\.on\("stopped"[\s\S]*basicRunCanContinue\(message\.stop\)[\s\S]*startBasicRun\(\)[\s\S]*basicRunContinuous = false;/,
    "BASIC execution must continue only across bounded instruction slices",
);
assert.doesNotMatch(
    html,
    /logical-rom-file|accept="\.bin,\.rom/,
    "Obsolete raw-ROM browser input is still present",
);

for (const kind of [
    "all", "instructionFetch", "dataRead", "dataWrite", "data",
]) {
    assert.match(
        html,
        new RegExp(`<option value="${kind}"`),
        `Access trace kind is missing: ${kind}`,
    );
}

for (const mode of ["read", "write", "access"]) {
    assert.match(
        html,
        new RegExp(`<option value="${mode}"`),
        `Memory watchpoint mode is missing: ${mode}`,
    );
}

assert.match(
    html,
    /id="breakpoint-condition"[^>]*placeholder="A == \$42 && mem8\[\$0010\] != 0"/,
    "Conditional breakpoint example is missing",
);
assert.match(
    html,
    /id="watch-expression"[^>]*placeholder="A or symbol\(&quot;loop&quot;\)"/,
    "Expression-watch example is missing",
);
assert.match(
    app,
    /request\("set-expression-watch",[\s\S]*watchId: nextExpressionWatchId,[\s\S]*expression,/,
    "Expression-watch add action is missing",
);
assert.match(
    app,
    /request\("clear-expression-watch",[\s\S]*watchId,[\s\S]*view: viewOptions\(\)/,
    "Expression-watch remove action is missing",
);
assert.match(
    html,
    /id="watch-symbol"[^>]*placeholder="main"/,
    "Symbol-watch example is missing",
);
assert.match(
    app,
    /request\("set-symbol-watch",[\s\S]*watchId: nextSymbolWatchId,[\s\S]*name,/,
    "Symbol-watch add action is missing",
);
assert.match(
    app,
    /request\("clear-symbol-watch",[\s\S]*watchId,[\s\S]*view: viewOptions\(\)/,
    "Symbol-watch remove action is missing",
);
assert.match(
    app,
    /elements\["reset-x-enabled"\]\.checked[\s\S]*configuration\.resetIndexRegister/,
    "Reset-state experiment input is not forwarded",
);
assert.match(
    app,
    /elements\["internal-ram-enabled"\]\.checked[\s\S]*configuration\.internalRamInitialValue/,
    "Internal-RAM experiment input is not forwarded",
);
assert.match(
    html,
    /id="calendar-cpu-cycle-ratio-enabled"[^>]*type="checkbox"[\s\S]*Use E-030 nominal 1\.2288 MHz CPU E clock/,
    "Named nominal calendar CPU clock is not explicit in the form",
);
const calendarCpuRatioInput = html.match(
    /<input id="calendar-cpu-cycle-ratio-enabled"[^>]*>/,
);
assert.ok(calendarCpuRatioInput, "Calendar CPU-cycle ratio checkbox is missing");
assert.doesNotMatch(
    calendarCpuRatioInput[0],
    /\bchecked\b/,
    "Calendar CPU-cycle ratio must default to off",
);
assert.match(
    app,
    /elements\["calendar-cpu-cycle-ratio-enabled"\]\.checked[\s\S]*configuration\.calendarCpuCycleRatio\s*=\s*"e030-nominal-1\.2288mhz"/,
    "Named calendar CPU-cycle ratio is not forwarded",
);
assert.match(
    app,
    /\["calendar-cpu-cycle-ratio-enabled", calendarEnabled\]/,
    "Calendar CPU-cycle ratio is not gated by calendar attachment",
);
assert.match(
    app,
    /if \(!calendarEnabled\) \{\s*elements\["calendar-cpu-cycle-ratio-enabled"\]\.checked = false;/,
    "Detaching the calendar does not clear its CPU-cycle ratio",
);
assert.match(
    app,
    /elements\["set-keyboard-response"\]\.disabled = !loaded\s*\|\| machineKind !== "jr800"/,
    "Raw keyboard response updates are not enabled during JR-800 runs",
);
assert.match(
    app,
    /const calendarOperationDisabled = !loaded \|\| running\s*\|\| machineKind !== "jr800" \|\| !calendarAttached;[\s\S]*elements\["advance-calendar"\]\.disabled = calendarOperationDisabled;[\s\S]*elements\["adjust-calendar-seconds"\]\.disabled = calendarOperationDisabled;/,
    "Calendar operations are not restricted to an idle attached experiment",
);
assert.match(
    app,
    /calendarAttached = configuration\.calendarAddressSource !== undefined;/,
    "Loaded calendar configuration is not retained for control gating",
);
assert.match(
    app,
    /request\("advance-calendar-oscillator", \{\s*ticks,\s*view: viewOptions\(\),/,
    "Explicit calendar oscillator action is not connected to the Worker",
);
assert.match(
    html,
    /id="calendar-oscillator-ticks"[^>]*value="32768"[^>]*disabled/,
    "Calendar control does not expose one explicit 32.768 kHz second",
);
assert.match(
    app,
    /request\("adjust-calendar-seconds", \{\s*view: viewOptions\(\),\s*\}\);[\s\S]*render\(snapshot\);/,
    "Qualified calendar adjustment is not connected to the Worker",
);
assert.match(
    html,
    /id="adjust-calendar-seconds"[^>]*aria-describedby="calendar-adjust-boundary"[^>]*disabled>Adjust seconds once<\/button>/,
    "Calendar adjustment control is not explicit and default-disabled",
);
assert.match(
    html,
    /Calendar ADJ is one software-qualified RP5C01 action\. No physical input level or pulse is modeled\./,
    "Calendar adjustment control does not preserve its hardware boundary",
);
assert.match(
    app,
    /update\.appliedDuringRun[\s\S]*update\.totalInstructionsExecuted/,
    "Live raw keyboard update timing is not presented",
);
assert.match(
    app,
    /function renderKeyboardActivity\(activity\)[\s\S]*activity\.readAttempts[\s\S]*activity\.distinctAddresses/,
    "Privacy-bounded keyboard activity is not rendered",
);
assert.match(
    app,
    /renderKeyboardActivity\(snapshot\.keyboardActivity\)/,
    "Keyboard activity renderer is not connected to snapshots",
);
assert.match(
    app,
    /elements\["calendar-alarm-terminal"\]\.textContent\s*=\s*translate\(state\.calendarAlarmTerminal\);/,
    "Calendar ALARM diagnostic is not connected to snapshots",
);
assert.match(
    app,
    /elements\["port2-timer-output"\]\.textContent\s*=\s*translate\(\s*state\.port2TimerOutput,\s*\);/,
    "Port 2 timer-output diagnostic is not connected to snapshots",
);
assert.match(
    html,
    /id="port2-timer-output"[^>]*aria-live="polite"/,
    "Port 2 timer-output diagnostic field is missing",
);
assert.match(
    app,
    /state\.lcdSubstitutedDataReadCount === null[\s\S]*String\(state\.lcdSubstitutedDataReadCount\)/,
    "LCD substituted-read diagnostic is not connected to snapshots",
);
assert.match(
    app,
    /function renderLcdIndicators\(indicators\)[\s\S]*lcdIndicatorView\(indicators, translate\)[\s\S]*entry\.description[\s\S]*entry\.valueText/,
    "LCD indicator raw-value presentation is missing",
);
assert.match(
    app,
    /renderLcdIndicators\(snapshot\.lcdIndicators\)/,
    "LCD indicator presentation is not connected to snapshots",
);
assert.match(
    html,
    /Raw indicator values never imply a lit or unlit physical segment\./,
    "LCD indicator presentation overstates unresolved drive behavior",
);
assert.match(
    html,
    /Indicator RAM unavailable; battery telemetry omitted/,
    "Physical-only battery telemetry boundary is not visible",
);
assert.match(
    html,
    /id="lcd-substituted-read-count"[^>]*aria-live="polite"/,
    "LCD substituted-read diagnostic field is missing",
);
assert.match(
    html,
    /Calendar ALARM is the internal RP5C01 pull-down request; Port 2 timer out is a CPU-internal logical state\. Physical JR-800 wiring is unresolved\./,
    "Hardware diagnostics do not preserve their physical boundary",
);
assert.match(
    html,
    /LCD substituted reads count software replacements under the explicit LCD experiment and do not validate physical decoding\./,
    "LCD substituted-read diagnostic overstates the hardware evidence",
);
assert.match(
    app,
    /const symbolName = elements\["run-to-symbol-name"\]\.value;[\s\S]*startRun\(\s*"run-to-symbol"/,
    "Run-to-symbol browser action is missing",
);

assert.match(
    html,
    /The JR8ROM and RAM program stay in local Worker memory\./,
    "The owner-local file boundary is not visible in the UI",
);
assert.match(
    html,
    /Enabling an item is an experiment input, not a hardware claim\./,
    "The experimental configuration boundary is not visible in the UI",
);
assert.match(
    html,
    /The matrix composition and indicator RAM locations are provisional\./,
    "The provisional LCD composition boundary is not accessible in the UI",
);
