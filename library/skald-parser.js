/*
 *  This parser is separate from the dynamic implementation because it is also used in the Node-based
 *  command line Skald compiler.
 */

/*
let exampleSkaObject = {
    functions : [
        {
            functionName : 'exampleFunction',
            components : [{
                    value: 'stringValue'
                }]
        }
    ]
};
*/

var BracketType = require("./skald-bracket-types");

let verboseLogging = false;
function log(item) {
    if (!verboseLogging)
        return;
    console.log(item);
}

module.exports = class SkaldParser {

    static processOptionalString(optionalString) {

        let optionals = [];

        // Iterate through comma-separated clauses
        let optionalClauses = optionalString.split(",");
        optionalClauses.forEach((optionalClause) => {

            // TODO: Add some linting here

            // Trim the clause
            let trimmedClause = optionalClause.trim();

            // Handle !Bool:
            if (trimmedClause[0]==="!") {
                optionals.push({
                    prop: trimmedClause.substring(1),
                    comparison: true,
                    negative: true
                });

                // Handle inequalities:
            } else if (trimmedClause.search(/!=/s) > -1) {

                // Split the clause
                let parts = trimmedClause.split("!=");

                // Return the optional
                optionals.push({
                    prop: parts[0],
                    comparison: parts[1],
                    negative: true
                });

                // Handle equalities:
            } else if (trimmedClause.search(/=/s) > -1) {

                // Split the clause
                let parts = trimmedClause.split("=");

                // Return the optional
                optionals.push({
                    prop: parts[0],
                    comparison: parts[1],
                    negative: false
                });

                // Handle simple bool check
            } else {
                optionals.push({
                    prop: trimmedClause,
                    comparison: true,
                    negative: false
                });
            }
        });

        return optionals;
    }

    static getOptional(clause) {

        // Try to find the optional clause
        let find = clause.match(/\(.*?\): /s);

        // If there's nothing:
        if (find === null) {
            return {
                value: clause,
                optional: null
            };
        }

        // If there is an optional, get it:
        let optionalString = find[0].substring(1, find[0].length - 3);

        // Pull optionals out of the resulting string
        let optionals = this.processOptionalString(optionalString);

        // If there is, return both:
        let realString = clause.replace(find, "");
        return {
            value: realString,
            optional: { conditions: optionals }
        };
    }

    static parseBracket(bracket) {

        let contents = bracket.substring(1, bracket.length - 1);
        let result = {};

        // Picker
        if (bracket[0] === "[") {
            result.type = BracketType.Draw;

            // Split them up
            result.options = contents.split(" / ").map((option) => {

                // Return an optional if there is one
                let ob = SkaldParser.getOptional(option);
                return (ob.optional === null) ? ob.value : ob;
            });
        }

        // Insert
        if (bracket[0] === "{") {

            // Attempt to get an optional, which defines whether this is an optional or an insert
            let ob = SkaldParser.getOptional(contents);

            if (ob.optional === null) {

                // If there's no optional, it's an insert
                result.type = BracketType.Insert;
                result.insert = contents;

            } else {

                result.type = BracketType.Optional;
                result.optional = ob.optional;
                result.value = ob.value;
            }
        }

        return result;
    }

    static parseStringIntoComponents(source) {

        // Get the results array ready
        var components = [];

        // Set up the working string
        let workingString = source.slice();

        // Then, look for any curly or square bracket arrays
        var brackets = workingString.match(/\[.*?\]|\{.*?\}/g);

        // If there's nothing, just return the source string
        if (brackets === null)
            return [source];

        // Iterate through the results
        for (var index in brackets) {
            if (brackets.hasOwnProperty(index)) {

                // Get the bracket
                let bracket = brackets[index];

                // Split the remaining string by this bracket
                let splitString = workingString.split(bracket);

                // Get the section of the string before the bracket
                let prefix = splitString[0];

                // Add it if it's long enough
                if (prefix.length > 0)
                    components.push(prefix);

                // Parse and add the bracket
                let parsedBracket = SkaldParser.parseBracket(bracket);
                components.push(parsedBracket);

                // Cut the string down
                let chop = prefix.length + bracket.length;

                // Cut the string down
                workingString = workingString.substring(chop);
            }
        }

        // If there's anything left, add that as well
        if (workingString.length > 0) {
            components.push(workingString);
        }

        // Return the result
        return components;

    }

    static parseSkaText(skaText) {

        log(">>> BEGIN PARSE");

        // Stub out the object to use
        var result = {
            functions : []
        };

        // Keep track of function names to prevent duplicates
        var functionNames = [];

        // Get ready for functions
        var workingFunction = null;

        // Get our individual lines
        let lines = skaText.split("\n");

        // Set up our error handler
        var lineNumber = 0;
        let error = (string) => {
            throw new Error(`${string} (line ${lineNumber})\n   -> ${lines[lineNumber]}`);
        };

        // Set up pick and switch systems
        var currentPick = null;
        var currentSwitch = null;
        var currentIf = null;
        var switchProp = null;

        // Step through the lines
        for (var i = 0; i < lines.length; i++) {

            // Store this for error logging
            lineNumber = i;

            // Get the untrimmed base line
            let untrimmedLine = lines[i];

            // Trim the whitespace
            let line = untrimmedLine.trim();

            // Ignore empty lines
            if (line === "") continue;

            // Ignore comments
            if (line[0] === '/' && line[1] === '/') continue;

            // Look for function defines
            if (line.search(/^@define/s) > -1) {

                // If we're not done with the previous one, throw an error
                if (workingFunction !== null)
                    error("Missing @end on the end of the previous function", i);

                // Get the function name
                let functionName = line.replace("@define ", "");

                // Make sure there are no duplicates
                if (functionNames.includes(functionName))
                    error("Duplicate function name: " + functionName);

                // Add it to the list
                functionNames.push(functionName);

                // Create a new function
                workingFunction = {
                    name: functionName,
                    components : []
                };

                continue;
            }

            if (line.search(/^@endpick/s) > -1) {

                // Make sure there's a pick to end
                if (currentPick === null)
                    error("Found an @endpick without an active pick");

                // Look for the end of a pick; if found, end pick and reset to normal mode.
                workingFunction.components.push({
                    type: BracketType.Pick,
                    items: currentPick
                });

                currentPick = null;

                continue;

            } else if (line.search(/^@endswitch/s) > -1) {

                if (currentSwitch === null)
                    error("Found an @endswitch without an active switch");

                // Check for the end of a switch; if found, end switch and reset to normal mode.
                workingFunction.components.push({
                    type: BracketType.Switch,
                    items: currentSwitch,
                    prop: switchProp
                });

                currentSwitch = null;

                continue;

            } else if (line.search(/^@endif/s) > -1) {

                if (currentIf === null)
                    error("Found an @endif without an active if statement");

                // Check for the end of an if; if found, end if statement and reset to normal mode
                workingFunction.components.push({
                    type: BracketType.If,
                    components: currentIf.components,
                    optional: {
                        conditions: currentIf.conditions
                    }
                });

                currentIf = null;

                continue;

            } else if (line.search(/^@end/s) > -1) {

                // Check for the end of the function

                // If there's no function open, throw an error
                if (workingFunction === null)
                    error("Found an @end without an active function");

                // Add the function to the result object and close it up
                result.functions.push(workingFunction);
                workingFunction = null;
                continue;
            }

            // If we're this far and no function has been defined, problem!
            if (workingFunction === null)
                error("Tried to define content outside of a function");

            // Hash marks are for paragraph breaks; they can be followed by comments if desired
            if (line[0] === '#') {
                workingFunction.components.push({
                    type: BracketType.Newline
                });
                continue;
            }

            // Process if statements
            if (currentIf != null) {

                // Parse the line into components
                let parsedComponents = this.parseStringIntoComponents(line + ' ');

                // Add them to the if array
                currentIf.components = currentIf.components.concat(parsedComponents);

                continue;
            }

            // Process Picks
            if (currentPick != null) {
                if (line[0] === '-') {

                    let pickLine = line.substring(2) + ' ';

                    var item = {};

                    // Check for an initial optional
                    if (pickLine.search(/^\(.*?\): /s) > -1) {

                        let parsedOptional = this.getOptional(pickLine);
                        item.optional = parsedOptional.optional;
                        item.components = this.parseStringIntoComponents(parsedOptional.value);
                    } else {
                        item.optional = null;
                        item.components = this.parseStringIntoComponents(pickLine);
                    }

                    // Add the item to the current pick
                    currentPick.push(item);

                    continue;

                } else {

                    // Make sure there's something in here, otherwise throw an error
                    if (currentPick.length === 0)
                        error("Pick started with a non-hypen line.");

                    // Lines that don't include the opening hyphen just get added to the last pick item
                    let last = currentPick[currentPick.length - 1];

                    // Parse the line into components
                    let parsedComponents = this.parseStringIntoComponents(line + ' ');

                    // Add them to the last item
                    last.components = last.components.concat(parsedComponents);

                    continue;
                }
            }

            // Process Switches
            if (currentSwitch != null) {

                // Attempt to split the string by the switch colon
                let switchMatch = line.match(/^.*?: /s);

                // If there are exactly two elements, keep processing the switch
                if (switchMatch !== null) {

                    let switchProp = switchMatch[0].substring(0, switchMatch[0].length - 2);
                    let switchLine = line.replace(switchMatch[0], "") + ' ';

                    // Add the item to the current switch
                    currentSwitch.push({
                        value : switchProp,
                        result : this.parseStringIntoComponents(switchLine)
                    });

                    continue;

                } else {

                    // Make sure there's something in here, otherwise throw an error
                    if (currentSwitch.length === 0)
                        error("Pick started with a non-hypen line.");

                    // Lines that don't include the opening hyphen just get added to the last pick item
                    let last = currentSwitch[currentSwitch.length - 1];

                    // Parse the line into components
                    let parsedComponents = this.parseStringIntoComponents(line + ' ');

                    // Add them to the last item
                    last.result = last.result.concat(parsedComponents);

                    continue;
                }
            }

            // Look for Picks
            if (line.search(/^@pick/s) > -1) {

                // Start the pick
                currentPick = [];
                continue;
            }

            // Look for Switches
            if (line.search(/^@switch\(.*?\)/s) > -1) {

                // Get the property we're switching on
                switchProp = line.substring(8, line.length - 1);

                // Make sure the property is valid
                if (typeof switchProp !== "string" || switchProp.length < 1)
                    error("Tried to switch with an invalid switch property")

                // Start the switch
                currentSwitch = [];
                continue;
            }

            // Look for If statements
            if (line.search(/^@if/s) > -1) {

                // Get the optional clause
                let optionalClause = line.substring(3).trim();

                // Set up the object
                currentIf = {
                    components: [],
                    conditions: this.processOptionalString(optionalClause)
                };

                continue;
            }

            // If no further work needs to be done, parse the inline text and add the results to the components. Add a space to the end.
            let parsedLineComponents = SkaldParser.parseStringIntoComponents(line + ' ');
            workingFunction.components = workingFunction.components.concat(parsedLineComponents)
        }

        // For now just pass the raw text as an example value so we can make sure the wiring's working
        result.exampleValue = "Example";

        log(">>> FINAL RESULT: ");
        log(result);
        log(">>> END PARSE");

        return result;
    }
}