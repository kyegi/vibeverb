# Reverb Effect: How It Works

## Overview

Reverb (short for reverberation) is the persistence of sound after the original source has stopped. It occurs naturally when sound waves reflect off surfaces in an enclosed space — walls, ceilings, floors, and objects — and reach the listener as a dense collection of echoes that gradually decay.

In audio processing, a reverb effect simulates this phenomenon digitally, making a dry (unprocessed) signal sound as though it was recorded in a physical space.

---

## The Physics of Natural Reverb

When a sound is produced in a room, three distinct components reach the listener:

1. **Direct Sound** — the original signal traveling directly from source to listener, unaffected by reflections.
2. **Early Reflections** — the first discrete echoes arriving within roughly 5–80 ms after the direct sound. These bounce off nearby surfaces and give the listener strong perceptual cues about room size and shape.
3. **Late Reverberation (Reverb Tail)** — the dense, diffuse mass of reflections that follow. These overlap so heavily that individual echoes become indistinguishable. The tail gradually fades as energy is absorbed by surfaces and air.

```
Amplitude
    |
    |  Direct     Early            Late Reverb Tail
    |  Sound    Reflections         (decaying)
    |   |       | | |  |  |   |  | |  | |  | |  |
    |   |       | | |  |  |   |  | |  | |  | |  |
    +---|-------|------|------|---|---|---|---|---|---> Time
        0ms    10ms  30ms  80ms                    2s+
```

---

## Key Parameters

| Parameter | Description |
|---|---|
| **Pre-delay** | Time gap (ms) between the dry signal and the onset of early reflections. Controls perceived distance from the source. |
| **Decay Time (RT60)** | How long it takes for the reverb tail to decay by 60 dB. Longer values simulate larger or more reflective spaces. |
| **Room Size** | Models the physical dimensions of the simulated space. Affects reflection spacing and tail density. |
| **Damping** | High-frequency absorption over time. Hard surfaces (concrete) damp less; soft surfaces (carpet, curtains) damp more. |
| **Diffusion** | Controls how quickly reflections build up in density. High diffusion creates a smoother tail. |
| **Dry/Wet Mix** | Balance between the unprocessed (dry) signal and the reverb (wet) signal. |
| **Early Reflection Level** | Volume of the early reflections relative to the tail. |

---

## Types of Reverb Algorithms

### 1. Plate Reverb

Originally a physical device: a large metal plate suspended by springs with transducers attached. Vibrations travel across the plate and create dense, smooth reflections.

- **Sound character:** bright, dense, musical
- **Digital simulation:** modeled using all-pass filters and delay networks
- **Common uses:** vocals, drums, strings

### 2. Spring Reverb

Uses metal springs to create reflections. Signals travel through a coiled spring, bounce back, and produce a distinctive "boing" character.

- **Sound character:** dark, wobbly, lo-fi
- **Common uses:** guitar amplifiers, vintage keyboards, surf music

### 3. Room / Chamber Reverb

Simulates a specific acoustic space — a small room, a stone chamber, a concert hall. Early reflections are distinct and shaped by geometry.

- **Sound character:** varies widely by room type
- **Common uses:** drums, orchestral instruments, ambience

### 4. Hall Reverb

Models large concert halls or cathedrals. Long pre-delay, long decay, wide stereo spread.

- **Sound character:** lush, expansive, smooth
- **Common uses:** orchestral, pads, lead vocals

### 5. Convolution Reverb

Uses an **Impulse Response (IR)** — a recording of a real space's acoustic response — to mathematically convolve with the input signal. Produces highly realistic results.

- **How it works:** the IR captures how a space responds to a transient (e.g. a gunshot or sine sweep), and the convolution operation applies that response to any input
- **Sound character:** highly realistic, matches the recorded space exactly
- **Drawback:** CPU-intensive, less flexible to tweak in real time
- **Common uses:** film scoring, realistic acoustic spaces, hardware emulation

### 6. Algorithmic Reverb

Uses networks of delay lines, all-pass filters, comb filters, and feedback loops to synthesize reverb artificially — no IR required.

- **More flexible** than convolution: parameters can be modulated in real time
- **Common architectures:** Schroeder reverberator, Moorer reverberator, FDN (Feedback Delay Network)

---

## Core DSP Building Blocks

