# AI Usage Report - All Phases (1, 2, and 3)

## Tool Used
ChatGPT / Gemini

## Phase 1: File Systems and Filtering

### Prompt 1: Parsing Conditions
**Prompt:** I asked the AI to generate a function `int parse_condition(const char *input, char *field, char *op, char *value);` which splits a `field:operator:value` string into its three parts based on my `Report` record structure.

**Result:** The AI suggested using `sscanf` with specific format specifiers to split the string into field, operator, and value components.

**Changes:** Rather than trusting raw pointer modifications that could easily overflow, I adjusted the implementation to copy the input into a safe local array wrapper (`temp[256]`) using `strncpy()` to guarantee boundary encapsulation before parsing the colon separators. I kept the signature but manually verified that it correctly handles the fixed-length string requirements of my struct.

### Prompt 2: Matching Records
**Prompt:** I asked the AI to generate a function `int match_condition(Report *r, const char *field, const char *op, const char *value);` that returns 1 if the record satisfies the condition and 0 otherwise.

**Result:** The AI generated a function using `strcmp` for string comparisons (category, inspector) and `atoi` for numeric values (severity).

**Changes:** I modified the function to:
- Support all required operators: `==`, `!=`, `<`, `<=`, `>`, `>=`.
- Fully support `time_t` timestamp comparisons.
- Ensure correct type conversion for numeric fields like severity and floating-point coordinates.

---

## Phase 2: Processes and Signals

### Prompt 3: Signal Handling with sigaction
**Prompt:** How do I implement a monitor program in C that responds to `SIGUSR1` and `SIGINT` using `sigaction` instead of `signal()`, and how do I safely write messages to the standard output inside a handler?

**Result:** The AI provided a template using `struct sigaction` and explained that `write()` is an async-signal-safe function, unlike `printf()`.

**Changes:** I implemented the `SIGINT` handler to explicitly call `unlink(".monitor_pid")` to ensure the hidden PID file is deleted when the program ends.

### Prompt 4: Process Forking for Deletion
**Prompt:** How can I make my C program run the external command `rm -rf` on a directory using `fork()` and the `exec` family of calls?

**Result:** The AI suggested a child process pattern using `fork()` where the child calls `execlp` to execute the system command.

**Changes:** - I added a role-based check so that only the `manager` can trigger the `remove_district` command.
- I added logic to ensure the corresponding `active_reports-*` symbolic link is also unlinked when the district is deleted.

---

## Phase 3: Grandchild Isolation and Hub Multi-Stage Pipelines

### Prompt 5: Grandchild Pipeline Generation
**Prompt:** Provide a C code framework where a process forks a child. This child creates a POSIX pipe, forks a grandchild, redirects the grandchild's standard output to the write end of the pipe, and executes an external program. The intermediate child must read from the pipe and print data continuously to stdout without blocking the top-level process loop.

**Result:** The AI provided a structural pattern demonstrating how to map `pipe()`, `fork()`, and `dup2(pipe_fd[1], STDOUT_FILENO)` to push data upstream between nested execution contexts.

**Changes:** - **File Descriptor Leak Isolation:** The initial AI example omitted closing unused pipe endpoints inside the nested forks. I manually introduced explicit `close()` statements right after the fork boundaries to prevent processes from hanging on read loops.
- **Interactive Integrity:** I adjusted the background child engine to print messages dynamically and call `fflush(stdout)`, ensuring the administrative `city_hub` prompt remains responsive and non-blocking.

---

## Issues Found
- **Signal Safety:** Initial AI versions of signal handlers used `printf()`, which I had to change to `write()` for signal safety to prevent internal deadlocks.
- **Type Matching:** The AI's initial filter logic did not account for the `timestamp` being a `time_t` type, requiring custom time processing and casting.
- **Custom File Manipulation:** I had to manually implement the logic for `lseek()` and `ftruncate()` for record removal as the AI's generic suggestions did not match the project's binary file structure.
- **Struct Alignment Realignment:** The initial data structures proposed by the AI suffered from structural padding issues that created gap artifacts in memory, causing a 196-byte sizing anomaly. I reordered the internal primitive types strictly by byte boundaries to match the required **exactly 208 bytes** size rule.
- **Unallocated Memory Crash (Segmentation Fault):** The AI parsed option flags by copying input strings directly into unallocated `NULL` pointers. I resolved this by providing fixed stack allocation arrays (`category_buffer[64]`, `desc_buffer[128]`) and adding a lookahead check (`if (i + 1 < argc)`) to protect against out-of-bounds pointer reads when options are missing parameters.
- **VirtualBox Permission Bugs:** The AI's method of evaluating directory attributes via `st.st_mode & 0777` caused errors inside the VirtualBox environment due to shared hypervisor mounts forcing additional high-order bits. I fixed this by using exact bitwise masks (`st.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO)`).

---

## What I Learned
- **Binary I/O:** How to use `lseek` to treat a binary file like an array of fixed-size records and use `ftruncate` to resize files cleanly when items are dropped.
- **UNIX Security:** How to programmatically enforce user roles (Inspector vs. Manager) and isolate permissions by inspecting `st_mode` attributes from `stat()`.
- **Signal Logic:** The importance of using `sigaction` for robust signal dispatching and utilizing low-level tracking PID files to control process single-instance restrictions.
- **Process Management:** Implementing multi-stage environments using `fork()` and `exec` to coordinate parent shell interfaces, decoupled background listeners, and piped grandchildren.
- **Memory Defense:** The importance of performing boundary checks on `argv` offsets and passing input parameters exclusively into bounded variables to ensure absolute system stability.