const fs = require('fs');
const util = require('util');
const chalk = require('chalk');
const parser = require('./skald-parser')

// Convert fs.readFile into Promise version of same
const readFile = util.promisify(fs.readFile);
const writeFile = util.promisify(fs.writeFile);
/**
 * Module dependencies.
 */

let inputDirectory, outputDirectory;
let numProcessed = 0;

function processFile(filename) {

    process.stdout.write("- " + chalk.yellow(filename))
    const content = fs.readFileSync(filename, {encoding: 'utf8'});
    const json = parser.parse(content);
    const output = JSON.stringify(json);
    const outputFilename = filename.replace(inputDirectory, outputDirectory).replace('.ska', '.json')
    fs.writeFileSync(outputFilename, output, {encoding: 'utf8'});
    process.stdout.write(" ->" + chalk.green(outputFilename) + "\n")
    numProcessed += 1;
}

function processPath(path) {
    console.log(chalk.cyan("PROCESS DIR: " + path))
    const files = fs.readdirSync(path);
    for (let i = 0; i < files.length; i++) {
        let filename = path + '/' + files[i];
        const stats = fs.statSync(filename);
        const isFolder = stats.isDirectory();
        const isYAML = !isFolder && filename.substring(filename.length - 4) === ".ska"
        if (isYAML)
            processFile(filename);
        else if (isFolder) {
            fs.mkdirSync(filename.replace(inputDirectory, outputDirectory));
            processPath(filename);
        } else {
            console.log("- " + filename + " (ignored)")
        }
    }
}

module.exports = () => {
    const program = require('commander');

    program
        .version('0.3.0', '-v, --version')
        .arguments('<input> <output>')
        .action((input, output) => {
            inputDirectory = input;
            console.log(chalk.green(">>> " + input))
            outputDirectory = output;
            console.log(chalk.yellow(">>> " + output))
        });

    program.parse(process.argv);

    if (!fs.existsSync(outputDirectory + '/')) {
        console.log(chalk.red("Directory '" + outputDirectory + "' doesn't exist\n\n"));
        process.exit(0);
    }

    console.log("Writing output to " + chalk.green(outputDirectory));

    // Delete everything in current output directory
    const files = fs.readdirSync(outputDirectory);
    for (let i=0; i<files.length; i++) {
        const delPath = outputDirectory + '/' + files[i];
        fs.rmSync(delPath, { recursive: true, force: true });
    }

    // Get the files in the folder and process all of them
    processPath(inputDirectory)
    console.log(chalk.green("Compilation complete! " + numProcessed + " files processed.\n"));
};