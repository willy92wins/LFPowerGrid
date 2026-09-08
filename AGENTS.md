# LFPowerGrid — instrucciones para agentes

Mod de DayZ en Enforce Script. Este fichero lo lee cualquier agente que trabaje en este repo
(Codex lo carga solo; a Claude y a los demas se les pasa en el brief). Si algo de aqui contradice
tu encargo, **manda el encargo y dilo en tu informe**.

---

## 1. EL GATE QUE HAY QUE PASAR SIEMPRE — linter offline de Enforce

**Antes de declarar bueno un cambio en `.c`, `.layout`, `config.cpp`, `inputs.xml` o `.rvmat`:**

```
python C:/Users/guill/DayZ-Modding-Knowledge-Pack/tools/dayz-script-validator/scripts/script_validator.py .
```

Tarda ~60 s sobre el arbol entero y saca **JSON**. Tres cosas que hay que saber o no sirve:

- **Las claves son `errors` y `warnings` en la raiz.** No `findings`.
- ⚠ **`status` vale `WARN` aunque `errors` sea 0, y el exit code es `0 PASS / 1 FAIL / 2 WARN`.**
  Un arbol limpio con warnings **sale con `2`**, asi que `if rc != 0` lo rechaza. Gatea por
  `len(errors)` del JSON, o trata el `2` como aprobado. Medido en este repo: 0 errores, 47
  warnings, `exit=2`.
- ⚠ **La ruta relativa `tools/dayz-script-validator/...` que aparece en varias skills es relativa
  a la raiz del Knowledge Pack**, no a este proyecto: desde aqui muere con
  `No such file or directory`, que se lee como «no esta instalado». Usa la ruta absoluta de arriba.
- **Lo que vale es el DELTA.** Correlo sobre la base (`git stash` o un worktree del commit base) y
  sobre tu arbol, y compara. Un numero absoluto no dice de quien es el error; el delta si.

**No es un compilador.** Enforce solo compila al cargar el mundo, asi que cero errores aqui **no**
sustituye un arranque. Pero **si caza una referencia colgando tras un borrado**, que es justo lo
que un grep de simbolos se deja. Si borras clases o ficheros, este gate no es opcional.

**Punto ciego MEDIDO el 2026-09-08, y costo un arranque entero:** este linter **NO ve una
variable no declarada**. Un `obj.m_Campo` donde la clase de `obj` no declara `m_Campo` pasa con
**0 errores y delta cero**, y despues el cliente muere con `Can't compile "World" script module!`
y `ACCESS_VIOLATION`. Caso real: `LFPG_CableRenderer.c:2825` incrementaba `tRnd.m_Projections`
sobre un `LFPG_RenderMetrics`; ese campo existe en `LFPG_PreviewMetrics`, no ahi. Doce lanes y una
revision adversarial de otra familia pasaron por encima, porque leido parece razonable. Los dos
arranques, el rojo y el verde, estan en `reviews/2026-09-08-gate-ingame-plan-definitivo/`.

Barrido barato que si lo caza, y cuesta segundos: por cada local tipada `LFPG_X obj = ...` de tu
diff, comprobar que cada `obj.m_Campo` que uses este declarado en `LFPG_X` o en alguno de sus
padres. Si tocas muchos ficheros, hazlo **antes** de gastar un ciclo de arranque.

Linea base conocida (2026-09-08, `adfd29c`): **263 ficheros, 0 errores, 47 warnings**. Si te salen
errores, son tuyos.

**Companero obligatorio si tocas layouts o textos** — `ui_reconcile.py`, en la misma carpeta:

```
python C:/Users/guill/DayZ-Modding-Knowledge-Pack/tools/dayz-script-validator/scripts/ui_reconcile.py .
```

Reconcilia cada `FindAnyWidget("nombre")` contra los layouts y cada `#STR_` contra la
stringtable, con sugerencia «did-you-mean» en las erratas. **Eso no lo ve ningun compilador**: un
nombre de widget mal escrito compila y falla en pantalla. `--strict` convierte sus WARN en fallo.
Linea base 2026-09-08: 9 layouts, 331 nombres, 262 claves `#STR`, **0 FAIL / 0 WARN**.

---

## 2. Orden de compilacion — se rompe en silencio

`scripts/3_Game/` → `scripts/4_World/` → `scripts/5_Mission/`. **Cada modulo solo ve los
anteriores.** Una llamada de `4_World` a algo de `5_Mission` no compila, y no te enteras hasta que
el mundo carga. `3_Game` lo ve todo el mod: un simbolo que borres ahi puede tener llamadores en los
otros dos.

---

## 3. Convenciones de Enforce — se revisan linea a linea

Prohibido en toda linea que anadas o modifiques:

| prohibido | escribe |
|---|---|
| ternario `? :` | un `if`/`else` |
| `++`, `--` | `x = x + 1;` |
| `+=`, `-=`, `*=`, `/=` | `x = x + y;` |
| `foreach` | `for (int i = 0; i < arr.Count(); i = i + 1)` |
| `Print(...)` | `LFPG_Util.Error/Warn/Info/Debug` |

**`ref` SOLO en miembros de clase.** Nunca en parametros, retornos, locales ni typedefs.
`ref array<ref X> m_Cosa;` es correcto; `ref X local = ...` dentro de una funcion, no.

Naming: miembros `m_`, estaticos `s_`, metodos PascalCase, locales camelCase, indentacion con tabs.

**El repo es CRLF.** No normalices finales de linea, no reindentes codigo ajeno, no reordenes nada
que tu encargo no pida. Un diff inflado por reformateo hace la revision imposible y se rechaza.

---

## 4. Persistencia — lo que rompe mundos de jugadores

Hay un servidor privado **con jugadores reales**. Los classnames de `CfgVehicles` estan ligados a
persistencia: **borrar o renombrar la declaracion de una entidad que algun mundo tenga colocada
rompe ese mundo.** Antes de tocar `config.cpp`, comprueba si la clase es padre de otra o si puede
estar colocada. Ante la duda, no la borres: dejala y marcalo como decision pendiente del dueno.

Caso real (2026-09-08): `LFPG_Sorter_TEST` hereda de `LFPG_Sorter` en script
(`scripts/4_World/test/LFPG_Sorter_TEST.c:15`) y en config (`config.cpp:1086`). Un plan pedia
borrar `LFPG_Sorter.c`; habria roto la compilacion de World **y** las bases de los jugadores.

---

## 5. Git

- **No commitees.** Deja los cambios en el arbol de trabajo: el commit lo pone quien revisa.
- **Nada de `git add -A` a ciegas**: este repo lo tocan varias sesiones a la vez y barrerias
  trabajo ajeno. Usa rutas explicitas.
- No hagas `merge`, `cherry-pick`, `rebase`, `reset` ni `checkout` de otra rama sin que te lo pidan.

---

## 6. Que NO puedes hacer aqui

- **No hay compilador** de Enforce invocable. No inventes un comando de build.
- **No arranques el juego.** Eso es infra aparte (DayZ-MCP + DayZDiag), y no es tuyo salvo encargo
  explicito.
- Si tu encargo depende de una prueba in-game, **hazlo y declaralo pendiente**; no lo des por
  verificado.

---

## 7. Donde esta lo demas

- Auditorias, planes, briefs, informes y dictamenes: `reviews/<fecha>-<nombre>/`.
- Estado vivo del proyecto y cola pendiente: `../LFPowerGrid_dev/HANDOFF.md`.
- Triaje de las fichas P2/P3 con veredicto y `path:line`:
  `reviews/2026-09-08-triaje-fichas-p2-p3/`.
