# 1. Introduction

## 1.1 Basic Syntax

Skald is designed to be readable and flexible, and adaptable to whatever spacing and organizational conventions you prefer. That being said, there are a few rules:

### 1.1.1 Identifiers

Identifiers must start with `a-z`, `A-Z`, or `_`. After the first character, they can also use numbers. Identifiers are used for tags (2.1) and variables (3.1.3).

### 1.1.2 Whitespace and Indentation

For the most part, Skald is whitespace agnostic. You don't need to include blank newlines between beats (2.1), for instance. However, you should indent operations under choices (2.2).

## 1.2 Line Comments

You can put a comment on any blank line, or the end of any logic (ie, non-beat) line, with three hypens: `---`

```
--- This is a comment on its own line

~ some_var = 10  --- This comment works too
```

Note: we use three hyphens instead of two (like in Lua) to keep from conflicting with em dashes and to better distinguish the comment from normal beat text.

## 1.3 Embedded Comments

You can put embedded comments in beats by using curly brackets and comment syntax:

```
This is a beat [but this text will not appear in it] -- cool, huh?
```

This only works in beats though, don't try it anywhere else!

# 2. Blocks

## 2.1 Block Tags and Beats

Core dialogue is composed of **Blocks**, which in turn is composed of **Beats**. A block is defined with `# ` followed by a tag (see Identifiers, 1.1.1) like this:

```
# some_block

This is a beat

This is a second beat

tutorial: This is an *attributed* beat.
```

Skald will feed you a block's content sequentially as beats (you have control over whether this is automated or requires e.g. hitting spacebar to read the next beat).

A beat that is just a line or paragraph of text is a **narration beat**, ie, without attribution.

A beat that begins with a tag (same rules as block tags in 1.1.1) and a colon, like `tutorial:` or `second_bandit:` is **attributed**. You will receive the attribution tag (`tutorial` or `second_bandit`) via the [[Skald API]] and can interpret it however you like.

### 2.1.1 Child Blocks

You can define a child block using `##`, and a grandchild with `###`, like this:

```
# parent

This is the parent block.

## child

This is the child block.

### grandchild

This is the grandchild block
```

Top level / parent tags have to be unique per file. Child tags have to be unique within that parent, and so on

This allows you to e.g.:

```
-> parent.child.grandchild
-> parent2.child.grandchild --- a diffent child and grandchild of a different parent.
```

For more, and for relative transitions, see 

### 2.1.1 Block Entry

The first beat in a block is the entry point; narration will proceed linearly until it hits a set of choices.

### 2.1.2 Block Resolution

A block can resolve in one of three ways:

1. A set of **choices** (see 1.2).
2. A **transition** (see 3.1.1 below).
3. A **module outcome**, e.g. a module transfer or dialogue conclusion (see 4.1 below)

Failure to resolve a block will result in an **error**.

## 2.2 Choices

Some beats are followed by one or more **choices**. These are selected by players, and will execute any redirects or logic attached to them.

```
> This is a choice.
```

An undecorated choice like this one will simply pass the player to the next block in the file, or to the next beat if the choice is in the middle of a block.

### 2.2.1 Anchor Beats

TODO: Clear this out

### 2.2.2 Choice Redirects

You can do a one-line choice **redirect** using arrow notation:

```
> This is another choice -> target-block
```

This will immediately move the player to the beginning of `target_block` if the player selects that choice.

### 2.2.3 Choice Operations

You can invoke a number of **operations** (see 3.1 below) by indenting on the line below the choice:

```
> This is a third choice with some other stuff
  :call_method
  -> other-target-block
```

Other effect syntax is discussed in 3.1 below.

Finally, you use an inline conditional, that only appears if a given condition is met:

```
> (? some_condition) This is a conditional choice
  -> another-target-block
```

Or a conditional block, for a group of choices:

```
@if some_condition
> First conditional choice
    -> some move
> Second conditional choice
    :some_method_call()
@end
> Nevermind.
```

