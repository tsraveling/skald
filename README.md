# skald
A procedural narrative scripting language and toolset used for interactive fiction projects.

## Compiler

The Node command-line compiler is in the `/compiler` folder. You can install it to your machine by
running `npm link` from within that folder. Basic usage is like this:

```
skald compile some-file.ska
```

This will create `some-file.json` in the same directory, containing the compiled Skald object.

You can also compile all files in a folder like this:

```
skald compile .
skald compile ./some-folder
```

If you like, you can set a custom output directory like this:

```
skald compile . -d ./output
```

This will save the compiled JSON to the `/output` subfolder. If the folder doesn't exist,
the compiler will throw an error.

Note: The compiler overwrites previous JSON files when it runs. It's best to not modify the
JSON output files directly.

## Example Project

Run `npm start` from within the `example/src` directory to run the sample project. It will load `example/public/example.ska` as its source file.

## Library

The library itself is located in `example/src/library`. More modularization, such as npm integration, is TBD.

## Usage

Review the [example.ska](example/public/example.ska) file for more details. The basics:

You define a function using `@define functionName` and `@end`. You can indent however you like.

```
@define helloWorld
    Hello, world!
@end
```

If you've compiled your .ska files into JSON, you can load the resulting JSON object
by using the default constructor of the Skald object, like this:

```
let skaObject = Skald(compiledJsonObject);
```

In addition to using the compiler referenced above, you have the option to form the Skald object dynamically. This is done asynchronously, like this:

```
let skaObject = await Skald.buildDynamically('/example.ska');
```

Once you have the Skald object, you can run a function from it like this:

```
skaObject.perform('functionName', {...});
```

The second argument is a `state` object. So if the object you pass in has,
for instance, `object.firstName: 'tom'`, then you can reference that property as `firstName` in your ska file.

For instance: You can **insert a value** like this:

```
An insert is inserted as-is: {firstName}
```

Use inline **square brackets** separated by ` / ` to add a random picker. Make sure to include spaces!

```
One of the following will be chosen: [option a / option B]
```

You can add conditions to some of your picker options like this:

```
You can also include conditionals: [(pickerSwitch): option a / option b].
```

In this case, `option a` will only be choosing if your `state` object includes a boolean called `pickerSwitch` that is true.

Other conditionals can be set using **negative flags**, **equality**, and **inequality**.

```
Here are some options: [(booleanValue): first option / (!booleanValue): second option / (otherValue=something): third option / (otherValue!=something): fourth option]
```

You can also chain conditionals using commas (again, spaces matter):

```
Here's an extra conditional chain: [(booleanValue, otherValue!=something): first option / second option]
```

You can perform a conditional insert like this:

```
This will only be shown if the state is right: { (shouldShowInclude): Inserted text }
```

You can add comments (on their own lines only at present) using `//`:

```
// This is a comment
```

You can perform a random pick of larger blocks of text by defining `pick:` followed by items that begin with `- `. For example:

```
pick:
    - First option
    - Second option
    - (pickerSwitch): Positive optional
    - (!pickerSwitch): Negative optional [inline option a / inline option b]
```

Note that you can include optionals as well as the inline functions defined above.

You can perform a switch on a given state property like this:

```
switch(switchValue):
    1: The answer was one
    2: The answer was two
    3: The answer was three
    default: There was some other answer.
```

The `default` option will be used if the passed property doesn't match one of the options. If there are no matches at all, `ERR` will be returned.

Enjoy!