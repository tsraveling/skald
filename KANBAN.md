## BACKLOG

- Boolean ternary should use a conditional for the lefthand side, vs switch ternaries which should be value-based
- Check for trailing ends, typos, etc.

## FIXES

- In the treesitter, em dashes read as comments (haven't tested the engine yet) when following insertions
    > mia: "All right, {mc} -- I'm sorry if I pushed you."
    > 
- and and or are reading as keywords even in text
- Logic beats mess up the treesitter -- choices after that don't show up right.

## NEXT


## CURRENT

- Ternary Insertions
- Mutations
    * [x] Equate
    * [ ] Add
    * [ ] Subtract
    * [x] Flip

## DOING


## DONE

- Implement state
    * [ ] Double check state setup on engine
    * [ ] Handle variable declarations
- Insertions
