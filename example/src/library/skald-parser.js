/*
 *  This parser is separate from the dynamic implementation because it is also used in the Node-based
 *  command line Skald compiler.
 */

export default class SkaldParser {

    static parseSkaText(skaText) {

        // TODO: Parse the long file string into the JSON object format we will use to run the actual processing

        // Stub out the object to use
        var ob = {};

        // For now just pass the raw text as an example value so we can make sure the wiring's working
        ob.exampleValue = skaText;

        return ob;
    }
}