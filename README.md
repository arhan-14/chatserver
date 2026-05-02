# README

## Authors
- Keith Miquela (kvm33)
- [Partner Name] ([netid])

## Overview
This project implements a multithreaded chat server (`chatd`) that follows the protocol described in the assignment. The server accepts TCP connections, allows clients to register a unique username, and supports messaging, status updates, and user queries.

Each client is handled in a separate thread, and shared user state is protected using a mutex.

## Features Implemented
- **NAM**: Register unique usernames (1–32 chars)
- **SET**: Set user status (0–64 chars)
- **MSG**:
  - Broadcast to `#all`
  - Private messaging to specific users
- **WHO**:
  - Query a specific user
  - Query all users via `#all`
- **ERR handling**:
  - All 5 error types implemented
  - ERR 0 closes connection, others are recoverable

## Design

### Concurrency
- Each client is handled by a dedicated thread (`pthread`)
- Shared user list protected by `users_mutex`

### Data Structures
- `User` struct: `fd`, `active`, `has_name`, `name`, `status`
- `Message` struct: parsed fields + true lengths (`field_len`, `content_len`)

### Parsing Strategy
- Accumulator buffer (`acc`) handles partial and multiple messages
- `check_header()` validates header and extracts lengths
- `parse_message()`:
  - Ensures structural correctness
  - Copies safely into fixed buffers (no overflow)
  - Stores true lengths for validation

### Validation Strategy
- All length checks use protocol lengths, not truncated strings
- Separation of concerns:
  - parsing = structure
  - validation = correctness

### Processing
`process_message()` handles:
- User registration
- Status updates + broadcast
- Message routing
- WHO queries

## Testing Plan

We tested using both:
- Provided `client` (functional testing)
- Provided `raw` client (protocol-level testing)

### 1. Basic Functionality
- Login (NAM)
- Broadcast messages
- Private messages
- Status updates
- WHO queries

### 2. Edge Cases
- Messages containing `|`
- Multiple messages in one send
- Partial messages (split input)
- Empty status (`SET||`)

### 3. Error Handling

**ERR 0 (fatal)**
- Invalid version
- Malformed header
- Missing final `|`
- MSG before NAM

**ERR 1 — Name in use**
- Duplicate username

**ERR 2 — Unknown user**
- Messaging non-existent user
- WHO for unknown user

**ERR 3 — Illegal character**
- Invalid characters in name, status, or message

**ERR 4 — Too long**
- Name > 32 chars
- Status > 64 chars
- Message > 80 chars

### 4. Length Validation
Verified that:
- Length field includes the final `|`
- Server correctly reads the exact number of bytes
- Too-long inputs produce ERR 4 (not ERR 0)

### 5. Concurrency
- Multiple clients connected simultaneously
- Broadcast and private messaging verified across clients

## Notes / Assumptions
- Sender field in client MSG is ignored (per spec)
- Server may send messages longer than 80 chars
- User list size capped at 64 users
- WHO output order is unspecified

## How to Run

```bash
make
./chatd <port>
```

Example:

```bash
./chatd 5555
```

## Files Included
- `chatd.c` — main server implementation
- `chatd.h` — data structures and enums
- `Makefile`
- `README`
- `AUTHOR`