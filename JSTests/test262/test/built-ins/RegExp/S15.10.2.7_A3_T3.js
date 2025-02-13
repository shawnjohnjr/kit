// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The production QuantifierPrefix :: + evaluates by returning the two
    results 1 and \infty
es5id: 15.10.2.7_A3_T3
description: Execute /\s+java\s+/.test("\t javax package") and check results
---*/

var __executed = /\s+java\s+/.test("\t javax package");

//CHECK#1
if (__executed) {
	throw new Test262Error('#1: /\\s+java\\s+/.test("\\t javax package") === false');
}
