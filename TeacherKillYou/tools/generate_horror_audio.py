"""Create replaceable, original placeholder WAVs. Existing audio is never overwritten."""
from array import array
from pathlib import Path
import math
import random
import wave

RATE = 22050
ROOT = Path(__file__).resolve().parents[1] / "Data" / "Audio"
random.seed(71)


def write(relative, duration, sample):
    path = ROOT / relative
    if path.exists():
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    pcm = array("h")
    for i in range(round(duration * RATE)):
        t = i / RATE
        value = sample(t)
        pcm.append(round(max(-0.8, min(0.8, value)) * 32767))
    with wave.open(str(path), "wb") as audio:
        audio.setnchannels(1)
        audio.setsampwidth(2)
        audio.setframerate(RATE)
        audio.writeframes(pcm.tobytes())
    print(path.relative_to(ROOT), len(pcm), "samples")


def footstep(t):
    return (0.35 * math.sin(2 * math.pi * 85 * t) + 0.25 * random.uniform(-1, 1)) * math.exp(-24 * t) * min(1, t / .004)


filtered_noise = 0.0


def growl(t):
    global filtered_noise
    filtered_noise = .93 * filtered_noise + .07 * random.uniform(-1, 1)
    envelope = math.sin(math.pi * t / 1.65) ** 2
    voice = math.sin(2 * math.pi * 63 * t + 2 * math.sin(2 * math.pi * 3 * t))
    return envelope * (.18 * voice + .1 * math.sin(2 * math.pi * 126 * t) + .45 * filtered_noise)


def reload(t):
    # Three mechanical clicks follow the existing 2.29 second animation.
    value = 0.0
    for start, frequency, gain in [(0.05, 620, .28), (.95, 450, .32), (1.85, 950, .28)]:
        age = t - start
        if 0 <= age < .22:
            value += gain * (random.uniform(-1, 1) + .3 * math.sin(2 * math.pi * frequency * age)) * math.exp(-35 * age) * min(1, age / .002)
    return value


ambient_noise = 0.0


def ambience(t):
    global ambient_noise
    ambient_noise = .997 * ambient_noise + .003 * random.uniform(-1, 1)
    # Integer cycles over 12 seconds keep the tonal layers continuous at the loop.
    edge = min(1.0, t / .15, (12 - t) / .15)
    return edge * (.09 * math.sin(2 * math.pi * 48 * t) + .04 * math.sin(2 * math.pi * 71 * t) + .5 * ambient_noise)


if __name__ == "__main__":
    write("SE/footstep.wav", .28, footstep)
    write("SE/enemy_growl.wav", 1.65, growl)
    write("SE/reload.wav", 2.29, reload)
    write("BGM/ambience.wav", 12, ambience)
