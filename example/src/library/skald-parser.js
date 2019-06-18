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

        // Set up our inline parser
        let parseInline = (line) => {
            var components = [];

            // TODO: parse more from here
            // For now we're just putting the whole line in
            components.push(line);

            return components;
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
            let parsedLineComponents = parseInline(line + ' ');
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