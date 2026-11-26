# neotunes

neotunes is a TUI for playing music. It was inspired by Jeremy Stevens' terminal-music-player.

## Installation

To install neotunes is very simple.

First, install the ncurses library.

Then, install ffplay.

Next, run these commands:

```console
git clone https://github.com/AI3X123/neotunes.git

cd neotunes

touch src/bluetooth.h # This can be empty; it's fine. You can also place your Bluetooth device's MAC address here if you would like to be able to connect to it from within neotunes.

make
```

Make sure that `XDG_MUSIC_DIR` is set as an environment variable. That is where neotunes will look for mp3 files.

Congratulations! You now have a binary. I recommend moving it to `~/.local/bin`.

## Will it run?

I sure hope so!

I run Arch Linux by the way, so that's the only place that neotunes has been tested. In theory, it should work on any Unix-like operating system with ncurses and ffplay installed.

## License

Freedom! Yeahh! This project is licensed under the GNU General Public License v3.0.
