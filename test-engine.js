const chalk = require('chalk')
const parser = require('./skald-parser')
const prompt = require('prompt-sync')({sigint: true});
const fs = require('fs')

const printHelp = () => {
    console.log(chalk.green("1-n:"), "Select choices")
    console.log(chalk.green("back:"), "Back one step")
    console.log(chalk.green("re:"), "Reprint the current block")
    console.log(chalk.green("set {input} = {value}:"), "Sets the value of a given input to the value on the right side of the equal sign")
    console.log(chalk.green("goto {section tag}:"), "Jump to the supplied section tag")
    console.log(chalk.green("status:"), "Prints the value of all saved inputs")
    console.log(chalk.green("testbed {testbed tag}:"), "Loads the specififed testbed")
    console.log(chalk.green("testbeds"), "Lists available testbeds and their values")
}

const checkConditions = (meta, state) => {
    return true;
}

const colorFunc = [chalk.green, chalk.yellow, chalk.blue];

const processMeta = (meta, state) => {
    let newState = {...state};

    // Encode transitions
    if (meta.transition)
        newState.currentSection = meta.transition;

    // Print signals, as the test engine does not use them direclty
    for (let signal of meta.signals) {
        console.log(chalk.bgYellow(chalk.black("---->", signal)))
    }

    // Handle mutations
    for (let mutation of meta.mutations) {
        console.log(chalk.gray(mutation.input), chalk.gray(mutation.operator), mutation.value)
        const {input, operator, value} = mutation;
        switch (operator) {
            case "=":
                // Handle boolean flip
                if (value === '!') {
                    if (typeof value !== 'boolean')
                        console.log(chalk.bgRed('Error: Trying to flip (= !) a non-boolean state variable:' + chalk.black(input)))
                    else
                        newState[input] = !newState[input];
                } else {
                    newState[input] = value;
                }
                break;
            case "+=":
                if (!newState[input]) {
                    console.log(chalk.yellow("Warning: trying to add to value not in state:"), chalk.bgYellow(chalk.black(mutation.input)));
                    newState[input] = 0;
                }
                // NEXT: complete mutations here
        }
    }
    return newState;
}

const printBlocks = (characters, section, state) => {
    let newState = {...state}

    for (block of section.blocks) {

        // Ensure we can see the block
        if (!checkConditions(block.meta, newState))
            continue;

        // Apply mutation
        newState = processMeta(block.meta, newState);

        // Print attributed blocks
        if (block.type === 'attributed') {
            let index = characters.findIndex(char => char === block.tag);
            let i = index % colorFunc.length;
            console.log(colorFunc[i](block.tag + ":"), block.body);
        }
    }

    return newState;
}

const printChoices = (section, state) => {
    if (section.choices.length > 0) {
        for (let i=0; i<section.choices.length; i++) {
            let choice = section.choices[i];
            if (checkConditions(choice.meta, state))
                console.log(chalk.green((i + 1)+":"), choice.body)
            else
                console.log(chalk.gray("x " + (i + 1) + ": " + choice.body))
        }
    } else {
        console.log(chalk.gray("[Continue]"))
    }
}

const loadTestbed = (testbed, inputs) => ({
    ...inputs,
    ...testbed.sets
})

exports.runTest = filename => {

    // First, load the file
    const stats = fs.statSync(filename);
    if (stats.isDirectory()) {i
        console.log(chalk.bgRed("This is a directory. You can only test individual files."))
        return;
    }
    if (filename.substring(filename.length - 4) !== ".ska") {
        console.log(chalk.bgRed("This is not a Skald file."))
        return;
    }
    const content = fs.readFileSync(filename, {encoding: 'utf8'});

    // Parse the content
    console.log(chalk.bgGreen(chalk.black("Parsing for test: " + filename)));
    const json = parser.parse(content)
    if (!json) return;
    const { characters, inputs, signals, testbeds, sections } = json;

    // Print help
    console.log("\nType HELP for help\n")

    // Initialize game state
    let gameState = inputs.reduce((ob, input) => ({...ob, [input]: ''}), {})
    gameState.currentSection = sections[0].tag;
    let lastStates = [];

    const updateState = state => {
        lastStates.push({ ...gameState});
        gameState = state;
    }

    const stepBack = (steps = 1) => {
        if (lastStates.length > 0) {
            for (let i=0; i<steps; i++) {
                gameState = lastStates.pop();
            }
        } else
            console.log(chalk.bgRed("State history is empty."))
    }

    // Load the first testbed if there is one
    if (testbeds.length > 0) {
        gameState = loadTestbed(testbeds[0], gameState)
        console.log(chalk.gray("Initialized state with testbed:"), chalk.green(testbeds[0].tag));
    }

    let val ='';
    let skipProcess = false
    while (val !== 'exit') {
        console.log("");

        // Get the section from state
        let currentSection = sections.find(sec => sec.tag === gameState.currentSection);
        if (!currentSection) {
            console.log(chalk.bgRed("No section found for tag:", chalk.black(gameState.currentSection) + ", exiting."))
            break;
        }

        // If we aren't skipping processing this loop, process all blocks and print output.
        if (!skipProcess) {
            console.log(chalk.gray('#' + currentSection.tag));
            let newState = printBlocks(characters, currentSection, gameState);
            updateState(newState);
            printChoices(currentSection, gameState);
        }

        // Get user input
        skipProcess = false
        val = prompt('> ').toLowerCase().trim();
        console.log("");

        // Handle choices
        if (!isNaN(val)) {
            let choiceNumber = parseInt(val);
            if (choiceNumber <= currentSection.choices.length) {
                let selectedChoice = currentSection.choices[choiceNumber - 1];
                if (checkConditions(selectedChoice.meta, gameState)) {
                    updateState(processMeta(selectedChoice.meta, gameState));
                } else {
                    console.log(chalk.bgRed("Your game state is not qualified for that choice:\n"), selectedChoice.meta.conditions);
                }
                continue;
            }
        }

        // Command: get help
        if (val === 'help') {
            printHelp();
            skipProcess = true;
            continue;
        }

        // Command: print game state
        if (val === 'status') {
            console.log(gameState)
            skipProcess = true;
            continue;
        }

        // Command: repeat current block
        if (val === 're') {
            stepBack();
            continue;
        }

        // If you hit enter on a section with no choices, go to the next section
        if (currentSection.choices.length < 1) {
            let nextIndex = sections.findIndex(s => s === currentSection)
            if (nextIndex >= 0 && nextIndex < sections.length - 1) {
                console.log(chalk.gray(' -> auto-transition to next block'))
                updateState({
                    ...gameState,
                    currentSection: sections[nextIndex + 1].tag
                });
                continue;
            } else {
                console.log(chalk.bgRed("Reached end of file with no transition, exiting."));
                break;
            }
        }

        // Invalid input
        console.log(chalk.gray("Invalid input."))
        skipProcess = true;
    }
}