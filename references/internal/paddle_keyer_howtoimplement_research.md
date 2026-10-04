## Iambic-B
Implementing Iambic Mode B can be notoriously finicky. If you get the timing wrong, the keyer feels "sticky," drops elements, or fires off extra dits when the user lets go of the paddle.

The secret to a perfect feeling Iambic Mode B keyer is **complete decoupling of input from output**. Your paddles do not directly control the oscillator. Instead, the paddles only set "memory flags," and an independent timing loop (a state machine) consumes those flags.

Here is the blueprint for implementing flawless Iambic Mode B logic, suitable for your high-priority C++ audio thread.
### The Timing Math
All CW timing is based on the duration of a single dit. Using the standard "PARIS" word timing model, you can calculate the duration in milliseconds:

- **Dit Length (T):** `1200 / WPM` milliseconds.
- **Dah Length:** `3 * T`
- **Inter-element Space:** `1 * T` (The silence between dits/dahs in the same letter).

Note: For absolute precision in your audio engine, convert these milliseconds into **audio samples**. For example, at a 48kHz sample rate, a 60ms dit is exactly 2,880 samples.
### The Core Variables
You need a few global state variables in your engine:

- `dit_memory` (Boolean): Latches to true if the dit paddle is touched.
- `dah_memory` (Boolean): Latches to true if the dah paddle is touched.
- `last_element` (Enum/Int): Remembers if the last played tone was a DIT or a DAH.
- `state` (Enum): The current state of the keyer (IDLE, PLAYING_MARK, PLAYING_SPACE).
- `timer` (Int): Counts down the samples/milliseconds of the current state.
### The Input Thread (The Latch)
Whenever your USB adapter detects a paddle press (even a microscopic tap), it must immediately set the memory flag. **Do not clear these flags when the paddle is released.**
```cpp
void onPaddleStateChanged(bool ditPressed, bool dahPressed) {
    if (ditPressed) dit_memory = true;
    if (dahPressed) dah_memory = true;
}
```
### The Output Engine (The State Machine)
This logic runs continuously in your audio callback (or high-resolution timing loop). It evaluates the memory flags only at specific safe moments.
```cpp
// Called continuously in your audio loop
void processKeyer() {
    
    // 1. Decrement the active timer
    if (timer > 0) {
        timer--;
        return; // We are busy playing a tone or a space. Do nothing else.
    }

    // 2. State Machine Transitions (Timer is 0)
    switch (state) {
        
        case PLAYING_MARK:
            // The tone just finished. We must now force an inter-element space.
            turnOffOscillator();
            state = PLAYING_SPACE;
            timer = DIT_LENGTH_SAMPLES; // 1T silence
            break;

        case IDLE:
        case PLAYING_SPACE:
            // The space finished (or we were idle). Now we decide what to do next.
            
            // Check for Iambic Squeeze (both memories are true)
            if (dit_memory && dah_memory) {
                if (last_element == DIT) {
                    playDah();
                } else {
                    playDit();
                }
            } 
            // Only Dit is pressed/remembered
            else if (dit_memory) {
                playDit();
            } 
            // Only Dah is pressed/remembered
            else if (dah_memory) {
                playDah();
            } 
            // Nothing is pressed, and memories are empty
            else {
                state = IDLE;
            }
            break;
    }
}

// Helper functions to execute the elements and consume the memory
void playDit() {
    turnOnOscillator();
    dit_memory = false;          // Consume the memory
    last_element = DIT;          // Remember for squeeze alternation
    state = PLAYING_MARK;
    timer = DIT_LENGTH_SAMPLES;  // 1T tone
}

void playDah() {
    turnOnOscillator();
    dah_memory = false;          // Consume the memory
    last_element = DAH;          // Remember for squeeze alternation
    state = PLAYING_MARK;
    timer = DAH_LENGTH_SAMPLES;  // 3T tone
}
```

### Why this naturally creates "Mode B"

The defining characteristic of Mode B is the "extra element" when you release a squeeze.

Imagine you are squeezing both paddles. Both `dit_memory` and `dah_memory` are constantly being set to true. The state machine happily alternates DIT -> DAH -> DIT -> DAH because of the `last_element` tie-breaker.

If you release the paddles _while the DAH tone is playing_, the `dit_memory` was already flagged to `true` during the squeeze. When the DAH finishes, and the 1T space finishes, the engine checks the memories. `dah_memory` is now false (you let go), but `dit_memory` is _still true_ because it latched before you let go. The engine plays that final DIT, consumes the memory, and goes to IDLE. That is the exact mechanical behavior of Iambic Mode B, achieved entirely through simple logic.
## Iambic-A
The transition from Mode B to Mode A fundamentally changes how the keyer's brain works.

While Mode B is purely **memory-driven** (every paddle touch unconditionally latches a memory flag), Mode A is **live-state driven**. In Mode A, the keyer relies on the _physical_ state of the paddles at the exact moment a decision is made, and only uses memory flags for "element insertion" (tapping one paddle while the other is currently playing).