In this scenario, if `some_condition` is false, the player will only get the `Nevermind.` choice.

See 3.2 **conditionals**, below, for conditional-specific syntax.

### 2.2.4 Default Continuation

If the player selects a choice without a redirect, the cursor will move to the next beat. If the choices are at the end of a block, or if a block ends without a redirect, the cursor will move to the first beat of the next block.

If the cursor reaches the end of the file without an explicit `END` (4.1.3), an error will be thrown.

If the cursor reaches a beat with choices, but none of those choices are available due to conditions, the cursor will accept any question index input and move on to the next beat as if no choices were present at all.

## 2.3 Insertions

### 2.3.1 Direct Insertion

You can insert the value of a variable into any text (choice or beat) using **curly braces**, like this:

```
This is some beat text! Our variable "player_name" is currently set to {player_name}.
```

### 2.3.2 Boolean Ternaries

You can do a **boolean ternary insertion** like this:

```
Right now it is {is_day ? "light" : "dark"}
```

The left-hand side of the ternary is just a basic conditional (3.2), and can use `and` / `or`, parentheses, and so on. The right hand side can either be a string, or a variable that resolves to a string, like this:

```
The value of either a variable or a method is {some_condition ? a_variable : "some text"}
```

### 2.3.3 Switch Ternaries

You can also do a **switch ternary** like this:

```
{pet_type ? [1:"dog", 2:"cat", 3:"donkey"]}
```

The switch value must match the type of the check value:

```
{str_val ? ["one": 1, "two": 2, "three": 3]}
```

# 3. Logic

## 3.1 Operations

**Operations** either occur between beats in a block, or are indented under a choice.

```
This is a beat

~ some_variable = 5

Another beat
```

Triggered by selecting a choice:

```
> This is a choice with an operation group
  :call_method()
  ~set_var = 2
  -> transition-block
```

### 3.1.1 Transitions

A **transition** will finish the other operations in its group, then immediately send the player to the indicated block. If that block doesn't exist, an error will be thrown.

```
-> some_block
```

#### 3.1.1.1 Relative Transitions

Suppose a structure like this:

```
# main
## first
### a
## second
### b

# other_main
## first
### a
## second
```

You can navigate to a child of the current block like this:

```
# main
-> .first --- navs to `## first` block.

## first
-> .a --- navs from child `first` into grandchild `a`.
```

You can navigate to a sibling like this:

```
# main
## first
## second
-> -first -- navs to main.first
```

You can navigate to this block's parent like this:

```
# main
## first
-> ^ --- navs back to main
```

You can combine these as well. For instance, to an aunt:

```
# main
## first
## second
### a
### b
-> ^-first --- navs back to parent's sibling, `first`.
```

Or to a cousin:

```
# main
## first
### a
## second
### a
-> ^-first.a --- navs to cousin main.first.a
```

Or a grandchild:

```
# main
-> .first.a --- navs to main.first.a
## first
## a
```

And of course you can navigate absolutely:

```
# main
## first
### a
## second
### a
# other
-> main.first.a
```

### 3.1.2 Methods

A **method** communicates via the api to call a method on the game side:

```
:some_method()
```

Methods interfaces are registered in the **codex** (see TBD). Trying to use a non-registered method will throw a warning.

A method may optionally return a boolean value for use in a conditional (see 3.2.3):

```
> (? :check_method()) Optional choice

@if :check_method()

This beat will fire after check_method processes through, if the result is true.

And then this one will follow normally, also if true.

@end

