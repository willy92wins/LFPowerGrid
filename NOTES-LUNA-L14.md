# Notas — Luna L14

## Resultado

- Archivo: `scripts/4_World/LFPG_Intercom.c`
- LOC antes: 1047
- LOC después: 946
- Reducción neta: 101 líneas
- Write-set: solo el archivo anterior; `PROPOSALS-LUNA-L14.md` y este reporte acompañan la revisión solicitada.

## Cambios

- Eliminado el campo y los tres bloques de telemetría de acciones clientes que estaban anidados dentro de métodos `#ifdef SERVER` y por tanto no podían compilarse para cliente.
- Eliminadas tres constantes sin referencias en el árbol (`LFPG_INTERCOM_HS_CAMO`, `LFPG_INTERCOM_HS_SCREEN`, `LFPG_SND_NONE`).
- Reducidas ramas equivalentes para inversiones booleanas, comparación del umbral, valores de animación/material vacíos y presencia de ghosts.
- Inlineados alias locales de un solo uso en registro de puertos/SyncVars, checks de adjuntos, errores de persistencia y lectura del port de entrada.
- No se quitaron logs funcionales, guards de entrada ni lecturas/reparaciones de persistencia.

## Verificación

- Se leyó el archivo completo antes de redactar propuestas y editarlo.
- Búsqueda de referencias por `scripts/` y `config.cpp`: constantes eliminadas sin uso; getters y helpers restantes tienen consumidores.
- Validador oficial: `status=WARN`, `errors=[]`, 253 archivos escaneados. Avisos del repositorio; el gate del repo toma la decisión por `errors`, no por `status`. Se omitió `ES-UNDEFINED-CLASS-REF` porque no se encontró el árbol vanilla.
- El validador no es compilador; no hubo compilación Enforce ni prueba in-game.
- El `HANDOFF.md` actual prescrito no está en la ruta hermana del worktree, así que no se pudo comparar con el estado vivo indicado.
- `git diff --check` señala CR al final de líneas añadidas por el formato CRLF requerido por el repo. Se conservó CRLF y se revisó el diff; no son espacios añadidos al contenido.

## Riesgo residual

Las simplificaciones son equivalencias locales de control/argumentos y las declaraciones quitadas no tenían usos. Falta confirmación de compilación/carga Enforce e in-game. El linter reporta avisos preexistentes y no pudo ejecutar la regla que depende de vanilla.
