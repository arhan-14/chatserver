Arhan Nagavelli - arn97
Keith Miquela - kvm33

TESTING PLAN:

1. Basic Functionality - Login (NAM)
Requirement: server correctly registers a unique username and sends a welcome message.
Detection method: when successful, server responds with a MSG from #all welcoming the new user.
Test: connect a raw client and send 1|NAM|6|alice| and verify the server responds with a welcome message.
Result: 1|MSG|...|#all|alice|Welcome to the chat!|

2. Basic Functionality - Broadcast (MSG #all)
Requirement: server correctly delivers a broadcast message to all named users.
Detection method: when successful, all connected named clients receive the message.
Test: connect two clients as alice and bob. Send 1|MSG|18||#all|hello world| from alice and verify bob receives it.
Result: 1|MSG|...|alice|#all|hello world|

3. Basic Functionality - Private Message
Requirement: server correctly routes a private message only to the intended recipient.
Detection method: when successful, only the target user receives the message.
Test: connect two clients as alice and bob. Send 1|MSG|16||bob|hello bob!| and verify only bob receives it.
Result: 1|MSG|...|alice|bob|hello bob!|

4. Basic Functionality - Status Update (SET)
Requirement: server correctly stores a user's status and broadcasts the update.
Detection method: when successful, all named users receive the status broadcast.
Test: send 1|SET|9|studying| as alice and verify all clients receive "alice is now "studying"".
Result: 1|MSG|...|#all|#all|alice is now "studying"|

5. Basic Functionality - WHO query (specific user)
Requirement: server correctly returns the target user's status.
Detection method: when successful, the requesting client receives the target's status.
Test: after alice sets status to "studying", send 1|WHO|6|alice| as bob and verify the response contains "studying".
Result: 1|MSG|...|#all|bob|studying|

6. Basic Functionality - WHO query (#all)
Requirement: server correctly returns a list of all active named users and their statuses.
Detection method: when successful, the requesting client receives all usernames and statuses.
Test: connect alice and bob, then send 1|WHO|5|#all| and verify both users appear in the response.
Result: 1|MSG|...|#all|alice|alice: studying\nbob|

7. Edge Case - Pipe character inside message content
Requirement: server correctly handles pipe characters inside the content field (last field).
Detection method: when successful, the message is delivered with the pipe characters intact.
Test: send 1|MSG|23||#all|hello|world|test| and verify the content is delivered as "hello|world|pipe|test".
Result: 1|MSG|...|alice|#all|hello|world|test|

8. Edge Case - Pipe character inside status
Requirement: server correctly handles pipe characters inside the status field (ASCII 124 is in 32-126).
Detection method: when successful, the status is stored and broadcast with pipes intact.
Test: send 1|SET|18|status|with|pipes| and verify the broadcast contains "status|with|pipes".
Result: 1|MSG|...|#all|#all|alice is now "status|with|pipes"|

9. Edge Case - Multiple messages in one send
Requirement: server correctly processes multiple complete messages delivered in a single read.
Detection method: when successful, both messages are processed in order with correct results.
Test: send 1|NAM|4|bob|1|SET|5|idle|1|WHO|5|#all| as one string and verify all three are handled in sequence.
Result: welcome message, status broadcast, then WHO response listing bob as idle

10. Edge Case - Partial messages split across multiple sends
Requirement: server correctly reassembles a message split across multiple reads.
Detection method: when successful, the server waits and processes the message once complete.
Test: send 1|NAM|6|al in one write, then ice| in a second write. Verify the server processes the NAM correctly.
Result: 1|MSG|...|#all|alice|Welcome to the chat!|

11. Edge Case - Empty status (SET)
Requirement: server correctly handles an empty status and suppresses the broadcast.
Detection method: when successful, no broadcast is sent and the status is cleared.
Test: send 1|SET|1|| and verify no broadcast is sent to other clients.
Result: no broadcast observed

12. ERR 0 - Invalid protocol version (fatal)
Requirement: server closes the connection when the protocol version is not 1.
Detection method: when successful, server sends ERR 0 and disconnects the client.
Test: send 2|NAM|6|alice| on a fresh connection and verify ERR 0 is received and connection closes.
Result: 1|ERR|...|0|Unreadable|

13. ERR 0 - Unknown message code (fatal)
Requirement: server closes the connection when the message code is unrecognized.
Detection method: when successful, server sends ERR 0 and disconnects.
Test: send 1|FOO|6|alice| and verify ERR 0 is received and connection closes.
Result: 1|ERR|...|0|Unreadable|

14. ERR 0 - MSG before NAM (fatal)
Requirement: server closes the connection when a client sends MSG without first registering a name.
Detection method: when successful, server sends ERR 0 and disconnects.
Test: on a fresh connection (no NAM), send 1|MSG|18||#all|hello world| and verify ERR 0 is received.
Result: 1|ERR|...|0|Unreadable|

15. ERR 0 - Missing final pipe (fatal)
Requirement: server detects a malformed message body lacking the trailing pipe and closes the connection.
Detection method: when successful, server sends ERR 0 and disconnects.
Test: send 1|NAM|6|alice (no trailing pipe) followed by no further data and verify no processing occurs and ERR 0 is sent once more data forces a parse attempt.
Result: 1|ERR|...|0|Unreadable|

16. ERR 1 - Duplicate username
Requirement: server rejects a NAM request when the username is already in use.
Detection method: when successful, server sends ERR 1 and the connection remains open.
Test: connect client A as alice. Connect client B and send 1|NAM|6|alice|. Verify client B receives ERR 1 and can still send another NAM.
Result: 1|ERR|...|1|Name in use|

17. ERR 2 - MSG to unknown user
Requirement: server rejects a MSG when the recipient does not exist.
Detection method: when successful, server sends ERR 2 and the connection remains open.
Test: as alice, send 1|MSG|21||nobody|hello nobody| and verify ERR 2 is received and alice stays connected.
Result: 1|ERR|...|2|Unknown recipient|

18. ERR 2 - WHO for unknown user
Requirement: server rejects a WHO when the target user does not exist.
Detection method: when successful, server sends ERR 2 and the connection remains open.
Test: send 1|WHO|8|phantom| and verify ERR 2 is received.
Result: 1|ERR|...|2|Unknown recipient|

19. ERR 3 - Illegal character in username
Requirement: server rejects a NAM when the username contains a character outside [A-Za-z0-9_-].
Detection method: when successful, server sends ERR 3 and the connection remains open.
Test: send 1|NAM|7|ali@ce| and verify ERR 3 is received and the client can retry with a valid name.
Result: 1|ERR|...|3|Illegal character|

20. ERR 3 - Illegal character in message content
Requirement: server rejects a MSG when the content contains a character outside ASCII 32-126.
Detection method: when successful, server sends ERR 3 and the connection remains open.
Test: send 1|MSG|18||#all|hello\tworld| (tab = ASCII 9) and verify ERR 3 is received.
Result: 1|ERR|...|3|Illegal character|

21. ERR 4 - Username too long
Requirement: server rejects a NAM when the username exceeds 32 characters.
Detection method: when successful, server sends ERR 4 and the connection remains open.
Test: send 1|NAM|34|aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa| (33 a's) and verify ERR 4 is received.
Result: 1|ERR|...|4|Too long|

22. ERR 4 - Status too long
Requirement: server rejects a SET when the status exceeds 64 characters.
Detection method: when successful, server sends ERR 4 and the connection remains open.
Test: send 1|SET|66|aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa| (65 a's) and verify ERR 4 is received.
Result: 1|ERR|...|4|Too long|

23. ERR 4 - Message content too long
Requirement: server rejects a MSG when the content exceeds 80 characters.
Detection method: when successful, server sends ERR 4 and the connection remains open.
Test: send a MSG with 81-character content and verify ERR 4 is received and alice stays connected.
Result: 1|ERR|...|4|Too long|

24. Recovery after recoverable error
Requirement: client connection persists and can succeed after a recoverable error.
Detection method: when successful, after receiving ERR 1-4, the client can send a valid message that is processed correctly.
Test: send 1|NAM|7|ali@ce| (ERR 3), then immediately send 1|NAM|6|alice| and verify the welcome message is received.
Result: 1|ERR|...|3|Illegal character| followed by 1|MSG|...|#all|alice|Welcome to the chat!|

25. Concurrency - simultaneous clients
Requirement: server correctly handles multiple clients connected and messaging at the same time.
Detection method: when successful, broadcasts and private messages are delivered correctly across all clients with no corruption or deadlock.
Test: connect five clients simultaneously, have each broadcast a message, and verify all clients receive all five broadcasts in order.
Result: all 5 MSG broadcasts received by all clients with correct sender fields