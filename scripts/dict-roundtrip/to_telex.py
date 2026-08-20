#!/usr/bin/env python3
"""Reverse-map Vietnamese Unicode syllables to the Telex keystrokes that produce them.

Emits "<syllable>\t<telex keys>" so the C++ core can be checked round-trip.
"""
import sys, unicodedata

TONE = {'̀': 'f', '́': 's', '̃': 'x', '̉': 'r', '̣': 'j'}
SHAPE = {'̂': 'hat', '̆': 'breve', '̛': 'horn'}

def syllable_to_telex(s):
    keys, tone = [], ''
    for ch in s:
        if ch == 'đ': keys.append('dd'); continue
        if ch == 'Đ': keys.append('DD'); continue
        d = unicodedata.normalize('NFD', ch)
        base, shape = d[0], None
        for m in d[1:]:
            if m in TONE:
                if tone: return None          # two tones on one syllable: not typeable
                tone = TONE[m]
            elif m in SHAPE:
                shape = SHAPE[m]
            else:
                return None                    # mark we do not model
        low = base.lower()
        if shape is None:
            keys.append(base)
        elif shape == 'hat':
            if low not in 'aeo': return None
            keys.append(base + (base if base.islower() else base.lower()))
        elif shape == 'breve':
            if low != 'a': return None
            keys.append(base + 'w')
        elif shape == 'horn':
            if low not in 'ou': return None
            keys.append(base + 'w')
    return ''.join(keys) + tone

def main():
    for line in sys.stdin:
        word = line.strip()
        if not word: continue
        parts = word.split(' ')
        keys = [syllable_to_telex(p) for p in parts]
        if any(k is None for k in keys): continue
        print(word + '\t' + ' '.join(keys))

main()
