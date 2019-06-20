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

export default class SkaldParser {

    static BracketType = {
        Pick: "pick",
        Insert: "insert",
        Optional: "optional"
    };

    static getOptional(clause) {

        // Try to find the optional clause
        let find = clause.match(/\(.*?\): /s);

        // If there's nothing:
        if (find === null) {
            return {
                str: clause,
                optional: null
            };
        }

        // If there is, return both:
        let realString = clause.replace(find, "");
        return {
            str: realString,
            optional: find[0].substring(1, find[0].length - 3)
        };
    }

    static parseBracket(bracket) {

        let contents = bracket.substring(1, bracket.length - 1);
        let result = {};

        // Picker
        if (bracket[0] === "[") {
            result.type = this.BracketType.Pick;

            // Split them up
            result.options = contents.split(" / ").map((option) => {

                // Return an optional if there is one
                let ob = SkaldParser.getOptional(option);
                return (ob.optional === null) ? ob.str : ob;
            });
        }

        // Insert
        if (bracket[0] === "{") {

            // Attempt to get an optional, which defines whether this is an optional or an insert
            let ob = SkaldParser.getOptional(contents);

            if (ob.optional === null) {

                // If there's no optional, it's an insert
                result.type = this.BracketType.Insert;
                result.insert = contents;

            } else {

                result.type = this.BracketType.Optional;
                result.optional = ob.optional;
                result.value = ob.str;
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

            // If no further work needs to be done, parse the inline text and add the results to the components. Add a space to the end.
            let parsedLineComponents = SkaldParser.parseStringIntoComponents(line + ' ');
            workingFunction.components = workingFunction.components.concat(parsedLineComponents)
        };

        // For now just pass the raw text as an example value so we can make sure the wiring's working
        result.exampleValue = "Example";

        console.log(">>> FINAL RESULT: ");
        console.log(result);
        console.log(">>> END PARSE");

        return result;
    }
}