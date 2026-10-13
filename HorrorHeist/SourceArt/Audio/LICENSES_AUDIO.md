# Audio licensing

## Origin of every file in `SourceArt/Audio`

All audio in this folder (ambience, sound effects, footsteps, UI sounds, dialogue-category
breaths/whispers and music) is **synthesized from scratch by the project's own Python scripts**.
No recordings, sample libraries, sound packs, loops or other third-party audio material of any
kind were used, downloaded or sampled. Every waveform is computed from noise generators,
oscillators, filters, modal resonators and algorithmic impulse responses.

| Script (in `Tools/AssetGen/audio/`) | Produces |
|---|---|
| `hhsynth.py` | Shared DSP library (noise, filters, envelopes, room impulse responses, WAV writing, manifest). Produces no files on its own. |
| `gen_sfx.py` | First batch: hideout ambience, rain, thunder, creaks, whispers, door/locker/switch SFX, indoor footsteps, UI sounds. |
| `gen_music.py` | Hideout theme, title and departure stings, the two in-world radio stations. |
| `gen_world_sfx.py` | Second batch: town and house ambience, haunting events, lights/devices, gameplay SFX, outdoor footsteps, extra UI, haunting drone/stingers and mission stings. |
| `gen_vehicle_sfx.py` | Third batch: the van's engine loop and horn, the patrol car siren, the garage door motor, a car bump and the neighbour's shout. |

Run any script from the `HorrorHeist` folder (e.g. `python Tools/AssetGen/audio/gen_world_sfx.py`)
to regenerate its files bit-for-bit; all randomness is seeded.

## Licence status

Because the audio is generated entirely by original project code, these files are **original
project assets**. They carry no external licence obligations, attribution requirements or
royalties. The scripts depend on numpy, scipy and soundfile as tools only; those libraries'
licences cover the libraries, not the audio they help compute.
