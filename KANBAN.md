## BACKLOG

- Check for trailing ends, typos, etc.
- Bookmarking system for Skalder that lets you reset from a certain point
- Annotation system for Skalder
- A tunnel system like in Ink; go to another block, then return here when that block hits `-> BACK` or similar.

## FIXES

- FIX: need to be able to set mutations to method outputs -- if this is working it is not resolving properly!
- FIX: Comments don't seem to be detected fully in the middle of a block, ie between beats.
- FIX: Imported variables (>) aren't handled by LSP

## IMPROVEMENTS

- SKALDER: add the ability to annotate runs in a side file, like a qflist
- Numbered enums

## NEXT


## CURRENT

- SKALDER: add the ability to jump in at a specific block
- SKALDER: add testbed support
- LSP: Support imported variable context comments and definition jumps
- Variable typing
- Conditional blocks
- Replace * logic blocks with inline commands, which themselves can be conditioned, e.g. (? check) -> somewhere
- Clean out all logic block references
- Different definition for global definition vs local
- Support else for regular beats
- LSP fully broken on SKD/beach.ska (and most long files)
- Boolean ternary should use a conditional for the lefthand side, vs switch ternaries which should be value-based
- Switch it so mutations, calls etc can be just called inline in a block, and can be directly preceeded by a conditional
- Define a root folder for a Skald project that the LSP and engine can use for GO command. Or even better, just switch this to be relative to the module at hand.
- Skalder "jump in" args that can be hooked up from Neovim
- Declared variables will be exported through to the next module; otherwise they're considered local.

## DOING


## DONE


## ARCHIVE

- Implement state
    * [ ] Double check state setup on engine
    * [ ] Handle variable declarations
- Insertions
- and and or are reading as keywords even in text
- Ternary Insertions
- LSP: Adding a new block doesn't immediately make it available to autocomplete.
    > This seems to be related to whether the block has content. It's possible the parser itself is missing any case where you have a block *not* followed by beats.
- LSP: Would be nice if # triggers autocomplete for unattached blocks
- LSP: Add preceding comments to entity IDE descriptions
- LSP: ~ doesn't trigger autocomplete
- Mutations
    * [x] Equate
    * [ ] Add
    * [ ] Subtract
    * [x] Flip
- Logic beats mess up the treesitter -- choices after that don't show up right.
- In the treesitter, em dashes read as comments (haven't tested the engine yet) when following insertions
    > mia: "All right, {mc} -- I'm sorry if I pushed you."
    > 
