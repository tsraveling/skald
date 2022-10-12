const chalk = require('chalk')
const parser = require('./skald-parser')
const prompt = require('prompt-sync')({sigint: true});
const fs = require('fs')

const printHelp = () => {
    console.log(chalk.green("1-n:"), "Select choices")
    console.log(chalk.green("back"), "Back one step")
    console.log(chalk.green("set {input} = {value}"), "Sets the value of a given input to the value on the right side of the equal sign")
    console.log(chalk.green("goto {section tag}"), "Jump to the supplied section tag")
    console.log(chalk.green("status"), "Prints the value of all saved inputs")
}

const checkConditions = meta => {
    return true;
}

const colorFunc = [chalk.green, chalk.yellow, chalk.blue];

const printBlocks = (characters, section) => {
    for (block of section.blocks) {
        if (!checkConditions(block))
            continue;
        if (block.type === 'attributed') {
            console.log(">>>", characters)
            let index = characters.findIndex(char => char === block.tag);
            let i = index % colorFunc.length;
            console.log(colorFunc[i](block.tag + ":"), block.body);
        }
    }
}

const printChoices = section => {
    if (section.choices.length > 0) {
        console.log(".... choices")
    } else {
        console.log(chalk.gray("[Continue]"))
    }
}

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
    const { characters, inputs, signals, sections } = json;

    // Print help
    console.log("\nType HELP for help\n")

    // Enter the game loop
    let currentSection = sections[0];
    let inputValues = inputs.reduce((ob, input) => ({...ob, input: ''}), {})
    let val ='';
    while (val !== 'exit') {
        let nextSection = null;
        if (currentSection) {
            console.log(chalk.gray('#' + currentSection.tag));
            printBlocks(characters, currentSection);
            printChoices(currentSection);
        }
        val = prompt('> ').toLowerCase();
        console.log("");
        if (val === 'help')
            printHelp();
        if (val === 'status')
            console.log(inputValues)
        console.log("");

        // Default logic
        if (!nextSection) {
            let nextIndex = sections.findIndex(s => s === currentSection)
            if (nextIndex >= 0 && nextIndex < sections.length) {
                console.log(chalk.gray(' -> auto-transition to next block'))
                nextSection = sections[nextIndex + 1];
            }
        }

        // Restart otherwise
        if (!nextSection) {
            console.log(chalk.bgRed("Reached end of file with no transition, restarting."));
            nextSection = sections[0]
        }
        currentSection = nextSection;
    }
}