/* Covers the small value checks the models share (public/core/values.js): a whole number inside limits, the url of a materials item, a claim,
 * a trimmed string and a plain object. Plain Node script, run through ctest. */
import { wholeNumber, itemUrl, isClaim, isClaimArray, text, isPlainObject } from "../public/core/values.js";
import { check, finish } from "./support/check.js";

/* wholeNumber: digits only, trimmed, inside the limits; leading zeros are read. */
check(wholeNumber("12", 1, 31) === 12, "12 is a whole number from 1 to 31");
check(wholeNumber(" 7 ", 1, 5) === null, "7 is outside 1 to 5 (the spaces are trimmed first)");
check(wholeNumber(" 4 ", 1, 5) === 4, "spaces around digits are trimmed");
check(wholeNumber("1.5", 0, 9) === null, "1.5 is not a whole number");
check(wholeNumber(undefined, 0, 9) === null && wholeNumber(null, 0, 9) === null, "nothing is not a number");
check(wholeNumber("007", 0, 9) === 7, "leading zeros are read as 7");
check(wholeNumber("-3", -5, 5) === null && wholeNumber("1e2", 0, 999) === null && wholeNumber("", 0, 9) === null, "a sign, an exponent and an empty text are not whole numbers");
check(wholeNumber(5, 0, 9) === 5 && wholeNumber("0", 0, 9) === 0 && wholeNumber("9", 0, 9) === 9 && wholeNumber("10", 0, 9) === null, "the limits are inclusive");

/* itemUrl: the id is encoded, the tail follows. */
check(itemUrl("a b", "/versions") === "/api/materials/items/a%20b/versions", "the id of an item is percent-encoded in its url");
check(itemUrl("mot-1", "") === "/api/materials/items/mot-1", "an item with no tail");

/* A claim is an item with a span or a location; a claim array is a non-empty array of them. */
check(isClaim({ span: [0, 1] }) === true && isClaim({ location: { bar: 1 } }) === true, "an item with a span or a location is a claim");
check(isClaim([]) === false && isClaim({}) === false && isClaim(null) === false && isClaim("span") === false, "an array, an empty object, null and a string are not claims");
check(isClaimArray([]) === false, "an empty array is not a claim array");
check(isClaimArray([{ span: [0, 1] }, { location: 1 }]) === true && isClaimArray([{ span: [0, 1] }, {}]) === false && isClaimArray({ span: 1 }) === false, "a claim array is a non-empty array of claims");

/* text trims a string and gives "" for anything else. */
check(text(5) === "" && text(null) === "" && text(undefined) === "", "a value that is not a string is empty text");
check(text("  x ") === "x", "a string is trimmed");

/* isPlainObject: an object that is neither an array nor null. */
check(isPlainObject({}) === true && isPlainObject({ a: 1 }) === true, "an object is a plain object");
check(isPlainObject([]) === false && isPlainObject(null) === false && isPlainObject("a") === false && isPlainObject(3) === false, "an array, null and a primitive are not");

finish();
