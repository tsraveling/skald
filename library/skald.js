var SkaldParser = require("./skald-parser.js");
var BracketType = require("./skald-bracket-types");

module.exports = class Skald {

    constructor(skaObject) {
        this.skaObject = skaObject;
        this.currentState = {};
    }

    static async buildDynamically(skaFile) {

        // Get the contents of the file
        let response = await fetch(skaFile);
        let textData = await response.text();

        // Parse the contents into a useable Skald Object:
        let skaObject = SkaldParser.parseSkaText(textData);

        // Generate the Skald instance using the object, and return it
        // Note: in production, you'll want to do this directly from a json file.
        return new Skald(skaObject);
    }

    drawFrom(array) {
        return array[Math.floor((Math.random() * array.length))];
    }

    valueOfOptional(prop) {

        // Split the prop into parts
        let propParts = prop.split(".");

        // Set up the var
        var stateProperty = this.currentState;

        // Iterate down the line
        propParts.forEach((part) => {
            stateProperty = stateProperty[part];
        });

        return stateProperty;
    }

    validateOptional(optional) {

        // Iterate through the conditions and check them out one by one
        for (var i = 0; i < optional.conditions.length; i++) {

            let condition = optional.conditions[i];

            // Get the value of the property
            let stateProperty = this.valueOfOptional(condition.prop);

            // Perform the comparison
            if (condition.negative) {
                if (stateProperty === condition.comparison) {
                    return false;
                }
            } else {
                if (stateProperty !== condition.comparison) {
                    return false;
                }
            }
        }

        return true;
    }

    compileComponentsIntoString(components) {

        // Compose the result item into a string
        var result = "";
        for (var i = 0; i < components.length; i++) {
            result += this.performComponent(components[i]);
        }
        return result;
    }

    performComponent(component) {

        // A string just is what it is
        if (typeof component === "string")
            return component;

        // If it's not an object, return ERR
        if (typeof component !== "object")
            return "ERR";

        // If it's an inline draw, return one of the available options
        if (component.type === BracketType.Draw) {

            let deck = [];

            // Build the deck out of qualified options
            component.options.forEach((option) => {

                // If it's a string, just add it
                if (typeof option === "string")
                    deck.push(option);
                else {
                    if (this.validateOptional(option.optional))
                        deck.push(option.value);
                }
            });

            // If there are no options, return ERR
            if (deck.length === 0)
                return "ERR";

            // Otherwise, draw and return!
            return this.drawFrom(deck);
        }

        // If it's an insert, do the insert
        if (component.type === BracketType.Insert) {

            return this.valueOfOptional(component.insert);
        }

        // If it's an optional, process and continue
        if (component.type === BracketType.Optional) {
            let val = this.validateOptional(component.optional);
            if (val)
                return component.value;
            else
                return "";
        }

        // If it's an If statement, process it and return the resulting string
        if (component.type === BracketType.If) {

            if (this.validateOptional(component.optional)) {
                return this.compileComponentsIntoString(component.components);
            }
        }

        // If it's a full pick, pick a line, process it, and return the resulting string
        if (component.type === BracketType.Pick) {

            let deck = [];

            // Build the deck out of qualified options
            for (let i = 0; i < component.items.length; i++) {

                let item = component.items[i];

                if (item.optional === null)
                    deck.push(item);
                else {
                    if (this.validateOptional(item.optional))
                        deck.push(item);
                }
            }

            // If there are no options, return ERR
            if (deck.length === 0)
                return "ERR";

            // Otherwise, draw an item
            let resultItem = this.drawFrom(deck);

            // Compose the result item into a string (with a final space)
            return this.compileComponentsIntoString(resultItem.components) + ' ';
        }

        // If it's a switch, find the match and return the resulting string. ERR if there is no match.
        if (component.type === BracketType.Switch) {

            // Get the value to check against
            var comparison = null;
            try {
                comparison = this.valueOfOptional(component.prop).toString();
            } catch (err) {
                return "ERR: No property <" + component.prop + ">";
            }

            // Prepare a default option
            var defaultOption = null;

            // Iterate through the items
            for (let switchIndex = 0; switchIndex < component.items.length; switchIndex++) {

                let switchItem = component.items[switchIndex];

                // Check for default
                if (switchItem.value === "default") {
                    defaultOption = switchItem.result;
                }

                // Check for a match ...
                if (comparison === switchItem.value) {

                    // Compose the result item into a string (with a final space)
                    return this.compileComponentsIntoString(switchItem.result) + ' ';
                }
            }

            // If there's a default, use that
            if (defaultOption !== null) {
                return this.compileComponentsIntoString(defaultOption) + ' ';
            }
        }

        // If it's something else, return an empty string
        return "";
    }

    perform(functionName, state) {

        // Retain state
        this.currentState = state;

        // First, try to find the function
        let f = this.skaObject.functions.find((func) => func.name === functionName);

        // If there's no function called that, throw an error
        if (f === undefined)
            throw new Error("Couldn't find function: " + functionName);

        console.log(f);

        // Prepare the result
        let result = [];
        var currentString = "";

        // Loop through the function's components and assemble a result
        for (var i = 0; i < f.components.length; i++) {

            // Get the component at this index
            let component = f.components[i];

            // Check for newlines, which break the result into string arrays
            if (component.type === BracketType.Newline) {
                result.push(currentString);
                currentString = "";
                continue;
            }

            // Process the component
            currentString += this.performComponent(component);
        }

        result.push(currentString);
        return result;
    }
}