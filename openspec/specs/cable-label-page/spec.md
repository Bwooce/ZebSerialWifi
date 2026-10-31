# cable-label-page Specification

## Purpose
A self-contained web page, `tools/cable-labels.html`, opened from disk in a browser, that lays out cable flag labels
(the same text at both ends with a blank wrap zone between), renders them as 1-bit bitmaps at 300 dpi and prints them
as ZPL through the bridge's `POST /pstprnt`.

## Requirements

### Requirement: Default label and layout settings
The page SHALL start from these defaults: bridge address `zebserial.local`, label 60 x 20 mm, cable diameter 3 mm,
end margin 2.5 mm, margin by the wrap 1 mm, top and bottom margin 1.5 mm, largest letters 5 mm, font DIN Condensed,
bold on, same size on, fold marks off, rotate 180° off. Changed settings SHALL be kept in `localStorage`.

**Source**: `tools/cable-labels.html` `DEFAULTS`, `store`, `saveSettings()` (key `cable.settings`)

#### Scenario: First visit
- **WHEN** the page opens with nothing saved under `cable.settings`
- **THEN** it SHALL lay out a 60 x 20 mm label for a 3 mm cable and print to `zebserial.local`

#### Scenario: Settings kept between visits
- **WHEN** the user changes the cable diameter and reopens the page
- **THEN** the page SHALL use the saved diameter

### Requirement: Fixed 300 dpi
The page SHALL convert millimetres to dots at 300 / 25.4 dots per mm, rounded to whole dots, and SHALL offer no dpi
setting.

**Source**: `tools/cable-labels.html` `DPMM`, `mm2d()`

#### Scenario: Default label size in dots
- **WHEN** the label is 60 x 20 mm
- **THEN** the bitmap SHALL be 709 dots wide and 236 dots high

### Requirement: Wrap zone from the cable diameter
The blank wrap zone in the middle of the label SHALL be π times the cable diameter plus 1 mm. Each end SHALL be half
of what remains, and the Layout settings SHALL show the wrap zone and the room left at each end.

**Source**: `tools/cable-labels.html` `WRAP_SLACK_MM`, `wrapMm()`, `geom()`, `syncSettings()` (`#wrapNote`)

#### Scenario: 3 mm cable on a 60 mm label
- **WHEN** the cable diameter is 3 mm
- **THEN** the wrap zone SHALL be 10.4 mm and each end 24.8 mm, as the note shows to one decimal place

#### Scenario: Settings saved by an older version
- **WHEN** the saved settings hold a `wrapMm` value and no `cableMm`
- **THEN** the page SHALL derive the diameter as (wrapMm - 1) / π, rounded to 0.1 mm, and drop `wrapMm`

### Requirement: Same text at both ends, same way up
The page SHALL draw the row's text in a box at each end, the right box mirroring the left about the label's middle,
both the same way up, each box inset by the end margin, the margin by the wrap and the top and bottom margin. Text
SHALL be centred in each box by its ink, not its font box.

**Source**: `tools/cable-labels.html` `geom()` (`boxes`), `render()`

#### Scenario: Label folded round the cable
- **WHEN** a row's text is printed and the two ends are pressed together back to back
- **THEN** the two copies SHALL line up and each side SHALL read the right way up

### Requirement: Text size fitting
The page SHALL use the largest whole-dot font size, from twice the largest-letters height down to 8 dots, at which the
capital H is no taller than the largest-letters setting and every line's ink fits the box, with lines 1.12 em apart.
Up to two lines (Line 1, optional Line 2) SHALL be used; empty lines are left out.

**Source**: `tools/cable-labels.html` `fitLines()`, `fontFor()`, `render()`

#### Scenario: Long single line
- **WHEN** a line is too long to fit at the largest size
- **THEN** the page SHALL shrink the size until it fits, or report that it does not fit at 8 dots

### Requirement: Same size across a batch
With "Same text size on every label" on, the page SHALL print every row of the batch at the smallest of the sizes the
rows fit at alone, and the preview SHALL use that size.

**Source**: `tools/cable-labels.html` `batchEm()`, `refreshPreview()`, print button handler

#### Scenario: Short and long labels together
- **WHEN** one row's text fits at a smaller size than the others and same size is on
- **THEN** every label in the batch SHALL print at that smaller size

### Requirement: Fonts and weight
The page SHALL offer the fonts DIN Condensed, Helvetica Neue, Arial, Arial Narrow, Avenir Next Condensed, DIN
Alternate, Menlo and Verdana, with fallback to Helvetica Neue, Helvetica, Arial and sans-serif, and SHALL draw at
weight 700 when Bold is on, 400 when off.