If you use the Mode B code from the previous step and just clear the memories when the paddles are released, you will accidentally destroy the ability to insert single elements.

Here is how to structure the logic to achieve perfect Iambic Mode A.
### The Core Difference: Live State vs. Conditional Memory
To implement Mode A, your input thread must separate the "live" state of the paddles from the "memory" of the paddles.

Furthermore, memory flags are no longer set unconditionally. In Mode A, a memory flag is **only** latched if you tap a paddle while the keyer is busy playing the _opposite_ element.

Add two new variables to your engine:

- `live_dit` (Boolean): The current physical state of the Dit paddle.
- `live_dah` (Boolean): The current physical state of the Dah paddle.
### The Input Thread (Live State & Edge Detection)
Your USB adapter thread now needs to track both the live state and detect "rising edges" (the exact moment a paddle is pushed down) to conditionally set the insertion memories.
```cpp
// Called whenever the USB adapter reports a change
void onPaddleStateChanged(bool ditPressed, bool dahPressed) {
    
    // Detect rising edges for element insertion memory
    if (ditPressed && !live_dit) {
        // MODE A RULE: Only remember a DIT tap if we are currently busy playing a DAH
        if (state != IDLE && last_element == DAH) {
            dit_memory = true;
        }
    }
    
    if (dahPressed && !live_dah) {
        // MODE A RULE: Only remember a DAH tap if we are currently busy playing a DIT
        if (state != IDLE && last_element == DIT) {
            dah_memory = true;
        }
    }

    // Update the live physical state of the paddles
    live_dit = ditPressed;
    live_dah = dahPressed;
}
```
### The Output Engine (The Mode A State Machine)
Inside your high-priority audio callback, the decision block is inverted compared to Mode B. You must evaluate the `live` physical paddle states first. You only fall back to the `memory` flags if the paddles are physically released.
```cpp
// Called continuously in your audio loop
void processKeyer() {
    
    // 1. Decrement the active timer
    if (timer > 0) {
        timer--;
        return; // Busy playing a tone or space. Do nothing.
    }

    // 2. State Machine Transitions (Timer is 0)
    switch (state) {
        
        case PLAYING_MARK:
            // Tone finished. Force an inter-element space.
            turnOffOscillator();
            state = PLAYING_SPACE;
            timer = DIT_LENGTH_SAMPLES; // 1T silence
            break;

        case IDLE:
        case PLAYING_SPACE:
            // Space finished. Decide what to do next.
            
            // PRIORITY 1: Squeeze (Both LIVE paddles are physically down)
            if (live_dit && live_dah) {
                if (last_element == DIT) playDah();
                else playDit();
            } 
            // PRIORITY 2: Single LIVE paddle is physically down
            else if (live_dit) {
                playDit();
            } 
            else if (live_dah) {
                playDah();
            } 
            // PRIORITY 3: Paddles are released, check conditional memory
            else if (dit_memory) {
                playDit();
            } 
            else if (dah_memory) {
                playDah();
            } 
            // Nothing is pressed, and memories are empty
            else {
                state = IDLE;
            }
            break;
    }
}

// playDit() and playDah() remain the same as Mode B:
// They must set state, timer, last_element, and consume their respective memory flag (e.g., dit_memory = false).
```
### Why this perfectly replicates Mode A hardware
Let's trace the two critical scenarios that define Mode A to see why this architecture works flawlessly:

**Scenario 1: The Squeeze Release (No extra element)**

