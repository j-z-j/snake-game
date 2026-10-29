# -*- coding: utf-8 -*-
"""生成三个很小的 WAV 音效：吃苹果、金果/宝石、死亡。无第三方依赖。"""
import math
import os
import struct
import wave

SR = 22050
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "sounds")
os.makedirs(OUT, exist_ok=True)


def save(name, samples):
    path = os.path.join(OUT, name)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        frames = bytearray()
        for s in samples:
            v = max(-1.0, min(1.0, s))
            frames += struct.pack("<h", int(v * 32767))
        w.writeframes(bytes(frames))
    print("wrote", path, len(samples) / SR, "s")


def tone(dur, f0, f1, vol=0.32, decay=3.0, square=False):
    n = int(SR * dur)
    out = []
    phase = 0.0
    for i in range(n):
        t = i / n
        f = f0 + (f1 - f0) * t
        phase += 2.0 * math.pi * f / SR
        s = math.sin(phase)
        if square:
            s = 0.75 * math.sin(phase) + 0.25 * math.sin(3 * phase)
        env = min(1.0, i / (SR * 0.004)) * math.exp(-decay * t)
        out.append(s * env * vol)
    return out


def silence(dur):
    return [0.0] * int(SR * dur)


# 苹果：短促上扬的“叮”
save("eat.wav", tone(0.075, 880, 1480, 0.34, 4.5))

# 金果/宝石：三连音
bonus = []
for f in (660, 990, 1320):
    bonus += tone(0.055, f, f, 0.28, 2.2)
    bonus += silence(0.008)
save("bonus.wav", bonus)

# 死亡：下滑的低音
save("dead.wav", tone(0.50, 420, 90, 0.34, 2.6, square=True))
