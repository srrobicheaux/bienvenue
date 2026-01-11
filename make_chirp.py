# Run this once → paste output into chirp_buffer.c
import numpy as np

SAMPLE_RATE = 1_000_000
F_START = 38500
F_END   = 41500
DURATION_S = 0.012

t = np.linspace(0, DURATION_S, int(SAMPLE_RATE * DURATION_S), endpoint=False)
phase = 2*np.pi * (F_START * t + (F_END-F_START)/(2*DURATION_S) * t*t)
samples = (np.sin(phase) * 32760).astype(np.int16)

print("#pragma once")
print("const int16_t chirp_buffer[] = {")
for i in range(0, len(samples), 12):
    line = "    " + ", ".join(f"{x:+6d}" for x in samples[i:i+12])
    print(line + ("," if i+12 < len(samples) else ""))
print("};")
print(f"#define CHIRP_LENGTH {len(samples)}")
