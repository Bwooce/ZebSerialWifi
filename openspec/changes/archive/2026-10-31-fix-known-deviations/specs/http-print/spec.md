## MODIFIED Requirements

### Requirement: Route and method
The bridge SHALL register `/pstprnt` for POST and OPTIONS. An OPTIONS request SHALL get 204 with no body and the
headers `Access-Control-Allow-Origin: *`, `Access-Control-Allow-Methods: POST, OPTIONS`,
`Access-Control-Allow-Headers: Content-Type`, `Access-Control-Allow-Private-Network: true` and
`Access-Control-Max-Age: 600`, and SHALL NOT touch the printer link. Other methods get the server's 405.

**Source**: `firmware/main/web.c` `web_start()` (`"/pstprnt"`, `HTTP_POST` and `HTTP_OPTIONS`), `pstprnt_options()`

#### Scenario: GET request
- **WHEN** a client sends `GET /pstprnt`
- **THEN** the bridge SHALL reply 405 and send nothing to the printer

#### Scenario: CORS preflight
- **WHEN** a browser sends `OPTIONS /pstprnt` as a preflight for a POST with `Content-Type: application/json`
- **THEN** the bridge SHALL reply 204 with the CORS headers above, and the browser then sends the POST

#### Scenario: Chrome private network preflight
- **WHEN** the preflight carries `Access-Control-Request-Private-Network: true`
- **THEN** the reply SHALL carry `Access-Control-Allow-Private-Network: true`
