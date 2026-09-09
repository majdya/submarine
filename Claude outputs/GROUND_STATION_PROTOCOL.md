# Ground Station TCP Protocol

The Ground Station is a **read-only** TCP client that connects to the Central Computer's TCP server on **port 9000**.

## Protocol Overview

**Connection**: TCP stream on localhost:9000 (or specified host:port)

**Request Format**: 
```
COMMAND_NAME[,param1,param2,...]\n
```

**Response Format**:
```
<JSON response body>
\n.\n
```

All responses are JSON with `"status"` field ("ok" or "error") and a `"data"` field.

## Supported Commands

### 1. LIST_SUBMARINES
Lists all submarines in the fleet.

**Request**: 
```
LIST_SUBMARINES
```

**Response**:
```json
{
  "status": "ok",
  "data": [
    {
      "serial": "C-9",
      "type": "Combat",
      "name": "Cutlass",
      "missionAssigned": true,
      "missionDescription": "...",
      "connected": false
    },
    ...
  ]
}
```

### 2. GET_LOGS
Retrieve logs for a submarine within a date range.

**Request**: 
```
GET_LOGS,<start_ymd>,<end_ymd>,<serial>
```

Example: `GET_LOGS,20260101,20260131,C-9`

**Parameters**:
- `start_ymd`: Start date (YYYYMMDD format)
- `end_ymd`: End date (YYYYMMDD format)  
- `serial`: Submarine serial number

**Response**:
```json
{
  "status": "ok",
  "data": [
    {
      "timestamp": "20260105T143022",
      "level": "INFO",
      "message": "Mode changed to PATROL",
      "source": "C-9"
    },
    ...
  ]
}
```

### 3. GET_EVENTS
Retrieve events for a submarine within a date range.

**Request**: 
```
GET_EVENTS,<start_ymd>,<end_ymd>,<serial>
```

Example: `GET_EVENTS,20260101,20260131,C-9`

**Parameters**:
- `start_ymd`: Start date (YYYYMMDD format)
- `end_ymd`: End date (YYYYMMDD format)
- `serial`: Submarine serial number

**Response**:
```json
{
  "status": "ok",
  "data": [
    {
      "timestamp": "20260105T143022",
      "type": "OBJECT_DETECTED",
      "details": "Object at bearing 045°, range 2.3 km",
      "submarine": "C-9"
    },
    ...
  ]
}
```

### 4. SUMMARY_REPORT
Get a comprehensive summary of the entire fleet.

**Request**: 
```
SUMMARY_REPORT
```

**Response**:
```json
{
  "status": "ok",
  "data": {
    "totalSubmarines": 3,
    "combat": 2,
    "research": 1,
    "activeMissions": 1,
    "submarines": [
      {
        "serial": "C-9",
        "type": "Combat",
        "missionAssigned": true,
        "missionStatus": "In Progress"
      },
      ...
    ]
  }
}
```

## Error Responses

If a command fails or has invalid parameters:

```json
{
  "status": "error",
  "data": "Error message describing what went wrong"
}
```

## Error Cases

- **Invalid command**: Returns HTTP 404
- **Missing parameters**: Returns error status with explanation
- **Submarine not found**: Returns error status
- **Date range empty**: Returns empty data array
- **Connection lost**: Client automatically reconnects on next request

## Implementation Notes

- Each connection receives **one request/response pair** only, then the server closes the connection
- The client (Ground Station CLI) reconnects for each command
- All dates are in YYYYMMDD format (no separators)
- Timestamps are in ISO 8601 format (YYYYMMDDTHHMMSS)
- JSON response is terminated by `\n.\n` (newline, period, newline)
- The system is **read-only**: no mutations, no configuration changes

## Ground Station CLI Usage

```bash
./ground_station [host] [port]
```

Default: `localhost:9000`

Example with remote server:
```bash
./ground_station 192.168.1.100 9000
```

The CLI provides an interactive menu with options 1-5 to invoke each command above.

## Central Computer Configuration

The TCP server in the Central Computer starts automatically on port 9000 when the app launches (see main.cpp). If port 9000 is in use, it logs a warning but continues running the console menu.

Both the HTTP dashboard (port 8080) and TCP server (port 9000) read from the **same Fleet** object under a shared `g_fleetMutex` lock, so Ground Station and the web dashboard always see consistent data.
