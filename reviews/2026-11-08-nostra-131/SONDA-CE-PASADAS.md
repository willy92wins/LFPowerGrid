# Sonda de las pasadas de guardado de la CE

Medido el 2026-11-09, 02:19-02:35 hora local, en DayZDiag con la mision `lfpg_grafo`. Sirve para fijar
cuantos estados guarda el diario y su TTL. Solo este informe entra en la rama.

## Resumen

- En las cuatro muertes, los 69 duenos comparados cargaron el hash de su ultima emision. Ninguno cargo
  algo mas viejo ni distinto.
- Cada dueno salio exactamente una vez por pasada de emision, y los duenos sin cambios tambien se reemiten.
- Las pasadas no se solaparon: terminan unos 14 s antes de que empiece la siguiente.
- No se pudo matar en la ventana entre una emision y la escritura de su fichero, que es justo donde `INFORME-2.md`
  y WJ-01 dejan la duda. **Esta sonda no prueba que esa ventana sea inofensiva** (ver Limites).

## Montaje

- PBO desechable `8DDE8D76`: copia de a20d759 con una linea de log en `LFPG_OnStoreSaveExtra`,
  `LFPG_OnStoreLoadExtra` y el `OnStoreSave` de `LFPG_Generator` (id, `GetTime`, longitud y `string.Hash`).
- Mundo nuevo: 23 Splitters y 2 Generators cableados, mas 46 lamparas. En total 71 entidades que emiten.
- 21 duenos cambian de cables cada 20 s (`cutall` y `wire`, por el camino real del gestor). 4 duenos no cambian.
- Mtimes de `storage_1/data/*` muestreadas cada 0,5 s.
- Las muertes son `dayz_test_stop` (kill forzado). La orden tarda hasta 5 s en matar, asi que la hora de cada
  muerte es la de la ultima escritura en disco.

## Emisiones por pasada

| Arranque | Inicio | Duracion s | Lineas | Duenos distintos | Max por dueno | Intervalo s | Hash distinto al anterior |
|---|---|---:|---:|---:|---:|---:|---:|
| 1 (mundo nuevo) | 02:21:06 | 1 | 71 | 71 | 1 |  | 0 |
| 1 | 02:21:59 | 1 | 71 | 71 | 1 | 53 | 21 |
| 1 | 02:22:51 | 1 | 71 | 71 | 1 | 52 | 21 |
| 1 | 02:23:44 | 0 | 71 | 71 | 1 | 53 | 21 |
| 2 | 02:25:28 | 0 | 71 | 71 | 1 |  | 0 |
| 2 | 02:25:52 | 1 | 71 | 71 | 1 | 24 | 4 |
| 2 | 02:26:47 | 1 | 71 | 71 | 1 | 55 | 21 |
| 2 | 02:27:39 | 1 | 71 | 71 | 1 | 52 | 21 |
| 3 | 02:29:05 | 0 | 71 | 71 | 1 |  | 0 |
| 3 | 02:29:31 | 1 | 71 | 71 | 1 | 26 | 25 |
| 3 | 02:30:24 | 0 | 71 | 71 | 1 | 53 | 21 |
| 3 | 02:31:16 | 1 | 71 | 71 | 1 | 52 | 21 |
| 4 | 02:32:08 | 0 | 71 | 71 | 1 |  | 0 |
| 4 | 02:32:35 | 1 | 71 | 71 | 1 | 27 | 25 |
| 4 | 02:33:27 | 1 | 71 | 71 | 1 | 52 | 21 |
| 5, apagado limpio | 02:34:31 | 0 | 71 | 71 | 1 |  | 0 |
| 5 | 02:34:57 | 1 | 71 | 71 | 1 | 26 | 25 |
| 5 | 02:35:12 | 0 | 71 | 71 | 1 | 15 | 0 |

Hechos:

- Todos los duenos salen juntos en una rafaga de 0-1 s, y exactamente una vez por rafaga.
- En cada arranque hay una rafaga en el segundo de la carga y otra 24-27 s despues. Despues, una cada 52-55 s.
- La linea `25` de la ultima columna es la segunda rafaga tras arrancar de una muerte: los 4 sin cambios
  tambien cambian de hash ahi. El arranque 2 da solo 4 en esa rafaga. No he investigado por que.

## Duenos sin cambios

Comparando cada dueno con su emision anterior en la misma sesion, en todas las rafagas:

| Dueno | Mismo hash | Hash distinto |
|---|---:|---:|
| Sin cambios (4) | 36 | 16 |
| Cambiante (21) | 42 | 231 |

Los duenos sin cambios **se reemiten en cada rafaga**, tambien las 46 lamparas sin cables. Sus 16 hash distintos caen en la segunda rafaga tras arrancar de una muerte, sin que nadie cambiara cables. No he mirado que difiere; por su momento podrian ser los net ids que refresca la validacion (hipotesis). Si es asi, un JSON cargado y el siguiente emitido no coinciden aunque los cables sean los mismos.

## Ficheros de `storage_1/data` por pasada

Primera y ultima modificacion. La copia va rotando: `.bin`, `.001`, `.002`.

