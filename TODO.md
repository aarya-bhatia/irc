# Todo

## Issues

- on register, passive server does not send NICKs

## Todos

- Remove user entry from other peers on quit

- multiserver
  - no cycles to handle duplicates
  - broadcast message to each server
  - remove server if cycle is detected at registration time
    - SERVER message sent for all known peers
    - check if peer name already exists on other server
    - disconnect from that peer
  - save channels that exist on other server
  - implement LUSERS and stat commands
- ping timeouts
- For final report, connect to server from real irc client (LimeChat)
- stats
- nicknames <https://datatracker.ietf.org/doc/html/rfc2813#section-5.7>
- TODO: report
- demo video

## Session log

- Filename: `log/sessions/<timestamp>.csv`
- Columns: nick, username, realname, IP, messages_sent, messages_rcvd, time_join, time_leave

## Request Logs

- Filename: `log/history.csv`
- Columns: conn_type, name, request, time
