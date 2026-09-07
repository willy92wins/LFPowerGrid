# T0a paleta — informe

## RECUENTO

Linea base medida ANTES de tocar codigo, sobre este arbol. Instrumento: busqueda `COL_`, `LFPG_SorterView\.COL_` y `LFPG_ColorData` en los dos ficheros ATM (herramienta Grep del workspace; el shell no arranco, ver mas abajo).

| fichero | `COL_` antes | `COL_` despues | `LFPG_SorterView.COL_` antes | `LFPG_SorterView.COL_` despues | `LFPG_ColorData` |
|---|---|---|---|---|---|
| `scripts/4_World/LFPG_BTCAtmView.c` | 69 | 69 | 64 | 0 | 5 y 5 |
| `scripts/4_World/LFPG_BTCAtmController.c` | 18 | 18 | 12 | 0 | 0 y 0 |

Desglose del 69 en View (igual antes y despues): 64 usos de paleta compartida + 1 comentario `:124` + 3 declaraciones ATM-propias (`COL_AMBER_BTN`, `COL_STATUS_OK_BG`, `COL_STATUS_ERR_BG` en `:127-129`) + usos de esas locales en el cuerpo. Lo que cambio es el calificador: `LFPG_SorterView.COL_*` → `LFPG_UIPalette.COL_*` (64 hits, incluido el comentario).

Desglose del 18 en Controller: 12 usos de paleta compartida + 1 comentario `:60` + 3 declaraciones ATM-propias (`COL_STATUS_OK`, `COL_STATUS_ERR`, `COL_DIM_BG`) + usos locales. Mismo recuento `COL_` despues.

Los 69 y 18 del orquestador clavan en este arbol. La cifra vieja «76» no aplica.

## TABLA DE LOS 29 COLORES

Antes: `scripts/4_World/LFPG_SorterView.c` (bloque que estaba en `:159-193` al medir). Despues: `scripts/3_Game/LFPG_UIPalette.c:28-62`. Leidos del codigo, no de memoria.

| constante | valor antes | valor despues | igual? |
|---|---|---|---|
| COL_AMBER | 0xFFFBBF24 | 0xFFFBBF24 | si |
| COL_BG_DEEP | 0xFF131C2B | 0xFF131C2B | si |
| COL_BG_ELEVATED | 0xE61E2B41 | 0xE61E2B41 | si |
| COL_BG_INPUT | 0xFF202E4C | 0xFF202E4C | si |
| COL_BG_PANEL | 0xF5121C36 | 0xF5121C36 | si |
| COL_BG_RULES_PANEL | 0xFF1E2B41 | 0xFF1E2B41 | si |
| COL_BG_SECTION | 0xEB162036 | 0xEB162036 | si |
| COL_BG_SECTION_CARD | 0xE61E2B41 | 0xE61E2B41 | si |
| COL_BLUE | 0xFF60A5FA | 0xFF60A5FA | si |
| COL_BLUE_BTN | 0xFF274B7C | 0xFF274B7C | si |
| COL_BTN | 0xFF374B6F | 0xFF374B6F | si |
| COL_CATCHALL_BG | 0x26FBBF24 | 0x26FBBF24 | si |
| COL_GREEN | 0xFF34D399 | 0xFF34D399 | si |
| COL_GREEN_BORDER | 0x3334D399 | 0x3334D399 | si |
| COL_GREEN_BTN | 0xFF087C5B | 0xFF087C5B | si |
| COL_GREEN_DIM | 0x1734D399 | 0x1734D399 | si |
| COL_HEADER | 0xF50F172B | 0xF50F172B | si |
| COL_INPUT_BORDER | 0x4CCBD5E1 | 0x4CCBD5E1 | si |
| COL_PAIRING_ERR | 0x50F87171 | 0x50F87171 | si |
| COL_PAIRING_OK | 0x5034D399 | 0x5034D399 | si |
| COL_PURPLE | 0xFFA78BFA | 0xFFA78BFA | si |
| COL_RED | 0xFFF87171 | 0xFFF87171 | si |
| COL_RED_BTN | 0xFFC72323 | 0xFFC72323 | si |
| COL_RED_BTN_BORDER | 0x40F87171 | 0x40F87171 | si |
| COL_RED_BTN_SOFT | 0x26F87171 | 0x26F87171 | si |
| COL_SEPARATOR | 0x43CBD5E1 | 0x43CBD5E1 | si |
| COL_TEXT | 0xFFF1F5F9 | 0xFFF1F5F9 | si |
| COL_TEXT_DIM | 0xFF7A8A9B | 0xFF7A8A9B | si |
| COL_TEXT_MID | 0xFFB0BEC5 | 0xFFB0BEC5 | si |

