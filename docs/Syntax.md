# 1. Introduction

## 1.1 Basic Syntax

Skald is designed to be readable and flexible, and adaptable to whatever spacing and organizational conventions you prefer. That being said, there are a few rules:

### 1.1.1 Identifiers

Identifiers must start with `a-z`, `A-Z`, or `_`. After the first character, they can also use numbers. Identifiers are used for tags (2.1) and variables (3.1.3).

### 1.1.2 Whitespace and Indentation

For the most part, Skald is whitespace agnostic. You don't need to include blank newlines between beats (2.1) for instance. However, you should indent operations under a beat or logic beat.

## 1.2 Line Comments

You can put a comment on any blank line, or the end of any logic (ie, non-beat) line, with `--` (like in Lua):

```
-- This is a comment on its own line

*
  ~ some_var = 10  -- This comment works too
```

## 1.3 Embedded Comments

You can put embedded comments in beats by using square brackets:

```
This is a beat [but this text will not appear in it] -- cool, huh?
```

This only works in beats though, don't try it anywhere else.

# 2. Blocks

## 2.1 Block Tags and Beats

Core dialogue is composed of **Blocks**, which in turn is composed of **Beats**. A block is defined with a tag (see Identifiers, 1.1.1) like this:

```
#some-block

This is a beat

This is a second beat

tutorial: This is an *attributed* beat.
```

Skald will feed you a block's content sequentially as beats (you have control over whether this is automated or requires e.g. hitting spacebar to read the next beat).

A beat that is just a line or paragraph of text is a **narration beat**, ie, without attribution.

A beat that begins with a tag (again, kebab-case preferred) and a colon, like `tutorial:` or `second-bandit:` is **attributed**. You will receive the attribution tag (`tutorial` or `second-bandit`) via the [[Skald API]] and can interpret it however you like.

### 2.1.1 Block Entry

The first beat in a block is the entry point; narration will proceed linearly until it hits a set of choices.

### 2.1.2 Block Resolution

A block can resolve in one of three ways:

1. A set of **choices** (see 1.2).
2. A **transition** (see 3.1.1 below).
3. A **module outcome**, e.g. a module transfer or dialogue conclusion (see 4.1 below)

Failure to resolve a block will result in an **error**.

## 2.2 Choices

At the end of a block is a set of **choices** (at least one). A choice looks like this:

```
> This is a choice
```

An undecorated choice like this one will simply pass the player to the next block in the file, or to the next beat if the choice is in the middle of a block.

### 2.2.1 Choice Redirects

You can do a one-line choice **redirect** using arrow notation:

```
> This is another choice -> target-block
```

This will immediately move the player to the beginning of `target-block`.

### 2.2.2 Choice Operations

You can invoke a number of **operations** (see 3.1 below) by indenting on the line below the choice:

```
> This is a third choice with some other stuff
  :call_method
  -> other-target-block
```

Other effect syntax is discussed in 3.1 below.

Finally, you can have a conditional choice, that only appears if a given condition is met:

```
> (? some_condition) This is a conditional choice
  -> another-target-block
```

See 3.2 **conditionals**, below, for conditional-specific syntax.

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

### 2.3.4 Chance Ternaries

You can use a **chance ternary** to inject an option at random:

```
I prefer {% ? "dark" : "light"} roast coffee.
```

You can also make a **chance switch ternary**:

```
Her pet is a {% ? ["dog", "cat", "donkey"]} -- these choices are evenly weighted
```

You can weight options like this:

```
Her pet is a {% ? ["dog", 3:"cat", 5:"donkey"]} -- This pet has a 5/9 chance of being a donkey
```

Finally, you can mark some options as conditional with paren syntax:

```
Her pet is a {% ? ["dog", (?is_cat_person):"cat", 3(?is_pirate): "parrot"]}.
```

In this example, if `is_pirate` is not set but `is_cat_person` is, it's a coin-flip between dog and cat. If she is a pirate, however, the pet has a 3/5 chance of being a parrot. If neither flag is set, it will always be "dog".

## 2.4 Logic Beats

Sometimes in a narrative flow, you may want to perform logic that is not directly attached to a text beat. To do that, you can use a **logic beat:**

