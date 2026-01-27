
### 2.3.5 Textual Insertions

If you are inserting sections of just text, you can use `|` (bar) syntax:

```
{some_flag ? You can add a bunch of text here | and this is if some_flag is false}
```

And with switches:

```
{some_val ? [1: here is some text | 2: and some text here | 3: and so on]}
```

And with chance switches:

```
{% ? [take the road more traveled | take the road less traveled]}
```

Note that with the normal syntax you must use either a string, number, variable, or method value, and with the bar syntax, you can only use text -- in other words you can't use text for one option and (for instance) a method for another.