| Pasada | Copia | dynamic | vehicles | building | doors/events/types |
|---|---|---|---|---|---|
| 3 | bin | 02:20:58-02:21:12 | 02:21:13 | 02:21:13-02:21:36 | 02:21:36 |
| 4 | 001 | 02:21:50-02:22:05 | 02:22:05 | 02:22:05-02:22:28 | 02:22:29 |
| 5 | 002 | 02:22:43-02:22:57 | 02:22:57 | 02:22:57-02:23:21 | 02:23:21 |
| 6 | bin | 02:23:35-02:23:50 | 02:23:50 | 02:23:50-02:24:13 | 02:24:13 |
| 7 (muerte 1) | 001 | 02:24:28-02:24:34 | - | - | - |
| 10 | bin | 02:26:38-02:26:53 | 02:26:53 | 02:26:53-02:27:16 | 02:27:17 |
| 11 | 001 | 02:27:31-02:27:45 | 02:27:45 | 02:27:45-02:28:09 | 02:28:09 |
| 14 | bin | 02:30:15-02:30:29 | 02:30:30 | 02:30:30-02:30:53 | 02:30:54 |
| 15 (muerte 3) | 001 | 02:31:08-02:31:21 | - | - | - |
| 18 (muerte 4) | bin | 02:33:19-02:33:33 | 02:33:33 | 02:33:33-02:33:46 | - |

Faltan en la tabla las pasadas del arranque, porque se mezclan con la carga. Lista completa en
`C:\tmp\lfpg-grok\runs\n131\evidence\sonda-tablas.md` de esa maquina.

Hechos:

- Una pasada completa dura unos 38-39 s en disco: `dynamic` 14-15 s, despues `building`, y al final
  `doors/events/types`. Empieza la siguiente unos 14 s despues, cada 52-53 s.
- **No se solapan**: en las pasadas completas de la tabla, la siguiente empieza despues de la ultima escritura de la anterior.
- La rafaga de emisiones cae a unos 8-9 s de empezar `dynamic` (02:23:44 en la pasada 6, 02:31:16 en la 15, 02:33:27 en la 18).

Esto corrige dos cifras de `BRIEF-DIARIO.md` v2, que venian de contar solo `dynamic`: la vuelta de
«13-15 s» dura en realidad unos 38-39 s, y `PASS_GAP` de 20 s es menor que esa duracion. Con 52-53 s entre rafagas,
dos emisiones seguidas estan separadas mas que una pasada completa **en este mundo**.

## Que cargo la CE en cada muerte

Hash cargado de cada dueno frente a los emitidos antes de la muerte.

| Muerte | Momento | Ultima rafaga | Ultima escritura | Cargados | Ultima emision | Una anterior | Sin coincidencia | Recovery stamp |
|---|---|---|---|---:|---:|---:|---:|---:|
| 1 | Pasada 7, 6 s tras empezar, antes de su rafaga | 02:23:44 | 02:24:34 | 69 | 69 | 0 | 0 | 7 |
| 2 | Entre pasadas (tras 02:28:09) | 02:27:39 | 02:28:09 | 69 | 69 | 0 | 0 | 11 |
| 3 | Pasada 15, tras su rafaga, con `dynamic` a medias | 02:31:16 | 02:31:21 | 69 | 69 | 0 | 0 | 15 |
| 4 | Pasada 18, tras `dynamic`, con `building` a medias | 02:33:27 | 02:33:46 | 69 | 69 | 0 | 0 | 18 |

- Los stamps salen de la linea `[CE][Hive] :: Storage load` del arranque siguiente.
- En la muerte 1 la CE cargo lo de la pasada anterior (no habia otra emision). En 3 y 4 cargo lo de la pasada en
  curso aunque no habia terminado. No he visto de que fichero concreto (`.bin`, `.001`) salio cada dueno.
- Los cables hechos despues de la ultima rafaga se perdieron en las cuatro muertes, como se esperaba: la ultima
  emision no los incluye.

## Apagado limpio

**Si hay una pasada final con todos.** `shutdown_clean` a las 02:35:05; rafaga de 71 lineas a las 02:35:12, a 15 s de
la anterior, y la escritura termina a las 02:35:12. `Termination successfully completed` a las 02:35:15.

## Limites

- **Ventana no alcanzada.** Las escrituras de `dynamic` del fichero de estos duenos siguen a su rafaga muy de cerca, y la
  orden de muerte tarda hasta 5 s. La muerte 3 cayo 5 s despues de la rafaga y el fichero ya estaba escrito. No he
  conseguido matar entre la emision y la escritura de ese fichero. Por tanto, el caso WJ-01 (la CE recupera el
  estado anterior tras haberse emitido el nuevo) **no se ha observado ni descartado**.
- **Generators sin comparar.** Solo instrumente su `OnStoreSave`, no su `OnStoreLoad`: por eso son 69 cargados de
  71 emisores. No se les ha comparado lo cargado.
- **Mundo casi vacio** en DayZDiag, con los duenos juntos en una zona. El ritmo y las duraciones en un servidor con
  miles de entidades o en retail no estan medidos. Con duenos repartidos en varios ficheros `dynamic`, una rafaga
  podria no ser una sola.
- **`.002` y mezcla por fichero.** Ninguna muerte llego a cargar `.002` ni mas viejo. Con un `.bin` danado no se ha
  probado. `INFORME-2.md` vio mezcla por fichero en un kill a mitad de vuelta.
- Cuatro muertes son pocas.
- Hash de 32 bits y longitud: una colision es improbable pero posible. Solo se comparo el hash.
