# DIY Electric Salt Spoon
A DIY electro-gustation spoon that artificially increases the salty taste of foods using microscopic electric currents. Inspired by Kirin's Electric Salt Spoon and built as an open-source version to test the concept. Watch the <a href="https://www.youtube.com/watch?v=7jDd-lppwBE" target="_blank">build and testing video on YouTube</a> (EN subs available).

## Some components used not mentioned in the source files:
- 1mm 316L stainless steel wire in the spoon tip (to lower the risk of electrolysis and corrosion)
- 1mm copper plate as the hand electrode
- 12V 23A battery (small batteries commonly used in remotes)

## Scientific Background & Inspiration
The waveform blueprint and the core concept of this project are based on the electro-gustation research conducted by Miyashita Laboratory (Meiji University) and Kirin Holdings. 
You can read the original paper here:
- [Design of Electrical Stimulation Waveform for Enhancing Saltiness and Experiment on Low-Sodium Dieters](https://www.frontiersin.org/journals/virtual-reality/articles/10.3389/frvir.2022.879784/full) (Kaji, Sato, & Miyashita, 2022)

> ## ⚠️ SAFETY WARNING & DISCLAIMER
> **BUILD AND USE AT YOUR OWN RISK.**
> 
> This project involves passing an electrical current directly through the human body (specifically, from the hand to the tongue) to pull sodium ions towards your tongue for an increase in saltiness. While the circuit is designed with hardware current-limiting features to keep this current at microscopic, safe levels, human error in assembly can result in injury.
> 
> **CRITICAL POWER SUPPLY RULE:**
> When applying this device to a person, it must **ONLY** be powered by a small **12V 23A battery**. 
> * **NEVER** connect this device to a human while it is plugged into a USB port, a wall adapter, a bench power supply, or a high-current battery (like a standard 9V or LiPo). 
> * Doing so risks bypassing the safety limits, which can cause severe pain, chemical burns on the tongue, or serious electrical injury.
> 
> **Always test your fully assembled circuit with a multimeter** (you can place a 10kΩ resistor across the electrodes to simulate the hand-to-tongue resistance) to verify the output current is within the expected micro-amp range before ever letting it touch your skin or mouth.
