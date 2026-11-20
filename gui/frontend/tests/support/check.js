/* The checks every frontend test script uses. Plain Node, no dependencies.
 *
 *   import { check, same, near, finish } from "./support/check.js";
 *   check(cond, "what is checked");   // prints PASS or FAIL and counts the failures
 *   same(a, b)                        // true when JSON.stringify gives the same text
 *   near(a, b, eps)                   // true when |a - b| < eps (1e-9 by default)
 *   finish();                         // call last: sets process.exitCode = 1 when a check failed
 */
let failures = 0;

export function check(cond, what) {
  if (!cond) {
    console.error(`FAIL: ${what}`);
    failures++;
  } else {
    console.log(`PASS: ${what}`);
  }
}

export const same = (a, b) => JSON.stringify(a) === JSON.stringify(b);

export const near = (a, b, eps = 1e-9) => Math.abs(a - b) < eps;

export function finish() {
  if (failures > 0) {
    console.error(`${failures} check(s) failed`);
    process.exitCode = 1;
  }
}
