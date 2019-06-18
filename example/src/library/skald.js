import SkaldParser from './skald-parser'

export default class Skald {

    constructor(skaObject) {
        this.skaObject = skaObject;
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

    perform(functionName, state) {

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
            result += component;
        }

        return result;
    }
}