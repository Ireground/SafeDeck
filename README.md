# SafeDeck

A tiny safety plugin (VST3) for the **last slot of the master bus**. It protects your speakers and ears
from blasts caused by any plugin before it, and doesn't touch the sound otherwise.

## Install
1. Open the **Actions** tab, click the latest green run, download **SafeDeck-VST3-Windows**.
2. Unzip it and copy `SafeDeck.vst3` to `C:\Program Files\Common Files\VST3\` (or your Claude VSTs folder).
3. REAPER: Options > Preferences > Plug-ins > VST > *Re-scan*. Put **SafeDeck** last on the master track.

## What it does
* **Guard (always on).** NaN / infinite values, or anything coming in louder than the **Panic Level**
  (default +18 dB), are replaced by silence. The audio just before is faded out, so there is no click.
  After 1 second of clean signal the output **fades back in** over 1 second.
* **Limiter (on/off).** Lookahead brickwall limiter. **Ceiling** (default -0.3 dB), **Release**, and
  **True peak** (also catches the peaks between samples that appear after conversion to analog).
* **Untouched sound.** When the limiter isn't reducing anything, the output is bit-for-bit the input.
  This is checked automatically on every build (null test).
* **Status light, meters (In / gain reduction / Out) and an event log** with the project time of every mute.

The lookahead adds about 2 ms of latency, which REAPER compensates automatically.
It stays the same with the limiter on or off, so switching never shifts your timing.

## Before mastering with Master Plan
SafeDeck is meant for writing and sound design. When you add Master Plan, either remove SafeDeck,
turn its limiter off, or put SafeDeck after Master Plan with the ceiling slightly above Master Plan's.

## Tests on every build
* Engine tests: null test (limiter off and on), ceiling never exceeded, true peak under 0 dBTP,
  NaN/inf and +40 dB spikes blocked and logged, clean fade back, at 44.1 / 48 / 96 / 192 kHz.
* pluginval at strictness level 10.
A red X on a run means one of them failed.
