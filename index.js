const fs = require('fs');
const util = require('util');
const chalk = require('chalk');
const parser = require('./skald-parser')
const package = require('./package.json')
const yesno = require('yesno');
const testEngine = require('./test-engine')

// Convert fs.readFile into Promise version of same
const readFile = util.promisify(fs.readFile);
const writeFile = util.promisify(fs.writeFile);
/**
 * Module dependencies.
 */

let inputDirectory, outputDirectory;
let numProcessed = 0;
let errorFiles = [];

function processFile(filename) {
    console.log(chalk.bgBlue("Processing file: " + chalk.yellow(filename)))
    const content = fs.readFileSync(filename, {encoding: 'utf8'});
    const json = parser.parse(content);
    if (json) {
        const output = JSON.stringify(json);
        const outputFilename = filename.replace(inputDirectory, outputDirectory).replace('.ska', '.json')
        fs.writeFileSync(outputFilename, output, {encoding: 'utf8'});
        numProcessed += 1;
    } else {
        errorFiles.push(filename);
        process.stdout.write(chalk.gray("x Skipping file.\n"));
    }
}

let fileConflits = [];
function checkPath(path) {
    let isEmpty = true;
    const files = fs.readdirSync(path);
    for (let i = 0; i < files.length; i++) {
        let filename = path + '/' + files[i];
        const stats = fs.statSync(filename);
        const isFolder = stats.isDirectory();
        const isJson = !isFolder && filename.substring(filename.length - 5) === ".json"
        if (isFolder) {
            if (!checkPath(filename)) {
                fileConflits.push(filename)
                isEmpty = false;
            }
        } else if (!isJson) {
            fileConflits.push(filename)
            isEmpty = false
        }
    }
    return isEmpty;
}

function processPath(path) {
    const files = fs.readdirSync(path);
    for (let i = 0; i < files.length; i++) {
        let filename = path + '/' + files[i];
        const stats = fs.statSync(filename);
        const isFolder = stats.isDirectory();
        const isSka = !isFolder && filename.substring(filename.length - 4) === ".ska"
        if (isSka)
            processFile(filename);
        else if (isFolder) {
            fs.mkdirSync(filename.replace(inputDirectory, outputDirectory));
            processPath(filename);
        } else {
            console.log("- " + filename + " (ignored)")
        }
    }
}

module.exports = async () => {
    const program = require('commander');
    const test = program.command('test')

    program
        .version('0.3.0', '-v, --version')
        .arguments('<input> <output>')
        .action((input, output) => {
            inputDirectory = input;
            outputDirectory = output;
        });

    test
        .arguments('<input>')
        .action((input) => {
            testEngine.runTest(input);
            process.exit(0);
        })

    program.parse(process.argv);

    if (!fs.existsSync(outputDirectory + '/')) {
        console.log(chalk.red("Directory '" + outputDirectory + "' doesn't exist\n\n"));
        process.exit(0);
    }

    console.log(chalk.bgGreen(chalk.black("-------------------------")))
    console.log(chalk.bgGreen(chalk.black("-- SKALD PARSER v" + package.version + " --")))
    console.log(chalk.bgGreen(chalk.black("-------------------------")))
    console.log("Writing output to", chalk.green(outputDirectory), "\n");

    // Check the destination first
    let isEmpty = checkPath(outputDirectory)
    if (!isEmpty) {
        console.log(chalk.bgRed("Destination directory has non-JSON files in it:"))
        for (conflict of fileConflits) {
            console.log(chalk.red(" - " + conflict));
        }
        const cont = await yesno({
            question: 'All extant files and folders in destination directory will be erased. Are you sure you want to continue? (Y/n)',
            defaultValue: 'y'
        });
        if (!cont)
            process.exit(0);
    }

    // Delete everything in current output directory
    const files = fs.readdirSync(outputDirectory);
    for (let i=0; i<files.length; i++) {
        const delPath = outputDirectory + '/' + files[i];
        fs.rmSync(delPath, { recursive: true, force: true });
    }

    // Get the files in the folder and process all of them
    processPath(inputDirectory)
    console.log(chalk.green("\nCompilation complete! " + numProcessed + " files processed.\n"));
    if (errorFiles.length > 0) {
        console.log(chalk.bgRed(errorFiles.length,"files with errors found:"));
        console.log(chalk.red(errorFiles.map(file => ' - ' + file).join('\n')))
    }
};