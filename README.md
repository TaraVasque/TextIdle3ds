# Text Idle

A small text-based idle/clicker game made for **AuroraOS** using the **Auric** programming language.

The goal is simple: generate as many coins as possible, upgrade your production, build increasingly powerful generators, and keep the numbers going up.

## About

`Text Idle` is designed around a retro terminal/text aesthetic rather than a traditional graphical idle game.
(because it was easier)
The entire interface and text is drawn.
(ww2 face after seeing this^)

* Designed for AuroraOS on Nintendo 3DS


Every 10th click can trigger a critical click once the critical upgrade has been purchased.

Combos reward continued clicking without letting the combo timer expire.



## Save System

`Text Idle` uses a hellish save system.

A save code contains the important permanent game data, including:

* Current coins
* Bytes
* Workers
* Factories
* Cores
* Click power
* Critical upgrade
* Total clicks
* Critical clicks
* Best combo
* Lifetime coins
* Highest coins
* Generator click upgrades
* Production upgrade

Example:

```text
C888Cur222W15F3Co7P12Click1Crit2Cnt1456CC37Comb42Lif92834Hi45000CU3WU1FU0CoU0PU1
```

Save codes do not use dash separators.

### Loading

Each value is entered individually and the game moves to the next section when `NEXT` is selected.

The top screen shows the progress of the save-code entry so it is readable and easy to see which sections have already been completed.

Invalid values are rejected before the game accepts the save.

please exuse the crude format and way ive made the game images i couldnt figure out how to get a screen shot of the game, using auroras l1+r1 ss combo didnt work in my game so i resorted to taking physical pictures and putting them in the gameimages folder




