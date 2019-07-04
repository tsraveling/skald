const SkaldParser = require('../example/src/library/skald-parser');
const FileSystem = require('fs');
const Util = require('util');
const Chalk = require('chalk');

// Convert fs.readFile into Promise version of same
const readFile = Util.promisify(FileSystem.readFile);
const writeFile = Util.promisify(FileSystem.writeFile);

/**
 * Module dependencies.
 */

var writeDirectory = '.';

async function getFile(filename) {
    return await readFile(filename, 'utf8');
}

async function writeObject(object, filename) {
    let string = JSON.stringify(object);
    return await writeFile(writeDirectory + '/' + filename, string, 'utf8');
}

async function processSkaldFile(filename) {

    // Make sure that this is a ska file
    if (filename.substring(filename.length - 4) !== ".ska") {
        console.log(Chalk.red(filename + " is not a Skald file.\n\n"));
        process.exit(1);
    }

    // Get the contents
    let skaldString = await getFile(filename);

    // Process Skald string
    let resultObject = SkaldParser.parseSkaText(skaldString);

    // Get the new filename
    let newFilename = filename.replace(".ska",".json");

    // Save the new file
    try {
        await writeObject(resultObject, newFilename);
    } catch (err) {
        console.log(Chalk.red(err.message));
        process.exit(1);
    }

    console.log(Chalk.green('Compiled ') + Chalk.yellow(filename) + ' to ' +Chalk.blue(newFilename));
}

async function processFiles(files) {

    var numberOfProcessedFiles = 0;

    for (var i = 0; i < files.length; i++) {
        let filename = files[i];
        if (filename.substring(filename.length - 4) === ".ska") {
            await processSkaldFile(filename);
            numberOfProcessedFiles ++;
        }
    }

    return numberOfProcessedFiles;
}

module.exports = () => {
    var program = require('commander');

    program
        .version('0.1.0', '-v, --version')
        .command('compile <filename>')
        .option('-d, --output-directory <dir>', 'Optional argument you can use to specify a different output directory')
        .action((filename, cmd) => {

            outputDirectory = cmd.outputDirectory;
            cmdValue = cmd;
            filenameValue = filename;
        });

    program.parse(process.argv);

    if (typeof cmdValue === 'undefined') {
        console.log(Chalk.red("\n\nPlease use 'skald compile <filename>' in order to compile a Skald file!\n"));
        process.exit(1);
    }

    console.log("\n");

    if (typeof outputDirectory !== 'undefined') {

        // Make sure the directory exists
        if (!FileSystem.existsSync(outputDirectory + '/')) {
            console.log(Chalk.red("Directory '" + outputDirectory + "' doesn't exist\n\n"));
            process.exit(1);
        }

        writeDirectory = outputDirectory;
        console.log("Setting output folder to: " + Chalk.yellow(writeDirectory));
    }

    // Check what's referenced
    FileSystem.stat(filenameValue, (err, stat) => {

        if (stat.isFile()) {

            // If the argument is just a file, compile it
            processSkaldFile(filenameValue).then(() => {
                console.log(Chalk.green("Compilation complete!\n\n"));
                process.exit(1);
            });

        } else if (stat.isDirectory()) {

            // Get the files in the folder and process all of them
            FileSystem.readdir(filenameValue, (err, files) => {
                processFiles(files).then((numProcessed) => {
                    console.log(Chalk.green("Compilation complete! " + numProcessed + " files processed.\n"));
                    process.exit(1);
                });
            });
        }

    });
};