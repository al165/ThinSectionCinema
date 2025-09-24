# ThinSectionCinema

Tile viewer and movie maker for cinematic Thin Section exploration.
Made with [openFrameworks](https://openframeworks.cc/).

![Screenshot of a thin section scan with cross polarisation.](thinsection.png)

Includes [TOML++](https://marzer.github.io/tomlplusplus/index.html) header file for parsing config files.

`ofxFFmpegRecorder` code adapted from [NickHardeman/ofxFFmpegRecorder](https://github.com/NickHardeman/ofxFFmpegRecorder).

## Build instructions

1. clone [openFrameworks](https://github.com/openframeworks/openFrameworks)
2. clone this repository into `openFrameworks/apps/myApps/`
3. clone the following addons into `openFrameworks/addons/`
    - [ofxAnimatable](https://github.com/armadillu/ofxAnimatable)
    - [ofxCsv](https://github.com/paulvollmer/ofxCsv)
    - [ofxJson](https://github.com/jeffcrouse/ofxJSON)
    - [ofxImGui](https://github.com/jvcleave/ofxImGui) (`develop` branch!)
4. in this repo run `make -j8` to build
5. setup a tile scan folder of images and the `config.toml`
6. `./bin/thinsections` to run.

## Project Folder structure

When creating a new new project, the following folder structure is created in `project_root` (defined in the [Config](#config)).

```directory
<project_root>                                <-- Folder of all projects
├── <project_name>                            <-- User defined name of the project
│   ├── Renders                               <-- Directory of renderings
│   │   ├── <project_name>_00.mp4             <-- Sequentially named render
│   │   ├── <project_name>_00_endstate.json   <-- State information of last frame
│   │   ├── <project_name>_00_path.json       <-- Path traced by the camera
│   │   └── ...
│   ├── layout.json                           <-- Layout of tilesets
│   └── sequence.json                         <-- Sequence of events
└── ...

```

## Tile scans folder format

The tiles need to be jpgs in the following folder structure:

```directory
<scan_name>                       <-- The name of the sample 
├── 2.0                           <-- Zoom level (powers of 2)
│   ├── 0.0                       <-- Polarisation angle (degrees)
│   │   ├── <x>x<y>x<w>x<h>.jpg   <-- Tile name
│   │   └── ...
│   ├── 18.0
│   ├── 36.0
│   └── ...
├── 4.0
├── 8.0
├── ...
└── poi.csv                       <-- Coordinates of points of interest
```

The jpgs filenames should contain the position and size (in pixels) for that zoom level.

The (optional) `poi.csv` file should have the following headings: `index, x, y, theta`.
`x` and `y` are normalised floats in `[0, 1]`.
`theta` is optional and unused.

## Config

Copy `config_template.toml` to `bin/data/config.toml` and edit with the following options:

- `scans_root` (required) Root folder of all scans.
- `project_root` (required) Root folder of projects.
- `recording_fps` (optional) Default 60. Frames per second of output recording.

## License

Released under [GNU General Public License v3](LICENSE)