1. You squeeze both paddles. `live_dit` and `live_dah` are true.
2. The decision point evaluates Priority 1 and alternates (let's say it plays a Dah).
3. While the Dah is playing, you release both paddles. `live_dit` and `live_dah` become false.
4. _Crucially_, because you didn't press the Dit paddle _after_ the Dah started, the rising edge condition in the input thread never occurred, so `dit_memory` is false.
5. When the Dah and its space finish, the decision point sees no live paddles and no memories. The keyer drops to `IDLE`. (Mode B would have played a trailing Dit here).

**Scenario 2: Single Element Insertion (The "Tap")**

1. You press and hold Dah. `live_dah` is true. The keyer starts playing a Dah. `last_element = DAH`.
2. While the Dah is sounding, you quickly tap the Dit paddle and let go.
3. The input thread sees the rising edge of the Dit paddle. Because `state != IDLE` and `last_element == DAH`, it sets `dit_memory = true`.
4. The Dah and its space finish. You are still holding Dah (`live_dah` is true), but you let go of Dit (`live_dit` is false).
5. Wait, looking at the logic above: if `live_dah` is true, Priority 2 will just play another Dah! To ensure proper alternating insertion while holding a paddle, we need a slight tweak to the decision hierarchy to respect squeeze insertions.

_Correction for flawless operation:_ If you want the tapped element to interrupt a held element (standard element insertion), you must evaluate the **memories before the live single-paddle state**.

Swap Priority 2 and 3 in your code:

1. **Live Squeeze** (`live_dit && live_dah`)
2. **Opposite Memory** (`dit_memory` or `dah_memory`)
3. **Live Single** (`live_dit` or `live_dah`)

By placing memory just below the live squeeze, that tapped `dit_memory` will fire exactly when it's supposed to, playing the Dit, consuming the memory, and then returning to the held Dah on the next cycle.
## Ultimatic
Ultimatic mode is a fascinating departure from Iambic. Instead of forcing an alternating dit-dah-dit-dah pattern during a squeeze, Ultimatic operates on a strict **"last-in wins"** philosophy.

Whichever paddle was pressed _most recently_ takes absolute control and repeats its element, even if you are still holding down the other paddle. When you release the newer paddle, control immediately reverts to the older, still-held paddle.

Because we don't need to force an alternating pattern, we can completely eliminate the `last_element` tracking variable used in Iambic modes. Instead, we introduce a `last_pressed` priority tracker.

Here is the blueprint for modifying your state machine to support Ultimatic keying.
### The New Priority State
You need a new variable to track which paddle currently owns the priority lock.
```cpp
enum Priority { NONE, DIT, DAH };
Priority last_pressed = NONE;
```
### The Input Thread (Rising and Falling Edges)
In Iambic, we mostly cared about rising edges to set memory. In Ultimatic, we must aggressively manage the `last_pressed` priority on **both** rising edges (when a paddle is pressed) and falling edges (when a paddle is released).
```cpp
// Called whenever the USB adapter reports a change
void onPaddleStateChanged(bool ditPressed, bool dahPressed) {
    
    // 1. Rising Edge Detection: Set memory and steal priority
    if (ditPressed && !live_dit) {
        dit_memory = true;
        last_pressed = DIT;
    }
    if (dahPressed && !live_dah) {
        dah_memory = true;
        last_pressed = DAH;
    }

    // 2. Falling Edge Detection: Priority fallback
    // If you release a paddle while still holding the other, 
    // priority must instantly revert to the held paddle.
    if (!ditPressed && live_dit) {
        if (dahPressed) last_pressed = DAH;
    }
    if (!dahPressed && live_dah) {
        if (ditPressed) last_pressed = DIT;
    }

    // Update physical states
    live_dit = ditPressed;
    live_dah = dahPressed;
}
```
### The Output Engine (The Ultimatic State Machine)
Inside your audio callback, the decision tree is simplified. Memory flags (for fast, single-element taps) still take absolute precedence. If there are no memory flags, you evaluate the live squeeze based entirely on the `last_pressed` priority.
```cpp
// Called continuously in your audio loop
void processKeyer() {
    
    // 1. Decrement active timer
    if (timer > 0) {
        timer--;
        return; 
    }

    switch (state) {
        
        case PLAYING_MARK:
            turnOffOscillator();
            state = PLAYING_SPACE;
            timer = DIT_LENGTH_SAMPLES; // 1T silence
            break;

        case IDLE:
        case PLAYING_SPACE:
            
            // PRIORITY 1: Consuming Memories (Fast taps)
            if (dit_memory && dah_memory) {
                // Rare: both latched simultaneously. Let priority break the tie.
                if (last_pressed == DAH) playDah();
                else playDit();
            }
            else if (dah_memory) {
                playDah();
            }
            else if (dit_memory) {
                playDit();
            }
            
            // PRIORITY 2: Live Held States (The Ultimatic Squeeze)
            else if (live_dit && live_dah) {
                // This is the core of Ultimatic. No alternation. 
                // Just repeat whatever was pressed last.
                if (last_pressed == DAH) playDah();
                else playDit();
            }
            else if (live_dit) {
                playDit();
            }
            else if (live_dah) {
                playDah();
            }
            
            // PRIORITY 3: Nothing active
            else {
                state = IDLE;
            }
            break;
    }
}

// playDit() and playDah() remain identical to previous implementations.
// They consume their respective memory flag, set the state, and start the timer.
```
### Why this nails the Ultimatic feel
Let's trace the classic Ultimatic maneuver—inserting dahs into a string of dits:

1. **Press and hold Dit.** `last_pressed` becomes `DIT`. The engine loops, playing dits.
2. **While holding Dit, press and hold Dah.** The input thread catches the rising edge: `dah_memory = true` and `last_pressed` switches to `DAH`.
3. **The switch.** The current dit finishes. The engine checks memories, sees `dah_memory`, and plays a Dah. Next cycle, memories are empty, but `live_dit` and `live_dah` are both true. Because `last_pressed` is `DAH`, it plays another Dah. It will play dahs forever as long as both are held.
4. **The release.** You release Dah but keep holding Dit. The falling edge logic in the input thread sees the release of Dah while Dit is true, instantly reverting `last_pressed` back to `DIT`.
5. **The return.** The current Dah finishes. Memories are empty. `live_dit` is true, `live_dah` is false. The engine resumes playing dits seamlessly.