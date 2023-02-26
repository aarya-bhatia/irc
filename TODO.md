# Todo

- Add nick_map to server struct and load_nicks on creation
- Update nick_map on user registration event and nick changes
- Write nick_map to file on session close.

## File databases

- Save registered user's username, realname, nick history to file

## PRIVMSG

- Check if target user exists in nick db for RPL_AWAY

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
