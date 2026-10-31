## MODIFIED Requirements

### Requirement: Bridge address and connection test
The Printer settings SHALL hold the bridge address, a "Test connection" button that fetches `http://<address>/` in
no-cors mode with an 8 s limit, and a link to `http://<address>/printer`. The Address field SHALL stay as typed
while it is edited, including empty; an empty address SHALL mean `zebserial.local` wherever the address is used,
and the field SHALL show `zebserial.local` again when it is left empty.

**Source**: `tools/cable-labels.html` `#ip`, `addr()`, settings input and change handlers, `#testConn` handler,
`#openBridge` handler

#### Scenario: Bridge answers
- **WHEN** the test fetch completes within 8 s
- **THEN** the page SHALL show "The bridge answered." and mark the dot green

#### Scenario: No answer
- **WHEN** the test fetch fails or takes longer than 8 s
- **THEN** the page SHALL show "No answer from <address>."

#### Scenario: Retyping the address
- **WHEN** the user clears the Address field and types `192.168.1.139`
- **THEN** the field SHALL show what was typed at each step, and prints SHALL go to `192.168.1.139`

#### Scenario: Address left empty
- **WHEN** the user clears the Address field and moves to another field
- **THEN** the field SHALL show `zebserial.local` and prints SHALL go there