```
*                 -- this is a logic beat
  :call_method
  ~ some_var = 3
```

### 2.4.1 Conditional Logic Beats

A logic beat can also be conditional:

```
* (? some_condition)
  -> first_block
```

This will only execute the attached operations / transition if the condition is met.

### 2.4.2. "Else" Logic Beats

If you immediately follow a conditional logic block with `* (else)`, that will do something if the previous condition is *not* met:

```
* (? brave)
  -> road_less_traveled
* (else)
  -> road_more_traveled
```

This works for regular beats as well:

```
(? did_win) And they lived happily every after!
(else) They unfortunately did not live happily ever after.
```

This can be useful for places where the narrative branches due to choices made previous to the current block.

### 2.4.3 Inline Logic Beats

If a logic beat has only one operation, it can be written inline:

```
* ~ money -= 5
* (? brave) -> road_less_traveled
* (else) -> road_more_traveled
```

This matters in terms of execution order. For instance, if you have this:

```
The shopkeeper takes your money.
    ~ money -= 20

* shopkeeping += 1
```

The signal for the money mutation would be sent as that text appears; the shopkeeping +1 mutation would be sent after the player "continues" the conversation.

## 2.5 Chance Blocks

If successive blocks are prefixed with (%), the engine will pick one to present at random:

```
(%) Hi there!
(%) Why hello.
(%) Sup?
```

You can mark some chances as conditional. Conditionals will only be included in the "draw pile" if the conditional is met:

```
(%) Hello, stranger.
(%) Greetings!
(%? has_met) Nice to see you again.
```

Chance blocks can be weighted with an integer prefix:

```
(2%) Ahoy!      -- This has a 2/3 chance of printing
(%) Hoy!        -- This has a 1/3 chance of printing
```

As with any other kind of block, logic can be added:

```
You roll the dice.

(5%) You miss, and pay up.
    ~ money -= 100
(%) You hit six!
    ~ money += 300
```

This also works with logic blocks (`*`):

```
(%) * -> find_way
(3%) * -> get_lost
```

# 3. Logic

## 3.1 Operations

**Operations** are called by choices (1.2), inline on blocks (XX), or directly from **logic blocks** (XX).

A single operation can be called inline:

```
#block-tag

This is a beat. :with_a_method

> This is a choice -> with-a-transition
```

Multiple operations are indented under the calling element, forming an **operation group**:

```
> This is a choice with an operation group
  :call_method()
  ~set_var = 2
  -> transition-block
```

### 3.1.1 Transitions

A **transition** will finish the other operations in its group, then immediately send the player to the indicated block. If that block doesn't exist, an error will be thrown.

### 3.1.2 Methods

A **method** communicates via the api to call a method on the game side:

```
> Do something
  :some_method()
```

This will inform the game that `some_method` needs to be processed. A method may optionally return a boolean value for use in a conditional (see 3.2.3):

```
> (? :check_method) Optional choice
```

Note that `check_method` must return either `true` or `false` on the API side.

