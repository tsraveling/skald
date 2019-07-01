import React from 'react';
import './App.css';

// Within this repo we're pulling the library from the parent folder; preferably you'd use npm or put the files somewhere within
// your extant project files.
import Skald from './library/skald'

class App extends React.Component {

    async getSkaldResult() {

        // Create the State object
        let stateObject = {
            shouldShowInclude: true,
            insert: 'a value to insert',
            pickerSwitch: true,
            secondSwitch: false,
            specificValue: 'aValue',
            switchValue: 2,
            childObject: {
                one: 1,
                two: 2,
                three: 3
            }
        };

        // Generate the Skald object from the file
        // Note: this is how we dynamically load a ska file so that we can do hotloading. For a production app you'd want to simply
        // import the compiled JSON file directly and use it as is.
        let skaObject = await Skald.buildDynamically('/example.ska');

        // Perform the action
        return skaObject.perform('introParagraph', stateObject);
    }

    constructor() {
        super();
        this.state = {
            skaldString : ''
        };
    }

    componentDidMount() {
        this.getSkaldResult()
            .then((skaldResponse) => {
                console.log("Received response: " + skaldResponse);
                this.setState({
                    skaldString: skaldResponse
                });
            });
    }

    render() {
        return (
            <div className="container">
                {this.state.skaldString}
            </div>
        );
    }
}

export default App;