**Source**: `tools/cable-labels.html` `FONTS`, `fontFor()`

#### Scenario: Font not installed
- **WHEN** the chosen font is not on the computer
- **THEN** the browser SHALL draw with the first available fallback

### Requirement: Optional fold marks
With fold marks on, the page SHALL print a 4-dot-wide, 1.5 mm long mark at the top and bottom edges of the label at
each edge of the wrap zone. The wrap zone shading in the preview SHALL be on screen only.

**Source**: `tools/cable-labels.html` `render()` (`S.marks`), `paint()`

#### Scenario: Fold marks off
- **WHEN** fold marks are off
- **THEN** the wrap zone SHALL print blank

### Requirement: Canvas to 1-bit image
The page SHALL render each label on a white canvas in black, then set a dot wherever the pixel's red value is below
128, packing rows most significant bit first with each row padded to whole bytes.

**Source**: `tools/cable-labels.html` `render()` (`bits`, `BPR`)

#### Scenario: Anti-aliased edge
- **WHEN** a text edge pixel is lighter than mid grey
- **THEN** that dot SHALL not print

### Requirement: ZPL with ASCII-compressed ^GFA
Each row SHALL become one format
`^XA^PO<N|I>^PW<width>^LH0,0^MNY^FO0,0^GFA,<bytes>,<bytes>,<row bytes>,<data>^FS^PQ<qty>,0,1,Y^XZ`, with `I` when
rotate 180° is on. Rows of data SHALL use ZPL ASCII compression: repeat counts G..Y (1 to 19) and g..z (20 to 400)
and "," for trailing zeros. A row identical to the previous row SHALL be encoded as ":"; otherwise an all-zero row as ",".

**Source**: `tools/cable-labels.html` `labelZpl()`, `gfa()` (line 646, the repeat test comes first), `compressRow()`, `repeatCode()`

#### Scenario: Blank row of the bitmap
- **WHEN** a bitmap row is all zero and the row before it is not
- **THEN** it SHALL be encoded as ","

#### Scenario: Second blank row
- **WHEN** two all-zero rows follow each other
- **THEN** the second SHALL be encoded as ":"

#### Scenario: Row same as the one before
- **WHEN** a bitmap row equals the previous row
- **THEN** it SHALL be encoded as ":"

### Requirement: Rows, quantities and pasting
The page SHALL hold rows of Line 1, Line 2 and a quantity of at least 1, kept in `localStorage` under `cable.rows`,
and SHALL skip rows with both lines empty. With no saved rows it SHALL start with one row, "NAS → SW1 port 12".
Removing the last row SHALL leave one empty row.

**Source**: `tools/cable-labels.html` `readRows()`, `jobs()`, `rows` paste handler, `addRow()`

#### Scenario: Last row removed
- **WHEN** the user removes the only row
- **THEN** one empty row SHALL remain

### Requirement: Pasting a list
Pasting text with two or more non-empty lines into a Line 1 field SHALL add one row per non-empty line, quantity 1,
with the text before the first tab as Line 1 and between the first and second tab as Line 2; text after a second
tab SHALL be dropped. It SHALL replace the row pasted into if that row is empty, and otherwise insert the new rows
before it. A paste of one line SHALL be an ordinary paste.

**Source**: `tools/cable-labels.html` `rows` paste handler

#### Scenario: Pasted list
- **WHEN** the user pastes "A\tB" and "C" on two lines into an empty Line 1
- **THEN** the page SHALL replace that row with rows (A, B) and (C, empty), each of quantity 1

#### Scenario: Pasted before a row with text
- **WHEN** the user pastes two lines into the Line 1 field of a row that holds "X"
- **THEN** the two new rows SHALL come before the row "X", which SHALL be kept

### Requirement: Keyboard entry
Enter in a row field SHALL move the focus to the next field; Enter in the last field SHALL add a row and focus it.

**Source**: `tools/cable-labels.html` `rows` keydown handler, `addRow()`

#### Scenario: Enter on the last quantity
- **WHEN** the user presses Enter in the last row's quantity field
- **THEN** a new empty row SHALL be added and its Line 1 field focused

### Requirement: Fit warnings and print refusal
For the previewed row only, the page SHALL show one warning, the first that applies in this order: the wrap zone
leaves less than a 5 x 3 mm text box; the text does not fit; capitals would be under 1.5 mm. Print MUST be refused,
sending nothing, if any row of the batch does not fit, and SHALL ask for confirmation before printing over 50 labels.

