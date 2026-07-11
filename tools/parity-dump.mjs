import fs from 'node:fs';

globalThis.window = globalThis;
await import('../../words-til-vn/analysis.js');
const { default: Parser } = await import('../../words-til-vn/parser.js');

const ignoredStarters = new Set([
  'define', 'default', 'image', 'scene', 'show', 'hide', 'play', 'stop', 'queue',
  'window', 'with', 'jump', 'call', 'return', 'if', 'elif', 'else', 'while', 'for',
  'init', 'label', 'transform', 'python', 'screen', 'style', 'voice', 'pause',
  'camera', 'old', 'new', 'translate', 'menu', 'pass', 'set', 'zorder', 'onlayer',
]);

const parser = Parser.create({ ignoredStarters, lintProject: globalThis.SayCountAnalysis.lintProject });
const countMenuChoices = process.argv[2] === '--count-menu-choices';
const files = process.argv.slice(countMenuChoices ? 3 : 2).map(name => ({ name, content: fs.readFileSync(name, 'utf8') }));
const options = { countMenuChoices };
const result = files.length === 1 ? parser.analyzeScript(files[0].content, options) : parser.analyzeProject(files, options);
const READING_WPM = 200; // app-core.js:28; formula from app.js:81
const output = {
  totalWords: result.totalWords,
  dialogueLines: result.dialogueLines,
  scriptLines: result.scriptLines,
  averageWords: result.averageWords,
  readingMinutes: result.totalWords ? Math.max(1, Math.round(result.totalWords / READING_WPM)) : 0,
  speakers: [...result.speakers].map(([speaker, stats]) => ({ speaker, ...stats })),
  scenes: [...result.scenes].map(([scene, stats]) => ({ scene, ...stats })),
  counted: result.counted.map(({ lineNumber, speaker, text, words, scene, isMenuChoice }) =>
    ({ lineNumber, speaker, text, words, scene, isMenuChoice: Boolean(isMenuChoice) })),
  characterNames: Object.fromEntries([...(result.characterNames || [])].sort(([a], [b]) => a < b ? -1 : a > b ? 1 : 0)),
  speakerColors: Object.fromEntries([...(result.speakerColors || [])].sort(([a], [b]) => a < b ? -1 : a > b ? 1 : 0)),
  warnings: result.warnings.map(({ type, file, lineNumber, message }) =>
    ({ type, file, lineNumber, message })),
};
process.stdout.write(`${JSON.stringify(output)}\n`);
