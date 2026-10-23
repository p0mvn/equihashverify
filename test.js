/* The public addon is the pool's entry point. Every check below can fail npm test. */
'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const {execFileSync} = require('node:child_process');
const verify = require('bindings')('equihashverify.node').verify;
const cases = fs.readFileSync(path.join(__dirname, 'tests/proofs.tsv'), 'utf8')
  .trim().split('\n').map(line => {
    const [name, verdict, header, solution] = line.split(/\s+/);
    return {name, valid: verdict === 'valid', header: Buffer.from(header, 'hex'),
            solution: Buffer.from(solution, 'hex')};
  });
describe('public Node Equihash verification', function () {
  for (const replacement of ['deleted', 'replaced']) {
    for (const beforeLoad of [false, true]) {
      it(`accepts buffers when global.Buffer is ${replacement} ${beforeLoad ? 'before' : 'after'} loading`, function () {
        // A fresh process also exercises initialization without global.Buffer.
        const script = `
          const assert = require('node:assert/strict');
          const addonPath = require('bindings')({bindings: 'equihashverify.node', path: true});
          const header = Buffer.from(${JSON.stringify(cases[0].header.toString('hex'))}, 'hex');
          const solution = Buffer.from(${JSON.stringify(cases[0].solution.toString('hex'))}, 'hex');
          let verify;
          if (!${beforeLoad}) verify = require(addonPath).verify;
          ${replacement === 'deleted' ? 'delete global.Buffer;' : 'global.Buffer = Uint8Array;'}
          if (${beforeLoad}) verify = require(addonPath).verify;
          for (const params of [[], [200, 9]]) {
            assert.equal(verify(header, solution, ...params), true);
            assert.throws(() => verify(new Uint8Array(header), solution, ...params), TypeError);
            assert.throws(() => verify(header, new Uint8Array(solution), ...params), TypeError);
          }
        `;
        execFileSync(process.execPath, ['-e', script], {cwd: __dirname, timeout: 10000});
      });
    }
  }
  it('preserves exceptions from custom buffer instance checks', function () {
    const descriptor = Object.getOwnPropertyDescriptor(Buffer, Symbol.hasInstance);
    const sentinel = new Error('instance check failed');
    try {
      Object.defineProperty(Buffer, Symbol.hasInstance, {
        configurable: true, value() { throw sentinel; }
      });
      assert.throws(() => verify(cases[0].header, cases[0].solution), error => error === sentinel);
    } finally {
      if (descriptor) Object.defineProperty(Buffer, Symbol.hasInstance, descriptor);
      else delete Buffer[Symbol.hasInstance];
    }
  });
  for (const proof of cases) {
    it(proof.name + ' through both argument forms', function () {
      assert.equal(verify(proof.header, proof.solution), proof.valid);
      assert.equal(verify(proof.header, proof.solution, 200, 9), proof.valid);
    });
  }
  for (const parameters of [[], [200, 9]]) {
    for (const length of [0, 108, 139, 141]) {
      it(`rejects header length ${length} with ${parameters.length + 2} arguments`, function () {
        assert.equal(verify(Buffer.alloc(length), cases[0].solution, ...parameters), false);
      });
    }
    for (const length of [0, 1, 1343, 1345, 100000]) {
      it(`rejects proof length ${length} with ${parameters.length + 2} arguments`, function () {
        assert.equal(verify(cases[0].header, Buffer.alloc(length), ...parameters), false);
      });
    }
    for (const value of [null, undefined, 3, 'buffer', {}, [], new Uint8Array(140), new ArrayBuffer(140), new DataView(new ArrayBuffer(140)), new Uint32Array(140)]) {
      it(`rejects non-buffer ${String(value)} with ${parameters.length + 2} arguments`, function () {
        assert.throws(() => verify(value, cases[0].solution, ...parameters), TypeError);
        assert.throws(() => verify(cases[0].header, value, ...parameters), TypeError);
      });
    }
  }
  it('borrows buffer slices at nonzero offsets without modifying them', function () {
    const proof = cases[0];
    const headerStorage = Buffer.concat([Buffer.alloc(7), proof.header, Buffer.alloc(5)]);
    const solutionStorage = Buffer.concat([Buffer.alloc(11), proof.solution, Buffer.alloc(3)]);
    const beforeHeader = Buffer.from(headerStorage), beforeSolution = Buffer.from(solutionStorage);
    const header = headerStorage.subarray(7, 147), solution = solutionStorage.subarray(11, 1355);
    assert.equal(verify(header, solution), true);
    assert.equal(verify(header, solution, 200, 9), true);
    assert.deepEqual(headerStorage, beforeHeader);
    assert.deepEqual(solutionStorage, beforeSolution);
  });
  it('rejects wrong argument counts' , function () {
    for (const args of [[], [cases[0].header], [cases[0].header, cases[0].solution, 200],
                        [cases[0].header, cases[0].solution, 200, 9, 0]]) {
      assert.throws(() => verify(...args), TypeError);
    }
  });
  it('rejects unsupported and non-integer parameters', function () {
    for (const [n, k] of [[201, 9], [200, 32], [0, 0], [0xffffffff, 0xffffffff]])
      assert.throws(() => verify(cases[0].header, cases[0].solution, n, k), RangeError);
    for (const n of [-1, 1.5, NaN, Infinity, '200', null, {}])
      assert.throws(() => verify(cases[0].header, cases[0].solution, n, 9), TypeError);
    assert.throws(() => verify(cases[0].header, cases[0].solution, 200, '9'), TypeError);
  });
  it('retains the original parameter sets and rejects their wrong lengths', function () {
    for (const [n, k] of [[96, 3], [144, 5], [192, 7], [96, 5], [48, 5]])
      assert.equal(verify(cases[0].header, Buffer.alloc(0), n, k), false);
  });
  it('rejects correctly sized invalid proofs for every retained parameter set', function () {
    for (const [n, k] of [[96, 3], [144, 5], [192, 7], [96, 5], [48, 5]]) {
      const size = (2 ** k) * (n / (k + 1) + 1) / 8;
      assert.equal(verify(cases[0].header, Buffer.alloc(size), n, k), false);
    }
  });
  it('accepts the regtest genesis proof and rejects same-length mutations', function () {
    const proof = JSON.parse(fs.readFileSync(path.join(__dirname, 'tests/regtest.json'), 'utf8'));
    const header = Buffer.from(proof.header, 'hex'), solution = Buffer.from(proof.solution, 'hex');
    assert.equal(verify(header, solution, 48, 5), true);
    const changedHeader = Buffer.from(header), changedSolution = Buffer.from(solution);
    changedHeader[139] ^= 1;
    changedSolution[0] ^= 1;
    assert.equal(verify(changedHeader, solution, 48, 5), false);
    assert.equal(verify(header, changedSolution, 48, 5), false);
  });
});