And this one will follow regardless.
```

Finally, methods can include simple **arguments** of the three types (string, int, or bool). These types will be defined in the codex, and trying to use the wrong type will throw an error:

```
:a_method(1)             -- Calls `some_method` with (int)1
:b_method(1, "hello")    -- Calls with two args, 1 and "hello"
:c_method(false)         -- Calls with bool false
```

Or a variable:

```
:some_method(is_traveling)  -- Calls with the contents of the "is_traveling" variable
```

If that variable is global or module-scoped, and is the wrong type, you will see a warning up front. If it is an ad hoc variable and of the wrong type, you will encounter a runtime rror. See the next section (3.1.3) for more details.

### 3.1.3 Variables

A **variable** is a key in game state that holds either an int, a float, a boolean, or a string.

There are three **scopes** of variable:

**Global** variables are defined in the codex. They are intended to be owned by the game side, and can optionally be marked as read-only. Trying to write to a read only variable will throw an error. Examples would be player name, current level, and so on. They are strongly typed.

**Module** variables are defined at the top of a .ska file. They are strongly typed, and will be **pushed** to any modules navigated to via `GO` (4.1.1).

```
--- Items in the following section will be "pushed" through GO operations.
--- It should come before any narrative blocks.
@let 
  num_var  = 2
  str_var  = "Hello, World!"
  bool_var bool = true --- you can provide a type with a default, but it's optional.
  int_var int --- if no default is provided, provide type!
@end

#start

The game is afoot ...
```

If you are using Neovim or VSCode to write Skald, the LSP will detect when a module is pushed to by another, and include its module-scoped variables in autocomplete. If you expect to receive a push from a module that hasn't been fully written yet, you can mark it for the LSP like this:

```
@receive subfolder/scene.ska
```

This can go before or after `@let` or `@testbed` blocks, but must come before narrative blocks.

See Module Transitions 4.1.1 for more details on how module-scoped variables are passed through `GO` statements.

**Ad Hoc** variables are defined inline, off-the-cuff, and are not syntactically typed (aka you don't have to do e.g. `~ ad_hoc_var int = 10`. However they are typed to their initial value at creation, so you can't do this:

```
~ ad_hoc = 10 --- first time variable is used creates ad_hoc int
~ ad_hoc = "no-op" --- this will throw an error because you can't change types

They are also not pushed through to following modules; they are intended for immediate conversational state:

```
--- Ad hoc variables

(? temp_check) This will resolve to false, because it hasn't been set yet.

~ temp_check = true

(? temp_check) This beat will show up now.
```

Global variables are defined in the .codex file (4.2). 
### 3.1.4 Variable Types

Variables can either be **boolean**, **integer**, **float** or **string**:

```
@let
  health int
  speed  float
  alive  bool
  name   string_
@end
```

TODO: add details on conditionals between ad hoc vars.

### 3.1.6 Variable Operators

You can **set** all variables, and **mutate** bool and integer variables:

```
> Mutate things ...
  ~ num_var += 1                -- You can add to variables
  ~ num_var -= 1                -- or subtract
  ~ bool_var =!                 -- You can toggle a bool
  ~ num_var = 30                -- or set numbers
  ~ bool_var = false            -- or set bools directly
  ~ str_var = "Goodbye, World!" -- or set strings
```

You can also use **same-type** values on the righthand side:

```
> Mutate by another variable ...
  ~ num_var += another_num_var
  ~ str_var = another_str_var
```

More advanced operators, like multiplication, division, or string concatenation, are currently not supported.

### 3.1.7 Variables as Arguments

You can pass a variable to a method as an argument, if it is the right type:

```
:call_method(str_var) -- Send str_var to the `call_method` method via the API

(? check_method(num_var, bool_var)) This beat will fire if `check_method`, with the two given arguments, returns true.
```

For ad hoc variables, you are responsible for sending the right type!

```
--- where: do_damage(amount int) action

These are ad hoc operations:
~ a = 10
~ b = "cow"
:do_damage(a) --- does 10 damage
:do_damage(b) --- throws an error!
```

## 3.2 Conditionals

**Conditionals** can determine if a choice will appear, if a beat will be included, or if an operation will be executed. There are two types of conditionals, **inline conditionals** and **conditional clauses**.

