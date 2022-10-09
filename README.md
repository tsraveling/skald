# SKALD
A procedural narrative scripting language and toolset used for interactive fiction projects.

## Installation

`npm install -g` in the home directory.

## Command Line Tool

Run `skald {source directory} {destination directory}` to run the compiler. JSON files will be
written into the destination directory, overwriting anything in it already. Errors
will be printed to the console if found. The source directory will not be modified in any way.

## Language

- Start a section tag with `#`
- Comment with `//` on any line
- A **text block** starts with a **character tag**, like this:
  ```
  bill: Shut up, Ted.
  ```
- A choice starts with `>`
- Choices and text blocks can have the following additions, inline or on the next lines:
    - Terniary: `? year < 1950`. Block or choice does not show up if terniary is false.
    - Mutation: `~ year = 1250`. Sets variable to the provided value.
    - Transition: `-> some-tag`. Moves the player to that section tag.
    - Interface method: `: call-method`. Sends the signal out to the outside game.