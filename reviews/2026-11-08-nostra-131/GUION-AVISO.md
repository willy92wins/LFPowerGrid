# Guion para Guillermo: ver el aviso de cable sustituido en pantalla

Unos dos minutos, en el servidor de pruebas con el PBO de la rama, nunca en el de los jugadores.

Lo que ya esta medido sin ti: el servidor envia el mensaje con el texto exacto una sola vez en E1 y E2,
no lo envia en un reroute (E3) ni en un puerto libre (E4), y con el ajuste a `false` no envia nada (E5).
Lo unico sin verificar es que el jugador lo vea: la captura automatica, a ~1 s del cableado, no lo mostro.
El cliente lo saca con `player.MessageStatus("[LFPG] " + msg)` (`scripts/4_World/LFPG_RPCClientHandler.c:106`).

1. Coloca un generador y dos lamparas, y coge un carrete de cable en las manos.
2. Cablea una salida del generador a la lampara 1.
3. Cablea **esa misma salida** a la lampara 2.
4. Mira el chat, abajo a la izquierda. Tiene que salir:
   `[LFPG] Replaced 1 existing cable: each port holds only one cable.`
5. Vuelve a tender el cable a la lampara 2 por otro camino: no tiene que salir nada.
6. Cablea otra salida libre a otra lampara: no tiene que salir nada.

Si en el paso 4 no aparece, dimelo: el envio funciona, asi que el fallo estaria en como se muestra.

Por si quieres apagarlo: `"NotifyWireReplaced": false` en
`$profile:LF_PowerGrid\LF_PowerGrid.json` y reinicia el servidor.