**Inline conditionals** live at the start of a beat, before an operation, or right after the `>` carat on a choice:

```
(? bool_var) This beat will only appear (and attached operations will only be run) if bool_var is true.

(? bool_var) :call_method() -- only called if bool_var is true

> (? bool_var) This choice will only be enabled if bool_var is true.
```

**Conditional blocks** wrap any of these:

```
@if condition

Condition is true!

@elseif sky="red"

Seems apocalyptic
> Continue
> Flee

@else

Everything seems fine!

@end

(? check) this is still a one-liner.
```

- `@if` starts the clause, and is followed by a condition statement (see the following sections).
- `@elseif` will perform an additional conditional check only if the preceding one evaluates to false.
- `@else` will fire if all preceding conditional sections are false.
- `@end` closes a conditional clause; everything after this is back to normal.

**Rules**:

1. Conditional clauses must end by the end of the block; a dangling open conditional clause will throw an error.
2. Conditional clauses cannot be nested (use `@elseif` instead).
3. Conditional clauses can wrap anything inside a block (beats, operations, choices) but cannot wrap a block tag or any top-of-file matter.

### 3.2.1 And / Or

Multiple conditions can be used with the `and` and `or` keywords:

```
(? bool_var and other_bool) This beat will only occur if both values are true

(? bool_var or other_bool) This beat will occur if either value is true
```

You can also nest parentheses:

```
(? bool_var and (a_bool or b_bool)) This text will occur if bool var is true, and either a_bool or b_bool is true.
```

### 3.2.2 Value Checks

You can check **equality** on any variable:

```
(? bool_var = true or str_var = "Test" or num_var = 3) This beat will show up if any of these variables match the given variable.
```

Likewise, you can check **not equals**:

```
(? bool_var != true or str_var != "Test" or num_var != 3) This beat will show up if any of these variables *don't* match the given variable.
```

Bools can be **false-checked** or **true-checked** directly:

```
(? !bool_var) This beat will show up if bool_var is false.
(? bool_var) This beat will show up if bool_var is true.
```

You can compare integers using **more and less than operators**:

```
(? num_var > 10) More than ten
(? num_var >= 10) More than or equal to ten
(? num_var < 10) Less than ten
(? num_var <= 10) Less than or equal to ten
```

### 3.2.3 Methods as Conditions

Any method can be used as a **boolean check**. If the method does not return a response, that will be treated as a **false** value.

```
(? :check_method()) -- This will fire if check_method() returns true via the API.
```

Likewise the false boolean check syntax applies to methods:

```
(? !:check_method()) -- This will fire if check_method() returns false via the API.
```

You can also pass arguments to conditional methods:

```
(? :check_method(some_variable)) -- This will pass `some_variable` to `check_method`, where it can be used in determining the calculation
```

# 4. Modules and Codices

A Skald **module** is a Skald file. The API opens with a file reference to a .ska file. It will load and parse the file, then continue through. As stated in 3.1.3 above, **variables** must be declared at the top of a module, unless those variables are already "in-scope", ie, declared in an a module earlier in the **narrative chain**.

A Skald **codex** is a whole Skald project (or subproject in a larger game), and contains your **game interface definition**. See 4.2 for more details on this.

But first, let's stick to the module level. There are two **module-level operations**: `GO` (4.1.1) and `EXIT` (XX).

```
> Continue on
    GO next_module.ska
> Finish it
    EXIT
```


## 4.1 The Narrative Chain

The narrative chain is composed of a series of modules loaded linearly. For instance, lets say you start in `yellow_wood.ska`. Let's say that in that module, there's a branching choice that can transition to two different modules: `road_less_traveled.ska` and `RoadMoreTraveled.ska`. If you transition to the former, the narrative chain will then be `yellow_wood.ska -> road_less_traveled.ska`. Any modules-scoped variables (3.1.3) defined in `yellow_wood` will be available in `road_less_traveled`.

### 4.1.1 Module Transitions

