import fs from 'node:fs';
import Parser from '../../words-til-vn/parser.js';

const ignoredStarters = new Set([
  'define', 'default', 'image', 'scene', 'show', 'hide', 'play', 'stop', 'queue',
  'window', 'with', 'jump', 'call', 'return', 'if', 'elif', 'else', 'while', 'for',
  'init', 'label', 'transform', 'python', 'screen', 'style', 'voice', 'pause',
  'camera', 'old', 'new', 'translate', 'menu', 'pass', 'set', 'zorder', 'onlayer',
]);

const parser = Parser.create({ ignoredStarters });
const files = process.argv.slice(2).map(name => ({ name, content: fs.readFileSync(name, 'utf8') }));
const result = files.length === 1 ? parser.analyzeScript(files[0].content) : parser.analyzeProject(files);
const output = {
  totalWords: result.totalWords,
  dialogueLines: result.dialogueLines,
  scriptLines: result.scriptLines,
  counted: result.counted.map(({ lineNumber, speaker, text, words, scene }) =>
    ({ lineNumber, speaker, text, words, scene })),
  characterNames: Object.fromEntries([...(result.characterNames || [])].sort(([a], [b]) => a < b ? -1 : a > b ? 1 : 0)),
  speakerColors: Object.fromEntries([...(result.speakerColors || [])].sort(([a], [b]) => a < b ? -1 : a > b ? 1 : 0)),
};
process.stdout.write(`${JSON.stringify(output)}\n`);
