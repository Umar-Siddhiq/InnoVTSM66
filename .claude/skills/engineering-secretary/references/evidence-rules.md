# Evidence and Accuracy Rules

## Final status gate
Do not declare a problem fixed or working unless at least one relevant verification path exists:
- successful build plus targeted automated test;
- successful integration/bench test;
- successful runtime reproduction showing the corrected behavior;
- protocol/output comparison against an authoritative specification;
- hardware measurement supporting the claimed behavior.

A successful compile alone proves only that the code builds.

## Source traceability
For important claims, capture the source when available:
- file/function/line or symbol
- log timestamp
- test case/run
- commit/hash
- specification section/page
- oscilloscope/meter/bench observation

## Inference labels
Use `Inference:` or `Hypothesis:` when the explanation is not directly proven.

## Contradictions
If user observations, source code, logs, and tests disagree, document the disagreement. Do not select one silently.

## AI-proposed solution vs implemented solution
Keep these distinct. An AI recommendation is not an implementation. An implementation is not a verified fix until tested.

## Secret handling
Redact credentials, tokens, passwords, private keys, SIM security data, customer-identifying data, and other secrets from reports.
