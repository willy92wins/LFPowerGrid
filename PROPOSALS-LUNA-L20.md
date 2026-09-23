# Propuestas de reducciÃ³n LOC â€” L20

Inventario tras lectura completa de `scripts/4_World/LFPG_RPCClientHandler.c` (876 lÃ­neas). Orden aproximado por lÃ­neas netas que podrÃ­an eliminarse. Â«AplicadoÂ» indica recorte directo con evaluaciÃ³n/orden y contenido del mensaje preservados; Â«diferidoÂ» identifica una reducciÃ³n aparente que no supera el criterio de equivalencia demostrable.

| # | LOC aprox. | Propuesta | Estado / motivo |
|---|---:|---|---|
| 1 | 30 | Eliminar bloques de comentarios histÃ³ricos/de versiÃ³n que describen la extracciÃ³n, dispatch ya obvio, cambios anteriores, detalles de implementaciÃ³n vanilla y seÃ±alizaciÃ³n repetida de refresh (cabecera, V4, v5, v3.2/v4.3). | Aplicado: solo comentarios; conservados los que explican contratos, lÃ­mites de seguridad, compatibilidad o decisiones no obvias. |
| 2 | 57 | Inlinear seis mensajes `perfSnapshot`/`syncMsg`/`logResp`/`logTx`/`logNA` en sus llamadas `Print`/`Debug`/`Info`. | Aplicado: cada variable se usa una vez; se conserva concatenaciÃ³n y punto de evaluaciÃ³n. |
| 3 | 12 | Inlinear mensajes temporales de error/success (`logMsg` de servidor/sorter, tres errores de cabecera de preview, `errRead`). | Aplicado: las expresiones se evalÃºan en el mismo punto y se mantienen exactamente los textos. |
| 4 | 3 | Eliminar declaraciones iniciales de `assembledPos`/`assembledOri`, que se sobrescriben sus tres componentes antes de cada inserciÃ³n. | Diferido: vectores con inicializador no son estrictamente redundantes en Enforce sin verificar semÃ¡ntica de asignaciÃ³n/estado de componentes. |
| 5 | 2 | Sustituir `readOk` por retornos directos ante lecturas truncadas del preview. | Diferido: cambiarÃ­a la ruta de diagnÃ³stico, que actualmente emite Ã­ndice de fallo; no es una eliminaciÃ³n puramente textual. |
| 6 | 1 | Quitar `if (!g_Game)` antes de `g_Game.GetPlayer()` en resync. | Diferido: guardia de null explÃ­cita; eliminarla cambia el comportamiento ante estado temprano/desconectado. |
| 7 | 1 | Quitar guardia de `camCount > LFPG_MONITOR_MAX_CAMERAS` o de `sentCount > LFPG_SORTER_PREVIEW_CAP`. | Diferido: lÃ­mites de payload deliberados; no asumir equivalencia aunque el servidor normalmente respete los topes. |
| 8 | 1 | Quitar rama `camCount <= 0` y dejar pasar la lista vacÃ­a por la ruta comÃºn. | Diferido: rama emite mensaje al jugador; altera comportamiento observable. |
| 9 | 1 | Colapsar cada guardia de `ctx.Read` en `Dispatch`/handlers o compactar retornos en una lÃ­nea. | Diferido: no reduce lÃ­neas netas con diagnÃ³sticos existentes; la forma compacta perjudica claridad y no es reducciÃ³n funcional. |
| 10 | 0 | Eliminar helpers o ramas aparentemente sin uso. | Ninguno encontrado: todos los handlers listados tienen ruta de dispatch; no hay helpers privados huÃ©rfanos en el archivo. |

No se identificaron bloques de cÃ³digo muerto ni duplicados con equivalencia segura. Se preservan checks de formato/red, ramas con efectos de UI/RPC, y el flujo de compensaciÃ³n searchlight.
