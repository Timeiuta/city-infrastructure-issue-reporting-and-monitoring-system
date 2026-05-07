# AI Usage Report - Phases 1 and 2

## Tool Used
ChatGPT / Gemini

## Phase 1: File Systems and Filtering

### Prompt 1: Parsing Conditions
**Prompt:** I asked the AI to generate a function `int parse_condition(const char *input, char *field, char *op, char *value);` which splits a `field:operator:value` string into its three parts based on my `Report` record structure.

[cite_start]**Result:** The AI suggested using `sscanf` with specific format specifiers to split the string into field, operator, and value components[cite: 231, 232].

**Changes:** I kept the implementation but manually verified that it correctly handles the fixed-length string requirements of my struct and correctly identifies the colon separators.

### Prompt 2: Matching Records
**Prompt:** I asked the AI to generate a function `int match_condition(Report *r, const char *field, const char *op, const char *value);` that returns 1 if the record satisfies the condition and 0 otherwise.

[cite_start]**Result:** The AI generated a function using `strcmp` for string comparisons (category, inspector) and `atoi` for numeric values (severity, timestamp)[cite: 234, 235].

**Changes:** I modified the function to:
- [cite_start]Support all required operators: `==`, `!=`, `<`, `<=`, `>`, `>=`[cite: 227].
- Fully support `time_t` timestamp comparisons.
- [cite_start]Ensure correct type conversion for numeric fields like severity[cite: 262].

---

## Phase 2: Processes and Signals

### Prompt 3: Signal Handling with sigaction
**Prompt:** How do I implement a monitor program in C that responds to `SIGUSR1` and `SIGINT` using `sigaction` instead of `signal()`, and how do I safely write messages to the standard output inside a handler?

[cite_start]**Result:** The AI provided a template using `struct sigaction` and explained that `write()` is an async-signal-safe function, unlike `printf()`[cite: 282].

[cite_start]**Changes:** I implemented the `SIGINT` handler to explicitly call `unlink(".monitor_pid")` to ensure the hidden PID file is deleted when the program ends[cite: 275, 276].

### Prompt 4: Process Forking for Deletion
**Prompt:** How can I make my C program run the external command `rm -rf` on a directory using `fork()` and the `exec` family of calls?

[cite_start]**Result:** The AI suggested a child process pattern using `fork()` where the child calls `execlp` to execute the system command[cite: 269].

[cite_start]**Changes:** - I added a role-based check so that only the `manager` can trigger the `remove_district` command[cite: 271].
- [cite_start]I added logic to ensure the corresponding `active_reports-*` symbolic link is also unlinked when the district is deleted[cite: 268].

---

## Issues Found
- Initial AI versions of signal handlers used `printf()`, which I had to change to `write()` for signal safety.
- The AI's initial filter logic did not account for the `timestamp` being a `time_t` type, requiring manual type casting.
- I had to manually implement the logic for `lseek()` and `ftruncate()` for record removal as the AI's generic suggestions did not match the project's binary file structure[cite: 210, 261].

## What I Learned
- **Binary I/O:** How to use `lseek` to treat a binary file like an array of fixed-size records[cite: 322, 328].
- **UNIX Security:** How to programmatically enforce user roles (Inspector vs. Manager) by checking `st_mode` bits from `stat()`[cite: 194, 198].
- **Signal Logic:** The importance of `sigaction` for reliable signal handling and the use of PID files for inter-process communication[cite: 273, 278].
- **Process Management:** How to use `fork()` and `exec` to safely delegate tasks to external system utilities[cite: 281].