## BACKLOG

- Boolean ternary should use a conditional for the lefthand side, vs switch ternaries which should be value-based
- Check for trailing ends, typos, etc.
- LSP: Would be nice if # triggers autocomplete for unattached blocks
- Would be nice to have a conditional "set" where you could if and endif a set of beats on a single conditional.
- Switch it so mutations, calls etc can be just called inline in a block, and can be directly preceeded by a conditional

## FIXES

- Test logic blocks thoroughly -- something there doesn't feel right.

## IMPROVEMENTS

- SKALDER: add the ability to annotate runs in a side file, like a qflist
- SKALDER: add the ability to jump in at a specific block
- SKALDER: add testbed support
- LSP: Add preceding comments to entity IDE descriptions

## NEXT


## CURRENT


## DOING


## DONE

- Implement state
    * [ ] Double check state setup on engine
    * [ ] Handle variable declarations
- Insertions
- In the treesitter, em dashes read as comments (haven't tested the engine yet) when following insertions
    > mia: "All right, {mc} -- I'm sorry if I pushed you."
    > 
- and and or are reading as keywords even in text
- Logic beats mess up the treesitter -- choices after that don't show up right.
- Ternary Insertions
- Mutations
    * [x] Equate
    * [ ] Add
    * [ ] Subtract
    * [x] Flip
- LSP: Adding a new block doesn't immediately make it available to autocomplete.
    > This seems to be related to whether the block has content. It's possible the parser itself is missing any case where you have a block *not* followed by beats.
- LSP: ~ doesn't trigger autocomplete
