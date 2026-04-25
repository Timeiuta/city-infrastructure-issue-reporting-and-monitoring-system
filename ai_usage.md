# AI Usage Report

## Tool Used
ChatGPT

## Prompt 1
I asked the AI to generate a function that parses a condition string of the form field:operator:value.

## Result
The AI suggested using sscanf to split the string into field, operator, and value.

## Changes
I kept the implementation but verified that it correctly handles the expected input format.

---

## Prompt 2
I asked the AI to generate a function that checks if a report matches a condition.

## Result
The AI generated a function using strcmp for string comparison and atoi for numeric values.

## Changes
I modified the function to:
- support additional operators: !=, <, <=, >, >=
- fully support timestamp comparisons with all operators
- ensure correct type conversion for numeric fields

---

## Issues Found
- The initial version did not support all comparison operators
- Timestamp handling was incomplete
- The function required manual validation to ensure correctness

---

## What I Learned
- How to parse structured strings using sscanf
- How to compare strings and integers in C
- The importance of verifying and correcting AI-generated code
- How incomplete logic can lead to incorrect filtering results