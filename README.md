# Auto Deafen

auto deafen mod for geode, no more paying :)

Auto Deafen presses a key or mouse button at percentages you choose during a Geometry Dash level, so you can deafen in Discord at specific points without looking away from the game.

**Windows only.** Auto Deafen uses Windows input functions, so it won't run on macOS or Linux.

Guide: [autodeafen.netlify.app](https://autodeafen.netlify.app/)

## Features

- Set one or more trigger percentages per level, for example `25, 50, 75`.
- Bind any keyboard key or mouse button by clicking **Set Key** and pressing it.
- **Test** button presses your bind once so you can confirm it works.
- Turn triggers on or off for each level, or for all levels in the mod settings.
- Undeafens when you die, complete the level, or quit the level, if you're still deafened.

## Usage

1. Pause a level and click the Auto Deafen icon at the top right.
2. Enter the percentages to trigger on.
3. Click **Set Key** and press the key or mouse button you want to send.
4. Click **Save**.

Settings are saved per level. The key is shared across all levels.

Mouse side buttons work best. Left and right click press at your cursor, so binding those will click in the game.

## Installation

Download `nightcat.auto-deafen.geode` from the [Releases page](https://github.com/Nightcatwastaken/auto-deafen/releases) and drop it into your `geode/mods` folder.

For a step-by-step guide with an FAQ, see [autodeafen.netlify.app](https://autodeafen.netlify.app/).

## Building from source

Install the Geode CLI and SDK, set `GEODE_SDK`, then run:

```
geode build
```

## License

MIT. See [LICENSE](LICENSE).