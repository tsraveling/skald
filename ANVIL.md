## FIXES

- FIX: need to be able to set mutations to method outputs -- if this is working it is not resolving properly!
- FIX: Comments don't seem to be detected fully in the middle of a block, ie between beats.
- LSP: count a_b as one word, not two

## NEXT

- LSP updates
- Test engine and polish
- Skald-Godot updates
- Treesitter updates

## TO DO

- @let clause
- Structural refactor
- Move operations inline
- Move choices into choice groups
- Handle child blocks
    * [ ] Map type of prefix to type of block
    * [ ] Use e.g. parent.child.grandchild as key, so that transitions can be done using string manipulation
- Updated transitions
    * [ ] support nested blocks in transitions
    * [ ] support GO insertions
- Conditional Blocks
    * [ ] Set up in data
    * [ ] Set up parsing for nested systems
    * [ ] Handle in parser
    * [ ] Handle in engine
- Strong typing
- Module variables vs ad hoc variables
- Codices
    * [ ] Infrastructure for secondary file type
    * [ ] @globals in grammar
    * [ ] @methods in grammar
    * [ ] Support in data
    * [ ] Support in engine infrastructure
    * [ ] Build out global state system
- Support full conditional statements in ternaries
- Support custom engine insertion

## DOING


## DONE

