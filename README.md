# SKALD
A procedural narrative scripting language and toolset used for interactive fiction projects.

## Compiler

TODO: Fill this out.

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