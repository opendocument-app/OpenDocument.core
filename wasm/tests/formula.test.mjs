import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import { before, describe, it } from 'node:test';

// `ODR_FORMULA_WASM` is set by ctest, empty where the build has no module.
const path = process.env.ODR_FORMULA_WASM ?? '';

const num = (number) => ({ type: 'number', number });
const str = (text) => ({ type: 'string', text });

/// A sheet `Sheet1` of plain values, keyed `column,row`, and the formulas in
/// it, which the source computes as it reads them.
function sheetOf(module, cells, settings = {}, syntax = 'ooxml') {
  const source = {
    sheet: (name) => (name.toLowerCase() === 'sheet1' ? 0 : null),
    cell: (sheet, column, row) => {
      const cell = cells[`${column},${row}`];
      if (cell === undefined) {
        return { type: 'empty' };
      }
      if (cell.formula !== undefined) {
        return module.evaluate(cell.formula, syntax, sheet, column, row,
          settings, source);
      }
      return cell;
    },
    extent: () => ({ columns: 4, rows: 4 }),
    name: (name) => ({ total: 'SUM(Sheet1!$A$1:$A$3)', relative: 'A1' })[
      name.toLowerCase()] ?? null,
  };
  return (formula, column = 3, row = 3) =>
    module.evaluate(formula, syntax, 0, column, row, settings, source);
}

describe('formula module', { skip: path === '' && 'no odr-formula.js' }, () => {
  let module;
  before(async () => {
    module = await createRequire(import.meta.url)(path)();
  });

  const cells = {
    '0,0': num(1), '0,1': num(2), '0,2': num(3),
    '1,0': str('x'), '1,1': { type: 'boolean', boolean: true },
    '1,2': { type: 'error', text: '#N/A' },
    '2,0': { formula: 'A1+A2' }, '2,1': { formula: 'C1*10' },
  };

  it('computes what the cells hold', () => {
    const at = sheetOf(module, cells);
    assert.deepEqual(at('SUM(A1:A3)'), num(6));
    assert.deepEqual(at('IF(B2,"yes","no")'), str('yes'));
    assert.deepEqual(at('B1&"y"'), str('xy'));
    assert.deepEqual(at('Sheet1!A2+1'), num(3));
    assert.deepEqual(at('Total*2'), num(12));
    // a relative name reads from a base cell the formats state apart
    assert.equal(at('Relative'), null);
  });

  it('computes a formula cell it reads', () => {
    assert.deepEqual(sheetOf(module, cells)('C2+1'), num(31));
  });

  it('reads OpenFormula', () => {
    const at = sheetOf(module, cells,
      { dialect: 'libreoffice', nullDate: 0, nullYear: 1930 }, 'opendocument');
    assert.deepEqual(at('of:=SUM([.A1:.A3])'), num(6));
    // a boolean is a number in LibreOffice
    assert.deepEqual(at('of:=[.B2]+1'), num(2));
  });

  it('spells an error as a cell does', () => {
    const at = sheetOf(module, cells);
    assert.deepEqual(at('1/0'), { type: 'error', text: '#DIV/0!' });
    assert.deepEqual(at('ISNA(B3)'), { type: 'boolean', boolean: true });
  });

  it('gives no answer where the evaluator has none', () => {
    const at = sheetOf(module, cells);
    assert.equal(at('NOW()'), null);
    assert.equal(at('NOSUCHFUNCTION(1)'), null);
    assert.equal(at('SUM('), null);
    assert.equal(module.evaluate('1', 'csv', 0, 0, 0, {}, {}), null);
  });

  it('formats a number with a format code', () => {
    assert.equal(module.format(0.256, '0.0%', 'en-US', false), '25.6%');
    assert.equal(module.format(1234.5, '#,##0.00', 'de-DE', false),
      '1.234,50');
    assert.equal(module.format(45000, 'yyyy-mm-dd', '', false), '2023-03-15');
    assert.equal(module.format(0, 'yyyy-mm-dd', '', true), '1904-01-01');
    assert.equal(module.format(1 / 3, '', '', false), '0.333333333333333');
    assert.equal(module.format(1, '0.0"', '', false), null);
  });

  it('names the category of a format code', () => {
    assert.equal(module.category('0.00%'), 'number');
    assert.equal(module.category(''), 'number');
    assert.equal(module.category('yyyy-mm-dd'), 'date');
    assert.equal(module.category('h:mm'), 'time');
    assert.equal(module.category('0.0"'), null);
  });

  it('counts a date as the settings do', () => {
    // 2024-01-01 is 45292 days after 1899-12-30
    assert.equal(module.serial(45292, {}), 45292);
    assert.equal(module.serial(45292, { epoch: 1904 }), 43830);
    assert.equal(module.days(43830, { epoch: 1904 }), 45292);
    assert.equal(module.serial(45292, { dialect: 'libreoffice', nullDate: 2 }),
      45290);
    // before 1900-03-01 Excel counts one day less, for its 1900-02-29
    assert.equal(module.serial(1, {}), 0);
  });

  it('turns typed input into the syntax of the file and back', () => {
    assert.equal(module.toFile('SUM(A1:B2)*2', 'opendocument'),
      'of:=SUM([.A1:.B2])*2');
    assert.equal(module.toFile('Sheet2!$A$1+1', 'opendocument'),
      'of:=[Sheet2.$A$1]+1');
    assert.equal(module.toFile('SUM(A1:B2)', 'ooxml'), 'SUM(A1:B2)');
    assert.equal(module.toFile('SUM(', 'ooxml'), null);
    assert.equal(module.toInput('of:=SUM([.A1:.B2])*2', 'opendocument'),
      'SUM(A1:B2)*2');
    assert.equal(module.toInput('of:=SUM(', 'opendocument'), null);
  });

  it('states what a formula reads', () => {
    assert.equal(module.reads('SUM(A1:B2)+C3', 'ooxml', 2, 'S'),
      '2,0,1,0,1 2,2,2,2,2');
    assert.equal(module.reads('s!A1', 'ooxml', 2, 'S'), '2,0,0,0,0');
    assert.equal(module.reads('Other!A1', 'ooxml', 2, 'S'), '2,0,*,0,*');
    assert.equal(module.reads('SUM(A:A)', 'ooxml', 0, 'S'), '0,0,0,0,*');
    assert.equal(module.reads('NOW()', 'ooxml', 0, 'S'), '');
  });

  it('builds no code at run time', () => {
    const glue = readFileSync(path, 'utf8');
    assert.doesNotMatch(glue, /new Function\b/);
    assert.doesNotMatch(glue, /[^\w.$]eval\(/);
  });
});
