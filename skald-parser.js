const chalk = require('chalk');

exports.parse = (content) => {

    // Templates
    const makeMeta = () => ({
        mutations: [],
        conditions: [],
        signals: [],
        isEnd: false
    });

    // Get our individual lines
    let lines = content.split("\n");
    let lineNumber = 0;
    let numberOfErrors = 0;
    let numberOfWarnings = 0;
    let numberOfEndings = 0;
    let characters = [];
    let signals = [];
    let inputs = [];
    let sectionTags = [];
    let transitions = [];
    let transitionLineNumbers = [];

    let sections = [];
    let testbeds = [];
    let currentTestBed = null;
    let currentSection = null;
    let currentBlock = null;
    let currentChoice = null;
    let currentMeta = null;

    const logger = {
        error: (...args) => {
            numberOfErrors += 1;
            console.log(chalk.bgRed(lineNumber + ":"), ...args)
        },
        warn: (...args) => {
            numberOfWarnings += 1;
            console.log(chalk.bgYellow(lineNumber + ":"), ...args)
        },
        log: (...args) => {
            console.log(chalk.gray(lineNumber + ":"), ...args)
        }
    }

    const interpretValue = value => {
        if (value === 'true') return true;
        if (value === 'false') return false;
        if (value === 'null' || value === 'nil') return null;
        if (isNaN(value)) return value;
        return Number(value);
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

        // Check for testbed
        if (line.substr(0, 8) === '@testbed') {
            currentTestBed = {
                tag: line.replace('@testbed', '').trim(),
                sets: {}
            }
            continue;
        }

        // Testbed context:
        if (!!currentTestBed) {
            if (line === '@end') {
                testbeds.push(currentTestBed);
                currentTestBed = null;
                continue;
            }
            let parts = line.split('=');
            if (parts.length !== 2) {
                logger.error("Testbed can only have simple = statements:", chalk.red(line))
                continue;
            }
            currentTestBed.sets[parts[0].trim()] = interpretValue(parts[1].trim());
            continue;
        }

        // Check for section header
        if (line[0] === '#') {
            if (line.search(/^#[a-zA-Z0-9\-]+$/s) > -1) {
                const tag = line.replace('#', '');
                if (sectionTags.includes(tag))
                    logger.error("Duplicate tag:", tag)
                else
                    sectionTags.push(tag);
                currentSection = {
                    tag,
                    blocks: [],
                    choices: []
                }
                sections.push(currentSection);
            } else {
                logger.error("Improperly formatted section tag: \n > ", chalk.red(line))
            }
            continue
        }

        if (!currentSection) {
            logger.error("No open section on line: \n > ", chalk.red(line))
            continue;
        }

        // Text blocks
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
                type: 'attributed',
                tag,
                body,
                meta: currentMeta
            };
            currentSection.blocks.push(currentBlock)
            continue;
        }

        // Logic blocks
        if (line[0] === '*') {
            if (currentSection.choices.length > 0) {
                logger.error("All blocks should come before choices in a given section.");
                continue;
            }
            const label = line.replace('*', '').trim();
            currentMeta = makeMeta()
            currentBlock = {
                type: 'logic',
                label,
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

            // Get the condition operator
            let conditionLine = line.replace('?', '').trim();
            let condition = {};
            let operator = conditionLine.match(/(<\=|\>=|>|<|\=\=|\!\=)/s)
            let input;

            if (operator) {
                // In an operator line, capture the operator and split the line to separate input and value names.
                condition.operator = operator[0];
                let parts = conditionLine.split(operator[0]);
                if (parts.length !== 2) {
                    logger.error("Mutation requires single part before and part after operator:\n > ", chalk.red(line))
                    continue;
                }
                input = parts[0].trim();

                // Encode the right-side (currently no variable / input support)
                condition.value = interpretValue(parts[1].trim());
            } else {
                // In a non-operator line, we're looking at a boolean condition
                input = conditionLine.replace('!', '');
                condition.operator = '==';
                condition.value = conditionLine[0] !== '!';
            }

            if (input.search(/^[a-zA-Z0-9]+$/s) === -1) {
                logger.error("Input names are alphanumeric only:", chalk.bgRed(input), "\n > ", chalk.red(line))
                continue;
            }
            condition.input = input;
            if (!inputs.includes(input))
                inputs.push(input);
            currentMeta.conditions.push(condition);
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
                logger.error("Input names are alphanumeric only:", chalk.bgRed(input), "\n > ", chalk.red(line))
                continue;
            }
            mutation.input = input;

            // Encode the right-side as-is for now
            mutation.value = interpretValue(parts[1].trim());
            if (!inputs.includes(input))
                inputs.push(input);
            currentMeta.mutations.push(mutation);
            continue;
        }

        // Signals
        if (line[0] === ':') {
            if (!currentMeta) {
                logger.error("Signal found, not on open block or choice:\n > ", chalk.red(line))
                continue
            }
            let signal = line.replace(':', '').trim();
            if (!signals.includes(signal))
                signals.push(signal);
            currentMeta.signals.push(signal)
            continue;
        }

        // Transitions
        if (line[0] === '-' && line[1] === '>') {
            if (!currentMeta) {
                logger.error("Transition found, not on open block or choice:\n > ", chalk.red(line))
                continue
            }
            let transition = line.replace('->', '').trim();
            if (!!currentMeta.transition) {
                logger.error("There can only be one transition on any given block or choice. Keeping first one, discarding", chalk.red(line))
                continue
            }
            currentMeta.transition = transition;
            if (!transitions.includes(transition)) {
                transitions.push(transition);
                transitionLineNumbers.push(lineNumber)
            }
            continue;
        }

        // End
        if (line === 'END') {
            if (!currentMeta) {
                logger.error("End found, not on open block or choice:\n > ", chalk.red(line))
                continue
            }
            numberOfEndings += 1;
            currentMeta.isEnd = true;
            continue;
        }

        // If we are still here, the line is unhandled
        logger.error("Unhandled line:", chalk.red(line))
    }

    // Check transition matches
    for (const index in transitions) {
        let transition = transitions[index];
        if (!sectionTags.includes(transition)) {
            lineNumber = transitionLineNumbers[index];
            logger.error("Tag not found for transition:", chalk.bgRed(transition));
        }
    }

    // Check number of endings
    if (numberOfEndings < 1) {
        logger.error("No endings found!")
    }

    if (numberOfWarnings > 0)
        console.log(chalk.yellow(numberOfWarnings), "warnings")
    if (numberOfErrors > 0) {
        console.log(chalk.red(numberOfErrors), "errors")
        console.log(chalk.bgRed("Resolve errors before continuing."))
        return null;
    }

    return {
        characters,
        inputs,
        signals,
        testbeds,
        sections
    };
};