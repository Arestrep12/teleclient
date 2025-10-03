# TeleClient - CoAP CLI

Usage examples:
- GET:
  ./bin/coapc_debug --host 127.0.0.1 --port 5683 --method GET --path /hello --timeout 500 --retries 3
- POST echo:
  ./bin/coapc_debug --host 127.0.0.1 --port 5683 --method POST --path /echo --payload "abc" --timeout 500

Flags:
- --host HOST (required)
- --port N (required)
- --method GET|POST (required)
- --path /path (required)
- --payload STRING (for POST)
- --timeout MS (default 1000)
- --retries N (default 2)
- --non (send as NON instead of CON)
- --verbose
