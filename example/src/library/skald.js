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

    perform(func, state) {

        console.log("Perform call:")

        // For now just print the file
        return this.skaObject.exampleValue;
    }
}