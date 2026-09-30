# Wolfie3D

A code-for-fun project to build a small Wolfenstein 3D-style game and learn how its rendering works.

The core focus is implementing proper raycasting and texture mapping for walls. The project starts from an empty repository; the programming language, libraries, and platform have yet to be chosen.

## Intended scope

- A small, explorable world inspired by Wolfenstein 3D.
- A raycasting renderer that projects a 2D map into a first-person view.
- Textured walls.

Further features will be decided as the project develops. There are no build or run instructions yet.

## Build

### Debug build

```shell
mkdir build
cmake -S . -B build/ -DCMAKE_BUILD_TYPE=Debug
cmake --build ./build
```

### Release build

```shell
mkdir build
cmake -S . -B build/ -DCMAKE_BUILD_TYPE=Release
cmake --build ./build
```