You can transition to a new module (.ska file) with the `GO` command. This can be used anywhere a normal operation can. For instance:

```
> Take the road less traveled
  ~robert_frost=true
  GO road_less_traveled.ska
```

This will cause the API to load that new file and enter it at the top, unless you define an **entry point** (4.1.2).

Module paths are relative to the **mother codex** (4.2). So if your file structure looks something like this:

```
/game
  /skald
    main.codex
    /chapter1
       a.ska
    /chapter2
       b.ska
```

And you wanted to move from `a` to `b`, you would do:

```
> Continue!
    GO chapter2/b.ska
```

In other words, relative to main.codex.

**Module-scoped variables** (3.1.3) are pushed through `GO` statements. So if you have a module `A.ska` that defines these:

```
--- A.ska
@let
  a_one = 1
  a_two = 2
@end
```

And then that module at some point does `GO B.ska`, then `B.ska` will be able to read and mutate the a_one variable.

Here's a subtle rule: if a module is pushed into this one, this one will push it on. But if we come in from a different origin that *doesn't* define that variable, it will be treated as a local ad-hoc variable.

So let's say we have `A.ska` above which pushes to `B.ska`, but we *also* have `A2.ska` which pushes to B. In the case where we start at `A2` and `GO` -> `B`, `a_one` and `a_two` will still be module-defined (pushed onward on `GO`), but will be defined as the zero value (0, 0.0, false, "") unless locally set in `@let` (see next section, 4.1.1.1).

A series of modules that are connected with `GO` or `@receive` are called **threads**. 

#### 4.1.1.1 Thread Variable Definitions

There are a few rules that govern how you can define thread variables, if you are trying to do so in more than one place.

1. Within a thread, you cannot define the same variable with two different types. If `A.ska` above defines `a_one = 1`, then `a_one` is an `int`; trying to define it in `A2.ska` as `string` will throw an error.
2. You can, however, redefine it locally with the same type, e.g. we could do `a_one = 10` in B's `@let` clause. If the player enters a thread at this module, or we come to this module from one where the variable was not defined, it will be initialized to this value.
3. If a previous module sets or mutates a module-scoped variable, though, that variable will override local definitions. So in the above cases:
    - As `A.ska` defines `a_one` as `1`, then a definition of B will be overwritten by 1.
    - But if we come in from `A2.ska`, which doesn't define `a_one`, then `a_one` will be initialized to `10`.

Finally, if we set a variable *before* any module defines it at module scope, it will be treated as an ad-hoc variable and discarded. So in the example above, if at some point in `A2` we do `~ a_one = 100`, that value will be useable within A2, but will be discarded when transitioning to B, which will then redefine a_one as a module variable initialized with 10.

**TL;DR**: modules **push module-scoped variables** forward through `GO` statements; they are not transmitted in any other way.

If this sounds like it will cause you issues, because your modules have multiple entry points and you want shared states, that's probably a good time to use **global variables** (4.2.2).


### 4.1.2 Conclusions

Every story must end. Every narrative chain must end as well; otherwise, you'll get an error. You can **conclude** a narrative chain with the `END` operation, like this:

```
> I'm done!
  EXIT
```

This will inform the API that the interaction is concluded. You can use this for something like a short conversation with an NPC, or at the very end of a story in a piece of interactive fiction.

You can also send an **argument** to the conclusion, as with methods:

```
> I'm done with a string value!
  EXIT "some string value"
> I'm done with a bool!
  EXIT false
> I'm done with a variable!
  EXIT some_variable
```

You can send any of the normal values to an EXIT statement: ints, floats, strings, booleans, or variables.

### 4.1.3 Narrative Completion

Every module must either transition to another module, or exit. If the engine reaches the end of the file without hitting an EXIT or GO, it will throw an "Unexpected end of of file (EOF)" error.

## 4.2 Codices

