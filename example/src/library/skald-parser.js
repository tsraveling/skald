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

module.exports = class SkaldParser {

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

        console.log(">>> BEGIN PARSE");

        // TODO: Parse the long file string into the JSON object format we will use to run the actual processing

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
        let error = (string, lineNumber) => {
            throw new Error(`${string} (line ${lineNumber})\n   -> ${lines[lineNumber]}`);
        };

        // Set up pick and switch systems
        var currentPick = null;
        var currentSwitch = null;
        var switchProp = null;

        // Step through the lines
        for (var i = 0; i < lines.length; i++) {

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

            // Look for function ends
            if (line.search(/^@end/s) > -1) {

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

            // Process Picks
            if (currentPick != null) {
                if (line[0] === '-') {

                    let pickLine = line.substring(2);

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

                    // If the next line doesn't match the initial hyphen pattern, end pick and reset to normal mode..
                    workingFunction.components.push({
                        type: BracketType.Pick,
                        items: currentPick
                    });

                    currentPick = null;
                }
            }

            // Process Switches
            if (currentSwitch != null) {

                // Attempt to split the string by the switch colon
                let switchElements = line.split(": ");

                // If there are exactly two elements, keep processing the switch
                if (switchElements.length === 2) {

                    // Add the item to the current switch
                    currentSwitch.push({
                        value : switchElements[0],
                        result : this.parseStringIntoComponents(switchElements[1])
                    });

                    continue;

                } else {

                    // If the next line doesn't match the initial hyphen pattern, end pick and reset to normal mode..
                    workingFunction.components.push({
                        type: BracketType.Switch,
                        items: currentSwitch,
                        prop: switchProp
                    });

                    currentSwitch = null;
                }
            }

            // Look for Picks
            if (line.search(/^pick:/s) > -1) {

                // Start the pick
                currentPick = [];
                continue;
            }

            // Look for Switches
            if (line.search(/^switch\(.*?\):/s) > -1) {

                // Get the property we're switching on
                switchProp = line.substring(7, line.length - 2);

                // Make sure the property is valid
                if (typeof switchProp !== "string" || switchProp.length < 1)
                    error("Tried to switch with an invalid switch property")

                // Start the switch
                currentSwitch = [];
                continue;
            }

            // If no further work needs to be done, parse the inline text and add the results to the components. Add a space to the end.
            let parsedLineComponents = SkaldParser.parseStringIntoComponents(line + ' ');
            workingFunction.components = workingFunction.components.concat(parsedLineComponents)
        }

        // For now just pass the raw text as an example value so we can make sure the wiring's working
        result.exampleValue = "Example";

        console.log(">>> FINAL RESULT: ");
        console.log(result);
        console.log(">>> END PARSE");

        return result;
    }
}