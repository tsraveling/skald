# TODO

Next steps for developing the Skald interactive fiction scripting language.

## On Deck

Get the optional + state and inserts working

## Current Objectives

- Define inline random swaps
    - Optional tags
- Define inserts
- Optional include block
- Pick sections
- Switch sections

## Next Phase

- Set up a Node command line compiler. [Commander might be useful.](https://www.freecodecamp.org/news/writing-command-line-applications-in-nodejs-2cf8327eee2/)
    - Isolate the JS required to interpret a .ska file into a JS object
    - Initialize a command line tool using the above tutorial
    - Add Skald project file to define all of the .ska files in use
    - Build individual file compiler
    - Add project file support

## Future / Nice to Have

- Better markup tools
- IntelliJ plugin to suport Ska files
- C# interpreter
- Swift interpreter