Every Skald file belongs to a **codex**. A codex is any file that ends in `.codex`, e.g. `main.codex` or `dialogue.codex`. All .ska files that are in the same folder as the codex, or subfolders of the codex, are considered "children" of it, and it is the **mother codex** of those files.

On the code side (see TBD), a Skald engine instance will start by loading up a specific codex. Each engine instance is independent; this is useful if e.g. you want to control character dialogue in one set of data, and a tutorial walkthrough in another.

A **codex file** looks like this:

```
--- main.saga
--- Contains the main game stories.

@methods
  --- Will get added to hover text
  --- a: describe a param
  call_something(a string) int

  do_something() action -- action types do not return a value.
@end

@globals
  --- hover text here too
  @readonly player_name string = "Test"

  --- will be zero default if not defaulted
  player_level int
@end
```

You will notice that there are two sections: one for **method definitions**, and one for **global definitions**. See 4.2.1 and 4.2.2 respectively for further details on each.

However, if your game is fairly simple and you don't intend to use either feature, it is actually sufficient to simply include an empty file (or one with only comments); this still serves as the mother to its skald files and as a reference point for the project to load modules from.

### 4.2.1 Method Definitions

The method section is wrapped like this:

```
@methods
@end
```

Each method is composed of the method name, optional arguments, and a **type**.

Arguments must be typed: `int`, `float`, `string`, `bool`.

Methods have an additional type: `action`. An action method type means no return value is expected.

Finally, any comments you add immediately above a method definition, without any spaces between, will be available in the hover text for that method in VSCode or Neovim.

Putting it all together:

```
--- Hover text!
hesome_method(some_argument int) action
```

This method takes a single int argument, and does not return a value, and will show "Hover text!" when you hover the method name in a Skald file.
```
:some_method(1) --- this works!
:some_method("one") --- this doesn't, because the argument is a string
(? :some_method(1)) -> somewhere --- this doesn't work, because action types don't return anything, and so can't be used in conditionals.
```

### 4.2.2 Global Variable Definitions

The globals section is wrapped like this:

```
@globals
@end
```

Globals are typed, with the usual four types:

```
health int
speed  float
name   string
alive  bool
```

Globals can be marked as read only (engine will throw an error if you try to mutate them from a Skald module):

```
@readonly name string --- I can't be changed from Skald
```

They can also be given defaults, which will be used e.g. in Skalder when testing:

```
health int = 100
```

# 5. Testing

You can test a Skald file using the **command-line tool**, or from a custom integration within your game using the "test" flag on the API module loader.

## 5.1 Test Values

A module loaded in "test mode" will initialize imported variables using the "test value", as described in 3.1.4:

```
~ module_value = 3     -- Will initialize as this in gameplay
< inherited_value = 9  -- Will only initialize as this in test mode
```

## 5.2 Testbeds

You may want to try a few different variations on your start conditions, e.g. one where the player has chosen to be a rogue, and another where they have chosen a wizard. For that, you have **testbeds**.

A testbed is declared after the initial variable definition, and before the first block, and looks like this:

```
-- Variables inherited from the CharacterCreation module
-- Default testing values are for a rogue character
< class = "Rogue"
< sneakiness = 10
< magic = 2

-- But we also need to be able to test out a wizard!
@testbed WizardMode
  class = "Wizard"
  sneakiness = 2
  magic = 10
@end
```

You can define as many testbeds as you like. If a variable is not set within a testbed, the test value (5.1) will be used.

You can also overwrite local module variables using testbeds. This is useful to try some different configurations without committing them, or for testing entrypoints (5.3) that may be deeper into the module.

## 5.3 Entrypoints

Finally, you can define any block as an entrypoint for testing. So if you wanted to jump into a dungeon crawl as a wizard, right after the player enters the throne room, you might invoke the command line tool like this:

```
> skalder DungeonCrawl.ska testbed=WizardMode start=throne_room
```

You can also start over from any block, using any testbed, directly from the Skalder interface.

