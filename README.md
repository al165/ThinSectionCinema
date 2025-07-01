# ThinSectionCinema

Tile viewer and movie maker for cinematic Thin Section exploration.
Made with [openFrameworks](https://openframeworks.cc/).

Includes [TOML++](https://marzer.github.io/tomlplusplus/index.html) header file for parsing config files.

## Building

1. clone [openFrameworks](https://github.com/openframeworks/openFrameworks)
2. clone this repository into `openFrameworks/apps/myApps/`
3. clone the following addons into `openFrameworks/addons/`
    - [ofxAnimatable](https://github.com/armadillu/ofxAnimatable)
    - [ofxFFmpegRecorder](https://github.com/Furkanzmc/ofxFFmpegRecorder)
4. in this repo run `make -j8` to build, and `./bin/thinsections` to run.

## Tile format

The tiles need to be jpgs in the following folder structure:

```directory
- <scan_name>                  <-- The name of the sample
  - 2.0                        <-- Zoom level (powers of 2)
    - 0.0                      <-- Polarisation angle (0 - 180 degrees)
        - <x>x<y>x<w>x<h>.jpg  <-- Tile name
        - ...
    - 18.0
    - 36.0
    - ...
  - 4.0
  - 8.0
  - ...
```

## Config

Copy `config_template.toml` to `bin/data/config.toml` and edit with the following options:

- `scans_root` (required) Root folder of all scans. Must end in '/';
- `scan_name` (required) Name of the scan folder to load.
- `secondary_name` (optional) Name of the second scan folder to load (e.g. for transitions).

## License

Released under [GNU General Public License v3](LICENSE)
