#!/usr/bin/env node


var SkaldParser = require('../example/src/library/skald-parser');

/**
 * Module dependencies.
 */

var program = require('commander');

program
    .version('0.1.0')
    .command('compile <filename>')
    .action((filename, cmd) => {
        console.log('compiling ' + filename);
    });

program.parse(process.argv);