For now, methods cannot be used for anything other than booleans, or used in any other operations (e.g. you can't add a method response to a variable).

Finally, methods can include simple **arguments** of the three types (string, int, or bool):

```
:some_method(1)             -- Calls `some_method` with (int)1
:some_method(1, "hello")    -- Calls with two args, 1 and "hello"
:some_method(false)         -- Calls with bool false
```

Or a variable:

```
:some_method(is_traveling)  -- Calls with the contents of the "is_traveling" variable
```

Methods are not strongly typed, and arguments are not counted. From the Skald side, you can currently send any number of any of the three types of arguments to any method -- so script with care!

### 3.1.3 Variables

A **variable** is simply a key in game state. So e.g. `(? health < 50)` will query (via the API) the game state to check if the `health` value is less than 50.

Game state exists in a layer between the Skald module and the game; both can query and mutate narrative state directly.

All variables used inline must be initialized at the top of a file, before any testbeds (XX) or blocks:

```
-- ModuleOne.ska

~ num_var = 2
~ str_var = "Hello, World!"
~ bool_var = true

#first-block

The game is afoot ...
```

### 3.1.4 Variables Across Modules

Variables are scoped within a narrative chain: so if a variable is defined in `ModuleOne.ska`, which transitions to `ModuleTwo.ska`, that variable will still be available. However, you must declare it as inherited like this:

```
< inherited_num = 2
< inherited_string = "Something"
< bool_from_last_module = false
```

This syntax informs the compiler to expect a variable from the previous module; it will throw a runtime error if that variable is not found. The initialization value is **only used for testing** (see XX below).

Likewise, if you declare a module-specific variable that is already scoped, the API will throw a runtime error that you are overwriting something.

### 3.1.5 Variable Types

Variables can either be **boolean**, **integer**, or **string**.

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

You can pass any variable to any method as an argument:

```
:call_method(str_var) -- Send str_var to the `call_method` method via the API

(? check_method(num_var, bool_var)) This beat will fire if `check_method`, with the two given arguments, returns true.
```

## 3.2 Conditionals

**Conditionals** can determine if a choice will appear, if a beat will be included, or if a logic beat (XX) will be executed.

```
(? bool_var) This beat will only appear (and attached operations will only be run) if bool_var is true.

* (? bool_var)
  :call_method() -- Operations on this logic beat will only occur if
                 -- bool_var is true.

> (? bool_var) This choice will only be enabled if bool_var is true.
```

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


# 4. Modules

A Skald **module** is a Skald file. The API opens with a file reference to a .ska file. It will load and parse the file, then continue through. As stated in 3.1.3 above, **variables** must be declared at the top of a module, unless those variables are already "in-scope", ie, declared in an a module earlier in the **narrative chain**.

There are two **module-level operations**: `GO` (4.1.1) and `END` (XX). Module operations can be identified in .ska files by that all-caps syntax.

## 4.1 The Narrative Chain

The narrative chain is composed of a series of modules loaded linearly. For instance, lets say you start in `YellowWood.ska`. Let's say that in that module, there's a branching choice that can transition to two different modules: `RoadLessTraveled.ska` and `RoadMoreTraveled.ska`. If you transition to the former, the narrative chain will then be `YellowWood.ska -> RoadLessTraveled.ska`. Any variables defined in `YellowWood` will be available in `RoadLessTraveled`.

### 4.1.1 Module Transitions

You can transition to a new module (.ska file) with the `GO` command. This can be used anywhere a normal operation can. For instance:

```
> Take the road less traveled
  ~robert_frost=true
  GO RoadLessTraveled.ska
```

This will cause the API to load that new file and enter it at the top, unless you define an **entry point** (4.1.2).

### 4.1.2 Module Entry Points

By default, a module will enter at its first block. However, the API will allow you to specify any entry point you like, which allows the game developer to programmatically inject the player anywhere in the story they like.

In addition, a module transition can define an entry point like this:

```
> Take the road less traveled
  ~robert_frost=true
  GO RoadLessTraveled.ska -> still_in_the_woods
```

This will drop the player into the `RoadLessTraveled` module, starting at the `#still-in-the-woods` block.

As with in-module transitions, all other operations in the group will be executed before the transition occurs.

**Note:** Conflicting transitions will throw an error:

```
> This is a badly formed operation group:
  -> in-module-block
  GO AnotherModule.ska             -- This will throw an error
```

### 4.1.3 Conclusions

Every story must end. Every narrative chain must end as well; otherwise, you'll get an error. You can **conclude** a narrative chain with the `END` operation, like this:

```
> I'm done!
  END
```

This will inform the API that the interaction is concluded. You can use this for something like a short conversation with an NPC, or at the very end of a story in a piece of interactive fiction.

You can also send an **argument** to the conclusion, as with methods:

```
> I'm done with a string value!
  END "some string value"
> I'm done with a bool!
  END false
> I'm done with a variable!
  END some_variable
```

### 4.1.4 Narrative Completion

Every module must either transition to another module, or exit. 

## 4.2 File Structure

The **Skald root** will be determined by the code invoking the Skald API. You can use **folders** within this, like this:

```
> Let's go to a subfolder!
  GO some_folder/SubfolderModule.ska
```

Paths are always relative to the Skald root, so if you are in `some_folder/SubfolderModule.ska` above, you would still transition to a sibling module like this:

```
> Go to a sibling
  GO some_folder/SiblingModule.ska  -- Even though we are in the same folder
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

