# Todo

Next Features

- more commands: WHO
- channels
    - commands: JOIN, PART, PRIVMSG 
- multiserver
    - no cycles to handle duplicates
    - authentication
    - how to check for cycles
    - TTL to avoid infinite loops
    - server list
    - broadcast message to each server
- ping timeouts
- For final report, connect to server from real irc client (LimeChat)

## Log Files

### Session log

- Type: CSV
- Filename: `log/sessions/<timestamp>.log`
- Columns: nick, username, realname, IP, messages_sent, messages_rcvd, time_join, time_leave

### Nick History

- Type: CSV
- Filename: `log/.history`
- Each row will store all nicks for user as comma separated
- These should be loaded into memory on user registration
- These should be updated on user disconnect
- PRVIMSG can be addressed to any nick used by user previously