`COL_BG_ELEVATED` y `COL_BG_SECTION_CARD` siguen siendo dos constantes con el mismo hex.

## QUE HICE

- **Donde puse el modulo y por que.** `scripts/3_Game/LFPG_UIPalette.c`. `gameScriptModule` carga `LFPowerGrid/scripts/3_Game` (`config.cpp:218`) antes que `worldScriptModule` carga `LFPowerGrid/scripts/4_World` (`config.cpp:223`). ATM y sorter V3 viven en World; el tipo tiene que existir antes. No lo puse en `4_World`: el orden de ficheros dentro del modulo es alfabetico y `LFPG_BTCAtmView.c` compilaria antes que un `LFPG_UIPalette.c` vecino. El orden de modulos Enforce es el ancla que el brief pide.

- **Que declare y donde.** Nada. `files[]` apunta al directorio, no a ficheros sueltos. Un `.c` nuevo en `3_Game` entra solo. No toque `config.cpp`.

- **Contenido del modulo.** `class LFPG_ColorData extends Managed` (antes `LFPG_SorterView.c:45`) y `class LFPG_UIPalette` con las 29 `static const int COL_*` y los hex originales. `#ifndef SERVER` como `LFPG_UIScaler.c`: paleta y ColorData son de UI cliente.

- `path:line` tocados:
  - `scripts/3_Game/LFPG_UIPalette.c` (nuevo): ColorData `:15-23`, paleta `:25-63`
  - `scripts/4_World/LFPG_SorterView.c`: clase `LFPG_ColorData` eliminada; bloque hex eliminado; usos de paleta calificados a `LFPG_UIPalette.COL_*` (p. ej. `ApplyColors` `:564-665`, pairing `:1094-1112`). El miembro `ref array<ref LFPG_ColorData> m_ColorDataRefs` sigue en `:58`
  - `scripts/4_World/LFPG_BTCAtmView.c`: comentario `:124`; `ApplyColors` `:567-667` ahora `LFPG_UIPalette.COL_*`. Las 5 refs a `LFPG_ColorData` (`:36`, `:169`, `:702`, `:708`, `:719`) no cambian de nombre; resuelven al tipo de 3_Game
  - `scripts/4_World/LFPG_BTCAtmController.c`: comentario `:60`; usos `:186`, `:190`, `:418`, `:421`, `:424`, `:436`, `:437`, `:445`, `:446`, `:803`, `:807`
  - `scripts/4_World/LFPG_SorterController.c`, `LFPG_SorterTagView.c`, `LFPG_SorterPreviewRow.c`: `LFPG_SorterView.COL_*` → `LFPG_UIPalette.COL_*` (60 + 2 + 6). Hace falta para que V3 siga compilando una vez las constantes ya no son miembros de `LFPG_SorterView`. No es retarget del ATM; es el punto 4 del alcance («la V3 consume el modulo nuevo»). Primera idea (aliases `static const int COL_X = LFPG_UIPalette.COL_X` en SorterView) la descarte: ese patron de init no existe en este arbol, y si Enforce lo rechaza tumba el modulo World entero, ATM incluido. El uso como expresion (`Foo.COL_BAR` en un `SetColor`) ya estaba en produccion.

## QUE NO CUBRE

- Borrar V3 sigue bloqueado por sitios que no son paleta: `LFPG_MissionInit.c` llama `LFPG_SorterView.Init/IsOpen/HandleEscKey/Close/Cleanup` (`:154`, `:182`, `:186`, `:233`, `:256`, `:274`, `:281`, `:479`). `LFPG_RPCClientHandler.c` y la entidad/accion del sorter no se tocaron (otras lanes / fuera de alcance).
- ATM View `:692` sigue siendo un comentario que apunta a `LFPG_SorterView.c` F1-B (patron SetUserData). No es dependencia de compilacion.
- `LFPG_ColorData_TEST` y las `COL_*` de `scripts/4_World/test/LFPG_SorterView_TEST.c` siguen duplicadas a proposito.
- Constantes ATM-propias (`COL_AMBER_BTN`, `COL_STATUS_*`, `COL_DIM_BG`) no eran de las 29; se quedan en el ATM.

