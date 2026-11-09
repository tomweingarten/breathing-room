# Breathing Room

A slow-breathing pacer in a single HTML file. An art-deco ring swells as you breathe in and settles as you breathe out, and a soft tone marks each change, so you can follow along with your eyes closed.

**Try it:** https://tomweingarten.github.io/breathing-room/

## What it does

- **Rhythms:** 4 in · 6 out (6 breaths a minute, the default), 5 · 5, 4 · 8 and 5 · 7.
- **Sessions:** 3, 10 or 15 minutes, with a brass ring that fills as the session runs.
- **Soft tones:** a higher tone means breathe in, a lower one means breathe out. Two closing notes end the session.
- **Screen glow:** the whole screen fills with colour on the in-breath and fades on the out-breath, so you can follow it from the corner of your eye.
- **Works as an app:** add it to your home screen and it opens full screen. It keeps the screen awake during a session, and on iPhone the tones play even with the silent switch on.
- Light and dark themes follow your system. Reduced-motion settings are respected.
- Your settings are remembered in your own browser. Nothing is sent anywhere, and there is no tracking.

## Run it

There's no build step and no dependencies. Open `index.html` in a browser, or serve the folder:

```sh
python3 -m http.server 8000
# then visit http://localhost:8000
```

To host it yourself, put the folder on any static host (GitHub Pages, Netlify, a plain web server). It needs HTTPS for the screen-awake feature and for adding to the home screen.

## Files

| File | What it is |
|---|---|
| `index.html` | The whole app: markup, styles and script |
| `manifest.webmanifest`, `icon-*.png` | Home-screen app name and icons |
| `fonts/` | Poiret One and IBM Plex Mono, bundled so it works offline |

## Nintendo DS version

`nds/` is the same pacer for the Nintendo DS (DS, DS Lite, DSi, 3DS): the ring on the top screen, settings and Begin on the touchscreen, dark theme by default. The A button also begins and stops; the +pad picks and changes settings.

**Vibration** (BUZZ), for following along in bed with the sound off and your eyes closed:
- **headphone jack:** a hum on the headphone output drives a small haptic actuator through an amplifier (e.g. Adafruit PAM8302 + a Vybronics VG0832012 LRA coin). The vibration swells on the in-breath and fades on the out-breath; plugging into the jack mutes the speakers. HUM sets the frequency (match your actuator, 235 Hz for the VG0832012) and STRENGTH caps the level; changing either plays a 2-second test swell.
- **rumble pak:** a DS Rumble Pak in the GBA slot (DS and DS Lite): two short buzzes for in, one long buzz for out.

**Lights out** switches both screens off a few seconds into a session and keeps them off through the end; any button wakes them. Sessions run 3 to 30 minutes and ease out over the last minute. Settings are saved to `/data/breathing-room.cfg` on the SD card.

**Install:** download `breathing-room.nds` from the [latest release](https://github.com/tomweingarten/breathing-room/releases) and copy it to your flashcart's microSD card (a DS Lite needs a Slot-1 flashcart such as an R4). On a DSi or 3DS, run it from TWiLight Menu++.

**Build:** GitHub Actions builds it on every change to `nds/` (see `.github/workflows/nds.yml`). To build locally, use [BlocksDS](https://blocksds.skylyrac.net):

```sh
docker run --rm -v "$PWD":/work -w /work/nds skylyrac/blocksds:slim-latest make
```

## Licenses

The code is MIT licensed (see `LICENSE`). The fonts are under the SIL Open Font License 1.1: Poiret One © The Poiret One Project Authors, and IBM Plex Mono © IBM Corp. Their license texts are in `fonts/`.

This is a relaxation aid, not a medical device.