**Source**: `tools/cable-labels.html` `geom()` (`ok`), `refreshPreview()` (`#fitWarn`), print button handler

#### Scenario: One row too long
- **WHEN** the user prints a batch in which one row does not fit
- **THEN** the page SHALL show "<n> row(s) won't fit the label (first: ...). Shorten the text or use two lines." and send nothing

#### Scenario: Another row too long
- **WHEN** the previewed row fits but another row does not
- **THEN** no warning SHALL show until Print is pressed

#### Scenario: Large batch
- **WHEN** the batch totals 51 labels
- **THEN** the page SHALL ask "Print 51 labels?" before sending

### Requirement: Preview, captions and Print button
The preview SHALL show the active row, or "Sample text" when that row is empty, with captions "wrap X.X" over the wrap
zone and "letters X.X mm" for the capital height. The Print button SHALL read "Print N label(s)", or "Print" with 0
labels, and SHALL be disabled with 0 labels and while a batch is being sent. A success message SHALL clear on the next
edit to the rows.

**Source**: `tools/cable-labels.html` `refreshPreview()` (`#flagCaps`, `#previewCap`), `refresh()` (`#printBtn`), print button handler, `clearOkStatus()`

#### Scenario: Empty row selected
- **WHEN** the active row has no text
- **THEN** the preview SHALL show "Sample text" and the caption SHALL say "Sample"

#### Scenario: Edit after a print
- **WHEN** a batch was sent and the user then types in a row
- **THEN** the "Sent ..." message SHALL disappear

### Requirement: Print through the bridge's /pstprnt
The page SHALL send the whole batch as one `POST http://<address>/pstprnt`, aborting after 120 s. The request SHALL
be a CORS simple request: POST, a string body, and no header other than `Content-Type: text/plain`, because the bridge
does not answer a preflight. A 2xx reply SHALL show "Sent <n> labels at <time>." with the bridge's reply text; any other status SHALL
show "The bridge said: <reply>".

**Source**: `tools/cable-labels.html` print button handler, `withTimeout()`, `showStatus()`; `firmware/main/web.c` `pstprnt_post()`, `pstprnt_reply()` (`Access-Control-Allow-Origin: *`)

#### Scenario: Job accepted
- **WHEN** the bridge replies 200 "Sent <bytes> bytes to the printer."
- **THEN** the page SHALL show the sent message with that text

#### Scenario: Long batch
- **WHEN** a batch takes the bridge more than 120 s to pass to the printer (the bridge replies only after the job has drained)
- **THEN** the page SHALL abort the request and show the "Couldn't reach" message, although the bridge may go on printing

#### Scenario: Printer busy
- **WHEN** the bridge replies 503
- **THEN** the page SHALL show "The bridge said: The printer is busy with another job. Try again."

### Requirement: Unreachable bridge
When the request fails or times out, the page SHALL show "Couldn't reach <address>. Check the bridge is on and the
address is right." with the macOS Local Network access hint, mark the printer dot red and open the Printer settings.

**Source**: `tools/cable-labels.html` print button handler (`catch`)

#### Scenario: Mac browser without Local Network access
- **WHEN** the browser blocks the request to the bridge
- **THEN** the page SHALL show the "Couldn't reach" message with the System Settings, Privacy & Security, Local Network hint

### Requirement: Bridge address and connection test
The Printer settings SHALL hold the bridge address (an empty value falls back to `zebserial.local`), a "Test
connection" button that fetches `http://<address>/` in no-cors mode with an 8 s limit, and a link to
`http://<address>/printer`.

**Source**: `tools/cable-labels.html` `#ip`, settings input handler, `#testConn` handler, `#openBridge` handler

#### Scenario: Bridge answers
- **WHEN** the test fetch completes within 8 s
- **THEN** the page SHALL show "The bridge answered." and mark the dot green

#### Scenario: No answer
- **WHEN** the test fetch fails or takes longer than 8 s
- **THEN** the page SHALL show "No answer from <address>."

## Notes
- The `min` attributes on the number inputs are not enforced: any finite number is accepted. The real limits are the
  5 x 3 mm text box check, the fit check and quantity at least 1.
- The page notes that the end margin should stay at 2.5 mm or more on the GX430t, as the last 2 mm at one side does
  not print.
- Left-right position is not set by the page; it is the printer's Left Position, set on the bridge's Printer page.

### Known deviations

- The Address field cannot be cleared and retyped: an empty value is replaced by `zebserial.local` at once
  (`tools/cable-labels.html:753` stores the default, and `syncSettings()` at line 745 writes it back into the field).
  The page will be fixed.
