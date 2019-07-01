import SkaldParser from './skald-parser'

export default class Skald {

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

    performComponent(component) {

        // A string just is what it is
        if (typeof component === "string")
            return component;

        // If it's not an object, return ERR
        if (typeof component !== "object")
            return "ERR";

        // If it's an inline draw, return one of the available options
        if (component.type === SkaldParser.BracketType.Draw) {

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
        if (component.type === SkaldParser.BracketType.Insert) {

            return this.valueOfOptional(component.insert);
        }

        // If it's an optional, process and continue
        if (component.type === SkaldParser.BracketType.Optional) {
            let val = this.validateOptional(component.optional);
            if (val)
                return component.value;
            else
                return "";
        }

        // If it's a full pick, pick a line, process it, and return the resulting string
        if (component.type === SkaldParser.BracketType.Pick) {
            // findme
        }

        // If it's something else, return ERR
        return "ERR";
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
        var result = "";

        // Loop through the function's components and assemble a result
        for (var i = 0; i < f.components.length; i++) {

            // Get the component at this index
            let component = f.components[i];

            // TODO: All the random logic goes here
            // For now, simply append the component to the result
            result += this.performComponent(component);
        }

        return result;
    }
}