# Scraper for goodluckOS

Names, descriptions, pictures and gameplay videos for your games, from [ScreenScraper](https://www.screenscraper.fr), fetched by the console itself. It's an app for goodluckOS's App Installer: no PC program, and nothing to add to the image.

It looks each game up by its file's CRC and size, so renamed ROMs are found too, and writes what it finds next to the games: a `gamelist.xml` in each ROM folder, a `gamelist.<language>.xml` with the descriptions in the interface's language, and the pictures and videos in `media/`. Any launcher that reads `gamelist.xml` (EmulationStation's format) shows them.

## What it needs

- goodluckOS with the App Installer (System -> App Installer).
- The internet on the console: the USB cable in Network mode, with the PC's connection shared with the console (Internet Sharing on a Mac, the adapter's Sharing tab on Windows, "Shared to other computers" in NetworkManager) or an Android phone's Ethernet tethering. See USB Network in goodluckOS's README.
- A ScreenScraper account. It's free at [screenscraper.fr](https://www.screenscraper.fr); without one, ScreenScraper often turns the scraper away.

## Installing

1. Download `Scraper-<version>.zip` from [Releases](../../releases) and copy it to the `apps` folder of the console's HOME, over USB or with the card in a card reader.
2. On the console, install it in the App Installer. Scraper shows up in the Apps row.

## Using it

| Row | |
|---|---|
| Internet | Whether the console is online. With the USB cable on Files, A switches it to Network; then share the PC's or phone's connection with it. |
| User, Password | Your ScreenScraper account, typed on the on-screen keyboard: A types, B deletes, START is done, SELECT cancels. Aa switches the case and the symbols. |
| Check the account | Logs in and shows how many of today's requests are used. |
| Games | Missing only fills in what the games lack; All replaces it. Answers already received are kept, so asking again costs no requests. |
| Picture | Mix (the screenshot, the box and the logo in one picture), Box, 3D box, Screenshot, Title screen, Logo or None. |
| Videos | Short gameplay clips, a few MB each. |
| Region | Whose names, covers and dates come first. Auto follows the file name, e.g. (USA). |
| Systems | The ROM folders of the launcher's tabs, each one on or off, and the Games tab. |
| Start | Scrapes the systems that are on. |

The app has to stay open while it scrapes. POWER would pause it, like every app, so the scraping page has Screen off, which turns the backlight off and lets the scraper go on; any button turns the screen back on. Closing the app stops the scraper, and Start goes on from where it stopped. A first run over a big collection takes hours, mostly for the videos: leave it on the charger, with the screen off.

The settings stay in `HOME/.config/gl-scrape/settings` and the answers already received in `HOME/.cache/gl-scrape`. The interface follows the launcher's language; it's in English and Brazilian Portuguese.

## Building

On a PC with Docker (Intel or Apple Silicon; on a Mac, Colima or Docker Desktop):

```
./build.sh                  # the app for the console, in out/Scraper
./build.sh app              # the App Installer's zip, out/Scraper-<version>.zip
./build.sh native           # the screen and gl-scrape for this machine, in out/native
./build.sh shot main "Down Down |"   # the native screen in Xvfb, a screenshot at each "|" in out/shots
```

ScreenScraper gives each program its own developer credentials. They come from `SS_DEVID` and `SS_DEVPASSWORD`, in a `.env` file next to `build.sh` or as the repository's Actions secrets, and never from git; a build without them says so on the scraping page.

GitHub Actions (`.github/workflows/app.yml`) builds the zip on every push to `main`; a tag like `v0.1.0` publishes it as that release, with `RELEASE.md` as its notes.

## Layout

| | |
|---|---|
| `scrape/` | gl-scrape, the scraper itself: a small C++ program with no libraries, that runs curl |
| `app/` | the screen (ImGui on SDL2, like goodluckOS's System Settings), its `app.puppy`, icon and translations |
| `tests/console/` | a made-up console for `./build.sh shot`: the launcher's tabs, a few games, an account |
| `build.sh`, `Dockerfile` | the build, in the `goodluckos-scraper` image; downloads and build trees in the `scraper-work` volume |

The zip also carries its own curl, with Mbed TLS and zlib linked in so it needs nothing but the C library, and Mozilla's CA certificates, since goodluckOS has no curl.

## Licenses

GPL-2.0, like goodluckOS, which gl-scrape comes from. The zip's `licenses/` folder has those of [curl](https://curl.se/docs/copyright.html), [Mbed TLS](https://github.com/Mbed-TLS/mbedtls) (Apache-2.0) and [Dear ImGui](https://github.com/ocornut/imgui) (MIT); the CA certificates are Mozilla's (MPL-2.0). Game data and media come from [ScreenScraper](https://www.screenscraper.fr) and its contributors.
