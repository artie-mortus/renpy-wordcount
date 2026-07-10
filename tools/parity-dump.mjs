import fs from 'node:fs';
import Parser from '../../words-til-vn/parser.js';

const ignoredStarters = new Set([
  'define', 'default', 'image', 'scene', 'show', 'hide', 'play', 'stop', 'queue',
  'window', 'with', 'jump', 'call', 'return', 'if', 'elif', 'else', 'while', 'for',
  'init', 'label', 'transform', 'python', 'screen', 'style', 'voice', 'pause',
  'camera', 'old', 'new', 'translate', 'menu', 'pass', 'set', 'zorder', 'onlayer',
]);

const parser = Parser.create({ ignoredStarters });
const result = parser.analyzeScript(fs.readFileSync(process.argv[2], 'utf8'));
const output = {
  totalWords: result.totalWords,
  dialogueLines: result.dialogueLines,
  scriptLines: result.scriptLines,
  counted: result.counted.map(({ lineNumber, speaker, text, words, scene }) =>
    ({ lineNumber, speaker, text, words, scene })),
};
process.stdout.write(`${JSON.stringify(output)}\n`);
