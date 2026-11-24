# Copy style guide

How UI strings are written. Strings live in Android resource files. Review keeps these rules; there is no automated check.

## Voice

The app talks like a well-made tool: quiet, exact, brief. It says what a thing does or what happened, in plain words.

- Short sentences. One idea per sentence.
- No exclamation marks, no emoji, no cheering, no filler, no jokes, no chatty voice.
- No greetings and no use of the player's name. The home screen starts with the games.
- No em dashes in UI strings. Use a period or a comma.
- No "not just X, but Y" constructions.
- Use the words the player already knows: game, save, memory card, BIOS, controller. Explain technical terms the first time they appear in settings.

## Buttons and actions

A verb with an object. The label says exactly what happens on tap.

| Write | Not |
| --- | --- |
| Play | Start your adventure |
| Import BIOS | Get started |
| Add game folder | Add |
| Delete save state | Remove |
| Export backup | Seamless save syncing |
| Tune for this device | Smart Optimize |

Destructive actions name the object and confirm once: "Delete save state 3?" with buttons "Delete" and "Keep".

## Errors

Three parts, in order: what happened, why (if known), what to do next.

> Couldn't read this BIOS file. PS2 BIOS dumps are 4 MB and this file is 3 MB. Choose a different file.

> Couldn't open the game folder. Android removed access to it. Choose the folder again.

Never "Oops", never "Something went wrong", never an error code alone. When the cause is unknown, say so and point to the bug report: "The game stopped responding. The cause is not known. Create a bug report to send to the developers."

## Progress

Progress shows real work and real numbers. No fake progress bars, no spinners on blank screens, no bare "Optimizing...".

| Write | Not |
| --- | --- |
| Compiling shaders: 41 of 120 | Optimizing your experience... |
| Scanning folder: 212 games found | Hang tight |
| Exporting backup: 18 of 64 files | Almost there |

## Empty states

Say what is missing and the next action.

> No games yet. Choose the folder that holds your game files.

> No save states for this game. Open the Quick menu during play and choose Save.

## Settings

- The name says what it controls: "Internal resolution", not "Visual quality".
- The description says what changes on screen or in speed, and what it costs: "Internal resolution 4x. Sharper image, needs a fast GPU."
- When a game profile overrides a value, say so: "Set by this game's profile. Reset to use your setting."

## Examples to avoid

| Instead of | Write |
| --- | --- |
| Oops! Something went wrong. | Couldn't read this BIOS file. PS2 BIOS dumps are 4 MB and this file is 3 MB. Choose a different file. |
| Optimizing your experience... | Compiling shaders: 41 of 120 |
| Let's get started! | Import BIOS |
| No games found. Let's add some! | No games yet. Choose the folder that holds your game files. |
| Unlock stunning visuals with Ultra Mode | Internal resolution 4x. Sharper image, needs a fast GPU. |
| Smart Optimize | Tune for this device |
| Seamless save syncing | Export backup |

## Words to be careful with

Some words usually read as filler or marketing: seamless, unlock, unleash, elevate, empower, effortless, magic, smart, powerful, ultimate, "Let's", "Oops", "Something went wrong", "Get ready", "Enjoy", "Experience". None is forbidden. Use one when it is the precise word ("Unlock the memory card slot" is fine), never for effect.

## Legal wording

- "PS2" appears only to describe the hardware ("PS2 BIOS", "PS2 games"). No Sony names, logos or product names beyond that.
- The BIOS step explains how to dump a BIOS from your own console. It never links to downloads.
- The About screen states: "Better AetherSX2 is not affiliated with Sony Interactive Entertainment or with the developer of the original AetherSX2."

## Brazilian Portuguese

- Write natural pt-BR, the way a Brazilian app would say it. Never translate word for word.
- Same rules as English: short, plain, no exclamation marks, verb plus object on buttons ("Jogar", "Importar BIOS", "Adicionar pasta de jogos", "Excluir estado salvo").
- Use "você" implicitly; prefer impersonal or imperative forms over addressing the player.
- Keep technical terms that Brazilian players use as-is: BIOS, shader, save state is "estado salvo", memory card is "memory card".
- The same care applies to Portuguese filler ("incrível", "aproveite", "opa").
- A native speaker reviews every pt-BR string before release.