### Comb Filter

Adds a delayed copy of the signal to itself, creating peaks and notches in the frequency spectrum. Feedback comb filters are the foundation of the reverb tail.

```
x[n] ──────────────────────────────────────────► y[n]
       │                                   ▲
       └──► [Delay: D samples] ──► [× g] ──┘

y[n] = x[n] + g * y[n - D]
```

- `D` = delay length (determines comb frequency)
- `g` = feedback gain (controls decay time)

### All-Pass Filter

Passes all frequencies at equal amplitude but alters their phase. Used to diffuse the signal and increase echo density without coloring the tone.

```
x[n] ──[× -g]─────────────────────────────────► y[n]
       │                                   ▲
       └──► [Delay: D] ──────────────────► +
                         │          ▲
                         └──[× g]──┘

y[n] = -g * x[n] + x[n - D] + g * y[n - D]
```

### Feedback Delay Network (FDN)

A matrix of N delay lines with feedback routed through a unitary matrix. Produces highly diffuse, colorless reverberation and is the basis of most modern algorithmic reverbs.

```
[x1]     [Delay 1]     [Feedback]     [y1]
[x2]  +  [Delay 2]  ×  [Matrix  ]  =  [y2]
[x3]     [Delay 3]     [(unitary)]    [y3]
[x4]     [Delay 4]                    [y4]
```

---

## Signal Flow (Typical Algorithmic Reverb)

```
Input (Dry)
    │
    ├──────────────────────────────────────► Dry Output
    │
    ▼
[Pre-delay]
    │
    ▼
[Early Reflections]  ← sparse delay taps, simulates room geometry
    │
    ▼
[Diffusion Network]  ← all-pass filters, densifies the signal
    │
    ▼
[FDN / Comb Filter Bank]  ← feedback loops create decaying tail
    │
    ▼
[Damping / EQ]  ← high-frequency rolloff over time
    │
    ▼
[Wet Output]
    │
    ▼
[Dry/Wet Mixer] ──────────────────────────► Final Output
```

---

## Convolution Reverb in Detail

Convolution reverb works by applying the **convolution** operation between the input signal and an impulse response:

```
y[n] = (x * h)[n] = Σ x[k] * h[n - k]
                     k
```

Where:
- `x[n]` = dry input signal
- `h[n]` = impulse response (IR)
- `y[n]` = wet output signal

In practice this is computed efficiently using **FFT-based fast convolution** (overlap-add or overlap-save), which transforms both signals to the frequency domain, multiplies them, then transforms back:

```
Y(f) = X(f) × H(f)
y[n] = IFFT(Y(f))
```

---

## Psychoacoustics: Why Reverb Matters

- **Spatial perception:** Reverb gives the brain cues to estimate room size, distance, and direction.
- **Envelopment:** Late reverberation surrounds the listener, creating a sense of immersion.
- **Cohesion:** Applying shared reverb to multiple tracks in a mix makes them feel like they exist in the same physical space.
- **Pitch masking:** The blending nature of reverb smooths transients, which is why excessive reverb can reduce clarity.

---

## Practical Tips for Using Reverb

- **Pre-delay** of 20–40 ms on vocals keeps the dry signal intelligible while still adding space.
- **High-pass the reverb return** (cut below 100–200 Hz) to prevent low-end muddiness.
- **Automate decay time** to match song tempo: `RT60 (ms) = 60000 / BPM * beats`.
- **Send, don't insert:** Use a send/return bus so multiple tracks share one reverb instance, saving CPU and creating cohesion.
- **Layer reverbs:** Combine a short room reverb for body with a long hall reverb for tail to achieve complex spaces.

---

## Glossary

| Term | Definition |
|---|---|
| **RT60** | Time for sound to decay 60 dB — the standard measure of reverb decay time |
| **Impulse Response (IR)** | A recording of how a space responds to a brief transient signal |
| **Convolution** | A mathematical operation that combines two signals; used in convolution reverb |
| **FDN** | Feedback Delay Network — a common algorithmic reverb architecture |
| **Dry Signal** | The original, unprocessed audio |
| **Wet Signal** | The processed, effect-applied audio |
| **Diffusion** | The density of reflections; high diffusion = smooth, dense tail |
| **Damping** | Absorption of high frequencies over time |
