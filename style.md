# Shared Style

Rules every language shares: lines, prose, comments and the shape of simple code.
Read it with the file of the language at hand: `pystyle.md`, `cstyle.md`, `jsstyle.md` or `cssstyle.md`.
A markdown document follows `mdstyle.md` and takes its prose rules from here.

One sentence holds the rest together:
**write the fewest lines and words that still carry the meaning, and put every break where the meaning breaks.**

Five more hold almost always:

- Minimalism, simplicity and readability beat speed: little code, plainly written, beats prose that is merely correct and code that is merely clever.
- A comment never repeats the code: it carries what the code cannot say, or it goes.
- Every description is stateless and timeless: it says what is, never its history, and nothing is written on the impulse of a fix, a session or a request.
- Layers stay apart, split by function: an application knows its library, a library knows no application, and nothing reaches past the layer under it, see [Layers](#layers).
- The exception is something truly odd: one line on why it is so, so that the bug never comes back.

Every rule below is a default, and these sentences win where they disagree.
Every code block is an invented example: its names are toys, never a call to copy.
A wrong form is marked `NO`, the right one `YES`.

**Layout** · [Line and indent](#line-and-indent) · [Breaking lines](#breaking-lines) · [Wrapping code](#wrapping-code) · [Alignment](#alignment) · [Separators](#separators) · [File header](#file-header)

**Prose** · [Line breaks in prose](#line-breaks-in-prose) · [Punctuation](#punctuation)

**Comments** · [What earns a comment](#what-earns-a-comment) · [Voice](#voice) · [Placement](#placement)

**Code** · [Naming](#naming) · [Timeless](#timeless) · [Simplicity](#simplicity) · [Layers](#layers) · [Repeated state](#repeated-state) · [Behaviour](#behaviour) · [Growth](#growth)

**[Before handing over](#before-handing-over)**

---

## Layout

### Line and indent

- **99** characters at most on a line of code, **2** spaces of indent, never 4 and never a tab.
  Two lines run past 99: a row of a data table, see [Breaking lines](#breaking-lines), and a line of a prompt a model reads, since a newline inside it is content.
- Single blank lines only, never double
- No trailing whitespace
- UTF-8 without BOM
- LF, held by a `.gitattributes` with `* text=auto eol=lf` in every repo, and CRLF for a `.bat`.
  C trees have their own case, see `cstyle.md`.

### Breaking lines

**One line carries one piece of information.**
Sometimes that is a sentence, more often less: a key of a dict or a field of an initializer, a condition, a term of a flag word, a call of a chain, a row of a table, or a short run of arguments that together set one thing up.
A callback with a body, a long expression or a nested structure or initializer is a piece of its own.
A diff then touches the line whose fact changed and no other line, which is what versioning wants.

The test that decides every break is the same:
**would a later change to one of these facts have to touch the other?**
If yes, they are two lines.
If they only ever change together, they are one.
A break is a judgement about meaning, never a count of tokens: neither everything on one line nor one item per line for its own sake.

```py
# NO: broken where the column ran out, the callback cut in half
job = schedule(name, every_s=60, retries=3, on_fail=lambda e: log.wrn(
  f"{name} failed | {e}"))

# NO: one argument per line for its own sake, five lines for one piece of setup
job = schedule(
  name,
  every_s=60,
  retries=3,
  on_fail=on_fail,
)

# YES: the short arguments that make the job are one piece, the callback its own
job = schedule(name, every_s=60, retries=3,
  on_fail=lambda e: log.wrn(f"{name} failed | {e}"))
```

One statement per line is the default, and a one-line guard is a statement.
A guard that does one thing on its way out keeps that thing on the line too:

```js
if(!items.length) return;
if(err) throw err;
if(note) { listEl.replaceChildren(note); return; }
```

Two declarations that are one piece of state may share a line, two unrelated ones do not:

```js
let qi = 0, streak = 0; // one walker, one state

// NO: nothing ties them, each will change alone
const total = 0, name = "";
```

A uniform table may pack, because there the row is the piece of information.
A row stays one line, its note included, even past 99: a note moved above its row, or a row folded in two, breaks the one thing a table is for.
Two statements share a line only inside such a run, where every row is one case:

```js
if(next === "n") { out.push("\n"); i += 2; continue; }
if(next === "r") { out.push("\r"); i += 2; continue; }
if(next === "t") { out.push("\t"); i += 2; continue; }
```

### Wrapping code

A continuation indents by 2 from its statement, and the break falls before the operator that starts the next term: `&&`, `||`, `?`, `:`, `.`, `+`.
It never pads to the opening delimiter or to an operand: that alignment moves every line when the first one is edited, and the width it eats grows with nesting.

```js
// NO
const url = build(base, path,
                  query);
const element = isSvg ? document.createElementNS(SVG_NS, tag)
                      : document.createElement(tag);

// YES
const url = build(base, path,
  query);
const element = isSvg
  ? document.createElementNS(SVG_NS, tag)
  : document.createElement(tag);
const ready = state.loaded
  && !state.busy
  && items.length > 0;
```

A wrapped collection keeps its opener on the line, closes on its own line at the indent of the statement, and ends every entry with a comma.
Its entries group by what belongs together: one per line when each is a fact of its own, several per line when they form one row.

```js
const client = createClient({
  base: BASE,
  prefix: "widget",
  headers: { "Accept": "application/json" },
});
```

### Alignment

One space, never a column of them.
Two things may form a column: a run of trailing comments, and a uniform table, where the columns are the point.
C, JavaScript and CSS align such a table, Python keeps one space.

**A trailing comment standing alone gets exactly one space before its marker.**
Three spaces on a lone comment is a column of one, which is padding.

**A run of declarations with trailing comments shares one column, 2 spaces past its longest commented line.**
A line without a comment inside the run neither ends the column nor moves it, and a blank line or another block ends it.
A single line too long for the column puts its note above rather than pushing the column right.
Widths too far apart for one column give up the column: one space each, or the notes move above.
Statements never pad: they change length as they are edited, and three adjacent ones then land on three columns.

```py
# NO
LIMIT_HIGH   = 1000
LIMIT_LOW    = 500
expect = 0   # one word, so the floor is the whole budget

# YES
LIMIT_HIGH = 1000
LIMIT_LOW = 500
expect = 0 # one word, so the floor is the whole budget

CONFIG = {
  "mode": "fast",  # fast | safe
  "limit": None,   # `None` = no cap
  "retries": 3,    # per item
}
```

### Separators

Exactly **99** characters, indent included: the comment marker, dashes, a space and the name.
A nested separator gives its indent out of the dashes and is still 99.

```py
#------------------------------------------------------------------------------------------ Section
  #----------------------------------------------------------------------------------------- Nested
```

One file marks its sections one way.
A bare label line above a group does the job a separator already does, and two mechanisms in one file leave the reader guessing which one means more.

### File header

The first line of every file is its path as a comment.
A blank line follows it, so nothing ever sits glued to the path.

- Python: `# package/module.py`, then the module docstring
- C: `// mod/module.h`, never `/** @file */`
- JavaScript: `// scripts/Widget.jsx`, and `/* styles/widget.css */` in a stylesheet

The path is where the file sits in its project.
On a rename or a move, the header follows.

---

## Prose

### Line breaks in prose

**A line of prose carries one portion of information**, as a line of code does: a sentence, two short sentences, or a clause after a comma.
A later edit then touches the line whose fact changed.
In code 99 is a ceiling, never a wrapping rule, and markdown has no ceiling at all.

A break lands where the sentence pauses: at a full stop, before a conjunction, after a clause, between items.
Never between an article or an adjective and its noun, a preposition and its object, a subject and its verb.
Never inside a parenthetical, and never leaving one word orphaned on the next line.
A line ending in `the`, `a`, `which`, `so`, `and` or `every` was broken by accident.

```py
# NO: breaks fall wherever column 99 landed
"""
`CACHE`, `INDEX` and `QUEUE` offloaded to a worker thread. Same API as the
sync versions, just `await` the calls, except the pure helpers (`CACHE.parse`,
`INDEX.format`) which are called directly; generators are
sync-only.
"""

# YES: one idea per line, the break where the sentence breathes
"""
`CACHE`, `INDEX` and `QUEUE` offloaded to a worker thread.
Same API as the sync versions, just `await` the calls.

Called directly, not awaited: the pure helpers `CACHE.parse` and `INDEX.format`.
Sync-only: the generators.
"""
```

Often the fix is not re-wrapping but rewriting: split the run-on into two sentences, or give a semicolon clause its own line.
A short line that finishes a thought beats a full one that cuts a phrase in half.

### Punctuation

**Never an em dash** (U+2014), anywhere: comments, docstrings, strings, help text, markdown.
A dash used as punctuation, em dash or ASCII hyphen, becomes a full sentence, a colon, a comma or parentheses.

```py
# NO
# Handles alpha — beta has the context
# Handles alpha - beta has the context

# YES
# Handles alpha, beta has the context
```

Code stays ASCII, and `->`, `>=`, `=>` are its syntax.
Comments, docstrings, logs and prose use the real characters: `→`, `←`, `↔`, `×`, `≈`, `≤`, `≥`, `°`, `±`, `Ω`, `µ`, `²`.

```py
# NO
# Raw -> parsed -> stored

# YES
# Raw → parsed → stored
def parse(day:str) -> str: # the `->` here is syntax
```

A value and its unit stand without a space in text: `40Hz`, `10ms`, `1.5kW`, `256MiB`, `90%`.
In code the unit lives in the name, `timeout_ms`, or in a trailing comment.

A code symbol in a comment or a docstring wraps in backticks: `CACHE.load`, `_busy`, `None`, `max_age`.
If a reader could grep for it, it takes backticks.
Plain words stay plain.

Periods follow the shape of the text.
A one-line comment reads as a label and takes none.
Two or more sentences take one after each, the last included, and so does prose that spans lines.

```c
// Active thread index
// Initialize module. Call after `BUS_Init`.

// Counter is stopped and rewound before every wait.
// `TIM_Init` leaves `ARPE` set, which buffers `ARR` until the next update event.
```

---

## Comments

### What earns a comment

**Silent, valuable, timeless, stateless.**

Silent: a comment that says what the code says goes.
The test is mechanical: if every word of the comment already appears in the line below it, the comment carries nothing.

```py
# NO
# Loop over items and render each one
for item in items: render(item)

# YES: nothing, the line says it
for item in items: render(item)
```

Valuable: the *why*, an invariant, an order that looks wrong and is right, a unit, a format, the meaning of a magic number or a register bit, a quirk of a library or of the hardware, a race or a timing, a trap the next editor would walk into.

```js
// A click starts the download asynchronously, so the url has to outlive this frame
setTimeout(() => URL.revokeObjectURL(url), 1000);
```

Not worth it: what the names already say, a restated signature, a TODO without a date or a ticket.
Substance beats brevity: three lines carrying a constraint earn their place, one line restating a signature does not.

Timeless and stateless: a comment describes the thing and the reason.
Never what changed, never who asked, never another file, never the session, never "now", see [Timeless](#timeless).

A pointer to a sibling file rots with the next rename and says nothing a name does not.
Name the symbol, which a reader can search for, or say nothing.

```py
# NO
# Helpers in keys.py. The role ladder itself is in utils.py.

# YES
# Data and wording come from `BUCKETS`
```

### Voice

English, plain, in every comment, docstring, Doxygen and JSDoc line.
Start with the fact.
No ceremony: no "note that", "please", "simply", "obviously", "in order to".
No `the` that points at nothing; keep it where it points at one specific thing.
A word earns its place by carrying a fact the reader would otherwise miss, never by making the sentence prettier.

```py
# NO
# The function checks whether the token has expired before the request is sent to the server.

# YES
# An expired token never leaves: the server would only say so, more slowly
```

A full-line comment starts with a capital letter, a trailing comment with a lowercase one:

```py
# Retries cover a flaky link, not a dead one
send(frame) # idempotent
```

### Placement

Placement follows reach.
A note about one line, fitting beside it, trails that line.
A note covering more than one line, or too long to trail, sits above the block.
Never both for one thought, and never wrapped to the right:

```py
# NO
LIMIT = 1000 # applies to the whole batch; a single item is
             # checked separately

# YES
# Applies to the whole batch, a single item is checked separately
LIMIT = 1000
```

---

## Code

### Naming

A name says what a thing is, never what it was: no `new`, `old`, `v2`, `tmp`, `legacy`.
What is public is unambiguous: API fields, functions, classes, constants, files.
A short form that is already a standard stays, and so does a name built on one: `bit_rst` beside `bit_set`, `absv`, `len`, `init`, `rx`, `tx`.
A thing of our own making takes full words, because nobody but its author knows its short form.
A local may stay short.

```py
# NO: temperature or a temporary file?
def read_tmp(sensor): ...

# YES
def read_temperature(sensor): ...
```

A quantity carries its unit in the name, `timeout_ms`, `freq_Hz`, and where the name cannot, the trailing comment does.
Casing is the language's call, see its file.

### Timeless

Code describes what is, never what changed.
No note explaining an edit, no history in a name, nothing that reveals which line was written today.

```js
// NO
const LIMIT = 45; // was 50, lowered after the timeout review
function loadV2(path) { ... }
const key = cache.get("token") || cache.get("access_token"); // legacy key

// YES
const LIMIT = 45;
function load(path) { ... }
const key = cache.get("access_token");
```

No backward compatibility: no shim reading a previous layout, no alias for a previous name, no fallback for a state that no longer exists.
A counterfactual is fine when it says why a line must stay: "mask the IRQ first, a pending edge would be queued otherwise".
A history of the repository is not.

### Simplicity

Two or three explicit items stay explicit.
Explicit beats clever, most of all when the items differ a little.

```py
# NO: the loop hides that one of them needs a different writer
for path, default in DEFAULTS.items():
  if not CACHE.exists(path):
    CACHE.save_smart(path, default)

# YES: each file visibly gets the right writer
if not CACHE.exists("config.json"):
  CACHE.save_pretty("config.json", CONFIG_DEFAULT)
if not CACHE.exists("state.json"):
  CACHE.save_smart("state.json", [])
```

The rule runs both ways: a pattern written three times earns a helper.
Duplicated logic that can drift is a defect in itself: call the existing function rather than writing it again, even when the shared path is slower.
A helper shared by two files lives where both can see it, or it is not shared.

Simple beats fast: a loop that reads at a glance beats a fold that needs a comment, and a second pass over a list beats an index juggle.
Optimize when a measurement says so, and say where in a comment.

### Layers

**Layers stay apart, split by function, and a dependency points one way: down.**
A file belongs to one layer.
A library knows no application: it is universal, a product of its own.
An application knows its libraries and reaches them only through what they make public.
It never reaches past one to what that one is built on: a library wrapping a format or a tool does all the work on it.
What a caller misses in a library goes into the library, never around it.
Programs with no reason to know each other never do: no import, no shared private, no path into the other's tree.
Even a single application is written in layers, its lower one shaped as if others would use it.

### Repeated state

A state change touching several fields or controls lives in one place, never copied into every handler.

```js
// NO: the same three lines in every handler
sendBtn.loading = true;
cancelBtn.hidden = false;
drop.loading = true;

// YES: one place to change
function setRunning(on, phase) {
  running = on;
  sendBtn.loading = on;
  cancelBtn.hidden = !on;
  drop.loading = on;
  status.textContent = on ? phase : "";
}
```

The same for a compound answer: one function decides what a run needs, and every caller reads the same decision.

### Behaviour

A lookup miss returns nothing, `None`, `null` or `NULL`, and the caller decides what it means.
A value compared against internal data carries the scale of that data: no `× 1000` on the way in and `/ 1000` on the way out.
A value the application's config sets lives there alone: a default repeated in a signature is a second source of truth.
A library has no such config, and its defaults stay in its signatures.
An async call is always awaited: one nobody awaits is a silent bug, unless a comment says fire-and-forget and why.

### Growth

What keeps code workable as it grows, in every language:

- **Data in, data out.** A library takes values and returns values; files, network, clock and environment stay at the edge, in the application or the CLI.
- **One fact, one place.** A default, a word, a colour or a schema is written once, and everything else reads it.
- **Kinds are data.** A thing that comes in kinds, as journals, devices or languages, is a row in a table the code reads, never an `if kind ==` in every function; a new kind is a new row.
- **Check at the door.** Input is checked where it enters, with a message that says what to do; past the door, code trusts it.
- **Same input, same output.** Time, randomness and the machine come in as arguments, so a run repeats and a test pins it.
- **State has an owner.** Nothing mutable lives at module level; what outlives a call belongs to an object the caller holds, or to a store, and a cache keyed by its input is the one exception.
- **Small surface.** A public name is a promise: export what callers need, keep the rest private, and the inside stays free to change.
- **Proof by running.** A change is proven by tests that ran, a fix by a test that failed first, and the report gives what ran and its numbers; a test older than the code is brought up to it, never silenced.
- **A dependency is a cost.** One more only where it replaces real code; a heavy one is optional, behind a guard.

---

## Before handing over

Read each line on its own: one that ends mid-phrase gets its break moved or its sentence rewritten.
Read the page around an added block and keep its density, because rules hold when a file is written from scratch and get lost when one is added to.