Tras este parche, borrar `LFPG_SorterView.c` **no** rompe la compilacion del ATM. Si rompe el MissionInit. Esta extraccion quita al ATM como bloqueador; no habilita el borrado de V3 ella sola.

## SALIDA DE LOS GREPS DEL GATE

El shell de este host no ejecuta. Tres hooks de PowerShell (`launch-ledger.ps1`, `prime-agent-skills-gate.ps1`, `gpu-lease-gate.ps1`) mueren con `syntax error near unexpected token '&'` en el wrapper Pre. Probe `echo hello` y `python -c "print('ok')"`: ambos rejected. No hay stdout de `grep` ni de `python enfcheck.py`.

Equivalente con la herramienta Grep del workspace, mismos patrones y rutas:

```
patron LFPG_SorterView\.COL_
  scripts/4_World/LFPG_BTCAtmView.c        → No matches found
  scripts/4_World/LFPG_BTCAtmController.c  → No matches found

patron COL_  (count)
  scripts/4_World/LFPG_BTCAtmView.c        → 69
  scripts/4_World/LFPG_BTCAtmController.c  → 18
```

Control positivo del instrumento (mismo patron, sitio donde SI esta): `LFPG_UIPalette.COL_` cuenta 64 en `LFPG_BTCAtmView.c` y 12 en `LFPG_BTCAtmController.c`. El cero de `LFPG_SorterView.COL_` no es un grep ciego.

Control extra: `LFPG_SorterView\.COL_` sobre `scripts/` entero → No matches found (TEST usa `LFPG_SorterView_TEST.COL_`, no casa).

## LO QUE NO PUDE VERIFICAR

- Compilacion Enforce (el juego no se lanza aqui). El fallo tipico de esta extraccion es orden de carga; por eso el fichero esta en 3_Game, no un test de syntax.
- `python enfcheck.py` sobre los ficheros tocados. Equivalente a mano sobre **lineas nuevas** de `LFPG_UIPalette.c`: comillas pares (el unico `"` es `"normalize"` en un comentario, 2 por linea); `{`/`}` = 3/3; `(`/`)` = 10/10 (incluye comentarios); cero escapes adyacentes; cero `?:`, `++`, `--`, `+=`, `-=`, `foreach` en codigo. Los retargets del ATM y V3 solo cambian el nombre de clase delante de `COL_*` ya existentes.
- Herencia de `static const` entre clases: no se uso. Los consumidores leen `LFPG_UIPalette.COL_*` en expresiones, el mismo mecanismo que ya usaban con `LFPG_SorterView.COL_*`.

## LA PREMISA DE ESTE ENCARGO

El modulo neutro en 3_Game es la forma correcta de romper el acoplamiento **de compilacion** ATM↔V3. Alternativa mas simple: copiar los 29 hex al ATM y mover solo `LFPG_ColorData`. Eso tambien desbloquearia borrar V3 respecto del ATM, pero reabre el drift F2-B (dos paletas). Una vez ColorData tiene que salir de SorterView, poner las constantes al lado es el movimiento minimo, no uno extra.

Esta extraccion **no basta** para el objetivo declarado «poder borrar la V3». El ATM ya no es el bloqueador de paleta. Sigue haciendo falta: MissionInit (Init/ESC/Cleanup de `LFPG_SorterView`), handlers V3 en RPC cliente, accion/entidad sorter, y la decision V4 `_TEST`. Nada de eso es paleta.

No hay otra dependencia **de tipo** del ATM hacia V3 aparte de la paleta + ColorData (ColorData ya no vive en V3). Lo que queda es comentario `:692` y convivencia en MissionInit (llamadas independientes, no un tipo compartido).

## VISTO-DE-PASO

- Unificar `LFPG_ColorData` con `LFPG_ColorData_TEST` (SetUserData / heap `0xc0000374`): fuera de alcance, no se toco.
- V3 Controller/TagView/PreviewRow tenian que dejar de hablar de `LFPG_SorterView.COL_*` o V3 no compilaba. Los retargete a `LFPG_UIPalette`. El brief decia «solo el ATM»; sin este paso el punto 4 (V3 no se rompe) falla. TEST no se toco.
- `LFPG_MissionInit.c` sigue atando el ciclo de vida de V3; otras lanes sobre ese fichero, no se toco.
