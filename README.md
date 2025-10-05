# TeleClient - CoAP CLI

Cliente de línea de comandos para interactuar con TeleServer (CoAP sobre UDP).

## Uso rápido (Producción)

Compilar (producción):
```bash
make release
```

Ejecutar health check:
```bash
./bin/coapc --host 127.0.0.1 --port 5683 --method GET --path /api/v1/health
```

Enviar telemetría (simulación de ESP32):
```bash
./bin/coapc --host 127.0.0.1 --port 5683 --method POST --path /api/v1/telemetry \
  --payload '{"temperatura":25.5,"humedad":60.2,"voltaje":3.7,"cantidad_producida":150}'
```

Consultar telemetría (renderizado en tabla):
```bash
./bin/coapc --host 127.0.0.1 --port 5683 --method GET --path /api/v1/telemetry
```
Salida esperada (tabla):
```
TIMESTAMP(ms)     |  TEMPERATURA |    HUMEDAD |  VOLTAJE |   CANTIDAD_PROD
-----------------+--------------+------------+----------+----------------
      1696294955 |         25.50 |      60.20 |     3.70 |            150.00
```

Notas:
- La salida en tabla se activa automáticamente cuando se consulta GET /api/v1/telemetry y el servidor responde JSON (Content-Format 50).
- Para otros endpoints, el payload se imprime tal cual (texto/JSON según corresponda).

## Flags
- --host HOST (requerido)
- --port N (requerido)
- --method GET|POST (requerido)
- --path /path (requerido)
- --payload STRING (solo para POST)
- --timeout MS (por defecto 1000)
- --retries N (por defecto 2)
- --non (envía NON en lugar de CON)
- --verbose

## Ejemplos adicionales
- GET status:
  ```bash
  ./bin/coapc --host 127.0.0.1 --port 5683 --method GET --path /api/v1/status
  ```
- Legacy GET /hello (si el servidor lo mantiene habilitado):
  ```bash
  ./bin/coapc --host 127.0.0.1 --port 5683 --method GET --path /hello
  ```

## Requisitos
- clang o gcc, make

## Observaciones
- Este cliente no requiere cambiar de directorio al servidor para funcionar. Solo
  necesita la IP/host y el puerto UDP del TeleServer.
- Para pruebas end-to-end, ver también el README del servidor y WARP.md.
