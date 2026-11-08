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

## Licenses

The code is MIT licensed (see `LICENSE`). The fonts are under the SIL Open Font License 1.1: Poiret One © The Poiret One Project Authors, and IBM Plex Mono © IBM Corp. Their license texts are in `fonts/`.

This is a relaxation aid, not a medical device.
