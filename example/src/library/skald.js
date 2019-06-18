export default class Skald {

    constructor(skaContents) {
        this.skaContents = skaContents;
        console.log("Loaded file: \n\n");
        console.log(this.skaContents);

    }

    static async buildDynamically(skaFile) {

        let response = await fetch(skaFile);
        let textData = await response.text();

        let skaObject = new Skald(textData);
        return skaObject;
    }

    perform(func, state) {

        console.log("Perform call:")

        // For now just print the file
        return this.skaContents;
    }
}