const chalk = require('chalk');

exports.parse = (content) => {

    console.log(chalk.green("\n\n============\n\n"))

    // Templates
    const makeMeta = () => ({
        mutations: [],
        conditions: [],
        transitions: [],
        signals: [],
        isEnd: false
    });

    // Get our individual lines
    let lines = content.split("\n");
    let lineNumber = 0;
    let numberOfErrors = 0;
    let numberOfWarnings = 0;
    let characters = [];

    let sections = [];
    let currentSection = null;
    let currentBlock = null;
    let currentChoice = null;
    let currentMeta = null;

    const logger = {
        error: (...args) => {
            numberOfErrors += 1;
            console.log(chalk.bgRed(lineNumber + " ERROR:"), ...args)
        },
        warn: (...args) => {
            numberOfWarnings += 1;
            console.log(chalk.bgYellow(lineNumber + " WARNING:"), ...args)
        },
        log: (...args) => {
            console.log(chalk.gray(lineNumber + " WARNING:"), ...args)
        }
    }

    // Step through the lines
    for (let i = 0; i < lines.length; i++) {

        // Store this for error logging
        lineNumber = i + 1;

        // Get the untrimmed base line
        let untrimmedLine = lines[i];

        // Trim the whitespace
        let line = untrimmedLine.trim();

        // Ignore empty lines
        if (line === "") continue;

        // Ignore comments
        if (line[0] === '/' && line[1] === '/') continue;

        // Remove other comments
        line = line.replace(/\/\/(.*)$/s, '');

        // >>>
        logger.log(line)

        // Check for section header
        if (line[0] === '#') {
            if (line.search(/^#[a-zA-Z0-9\-]+$/s) > -1) {
                const tag = line.replace('#', '');
                currentSection = {
                    tag,
                    blocks: [],
                    choices: []
                }
                sections.push(currentSection);
            } else {
                logger.error("Improperly formatted section tag: \n > ", chalk.red(line))
            }
        }

        if (!currentSection) {
            logger.error("No open section on line: \n > ", chalk.red(line))
            continue;
        }

        // Look for function defines
        const blockMatch = line.match(/^[a-zA-Z0-9]+:/s)
        if (blockMatch) {
            if (currentSection.choices.length > 0) {
                logger.error("All blocks should come before choices in a given section.");
                continue;
            }
            const tag = blockMatch[0].replace(':', '');
            const body = line.replace(blockMatch[0], '').trim();
            if (!characters.includes(tag))
                characters.push(tag)
            currentMeta = makeMeta()
            currentBlock = {
                tag,
                body,
                meta: currentMeta
            };
            currentSection.blocks.push(currentBlock)
            continue;
        }

        // Choices
        if (line[0] === '>') {
            currentMeta = makeMeta();
            currentChoice = {
                body: line.replace('>', '').trim(),
                meta: currentMeta
            }
            currentSection.choices.push(currentChoice);
            continue;
        }

        // Conditions
        if (line[0] === '?') {
            if (!currentMeta) {
                logger.error("Condition found, not on open block or choice:\n > ", chalk.red(line))
                continue
            }
            // TODO: Handle condition logic
            continue;
        }

        // Mutations
        if (line[0] === '~') {
            if (!currentMeta) {
                logger.error("Mutation found, not on open block or choice:\n > ", chalk.red(line))
                continue
            }

            // Get the mutation operator
            let mutationLine = line.replace('~', '').trim();
            let mutation = {};
            let operator = mutationLine.match(/[\+\-\=]+/s)
            if (!operator) {
                logger.error("No operator found for mutation:\n > ", chalk.red(line))
                continue;
            }
            mutation.operator = operator[0];

            // Get the left and righthand sides
            let parts = mutationLine.split(operator[0]);
            if (parts.length !== 2) {
                logger.error("Mutation requires single part before and part after operator:\n > ", chalk.red(line))
                continue;
            }

            // Check the left side
            let input = parts[0].trim();
            if (input.search(/^[a-zA-Z0-9]+$/s) === -1) {
                logger.error("Input names are alphanumeric only:", chalk.bgRed(parts[0]), "\n > ", chalk.red(line))
                continue;
            }
            mutation.input = input;

            // Encode the right-side as-is for now
            // TODO: Clean up right-side operators
            mutation.value = parts[1].trim();
            currentMeta.mutations.push(mutation);
            continue;
        }

        // Signals
        if (line[0] === ':') {
            if (!currentMeta) {
                logger.error("Signal found, not on open block or choice:\n > ", chalk.red(line))
                continue
            }
            currentMeta.signals.push(line.replace(':', '').trim())
            continue;
        }

        // Transitions
        if (line[0] === '-' && line[1] === '>') {
            if (!currentMeta) {
                logger.error("Transition found, not on open block or choice:\n > ", chalk.red(line))
                continue
            }
            currentMeta.transitions.push(line.replace('->', '').trim())
            continue;
        }

        // End
        if (line === 'END') {
            if (!currentMeta) {
                logger.error("End found, not on open block or choice:\n > ", chalk.red(line))
                continue
            }
            currentMeta.isEnd = true;
        }
    }

    console.log(chalk.yellow(">>> RESULT:\n"), sections);

    console.log(chalk.green("\n\n============\n\n"))

    return {
        characters,
        sections
    };
};