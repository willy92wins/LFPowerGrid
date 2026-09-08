# BRIEF — Revision adversarial de una lane de implementacion

## Tu papel
Eres el REVISOR. Otro modelo, de otra familia, acaba de implementar un encargo en este mismo
workspace. Tu no eres el que lo hizo y no tienes que defenderlo. **Modo solo lectura: no
modifiques ni un fichero de codigo.** Tu unico producto es un dictamen escrito.

## Que tienes en la raiz del workspace
- `BRIEF.md` — el encargo original que recibio el implementador. **Leelo entero primero.**
- `CAMBIOS.diff` — el diff exacto de lo que cambio, contra el commit base `8de29d5`.
- `INFORME.md` — lo que el implementador dice que hizo.
- El arbol de codigo completo, ya modificado: puedes abrir cualquier fichero para comprobar.

## Contexto del codigo
LFPowerGrid, mod de DayZ en Enforce Script. Orden de compilacion 3_Game -> 4_World -> 5_Mission:
**cada modulo solo ve los anteriores**, asi que una llamada de 4_World a algo de 5_Mission es un
fallo de compilacion. Enforce solo compila al cargar el mundo: aqui **nadie ha compilado esto**, y
tu tampoco puedes. Un error de compilacion que detectes leyendo es un hallazgo de primera.

## Lo que tienes que comprobar, en este orden

### 1. Correccion — ¿el arreglo arregla lo que dice?
Por cada ficha del `BRIEF.md`: abre el codigo y decide si el cambio cierra el defecto descrito,
lo cierra a medias, o no lo toca. **Un veredicto `ARREGLADA` en el informe no es prueba de nada:
tu juicio sale del codigo.** Busca especialmente:
- arreglos que tratan el sintoma y dejan vivo el camino de error;
- guardas nuevas que se pueden esquivar por otra rama;
- estado que se escribe pero nunca se lee, o al reves;
- cambios que rompen a un llamador que el implementador no miro.

### 2. Convenciones de Enforce — **solo en las lineas ANADIDAS del diff**
Prohibido: ternarios `? :`, `++`, `--`, `+=`, `-=`, `*=`, `/=`, `foreach`, `Print(`, y `ref` en
parametros, retornos, locales o typedefs (`ref` solo vale en miembros de clase).
Cada violacion es un hallazgo con su `path:line`.

### 3. Alcance — ¿se salio de su lista blanca?
El `BRIEF.md` trae una LISTA BLANCA de ficheros. Comprueba en `CAMBIOS.diff` que no hay ni un
fichero fuera de ella. **Hay otras cuatro lanes editando ficheros vecinos en paralelo**: un fichero
tocado fuera de la lista blanca es un choque, no un extra.

### 4. Reformateo encubierto
El repo es CRLF. Un diff inflado por normalizacion de finales de linea, reindentado o reordenado
esconde el cambio real. Si lo ves, dilo con su `path:line`.

### 5. Honestidad del informe
¿La seccion `LO QUE NO PUDE VERIFICAR` es real o es de relleno? ¿Hay algo que el informe declara
hecho y el codigo no respalda? Una cita `path:line` del informe que no case con el codigo es un
hallazgo GRAVE por si sola.

## Como clasificar cada hallazgo
- **GRAVE** — el codigo esta peor que antes, el arreglo no arregla, hay un fallo de compilacion,
  se salio del alcance, o el informe afirma algo falso.
- **MEDIO** — el arreglo es incompleto o fragil, pero no empeora nada.
- **MENOR** — estilo, convencion, comentario que miente.

**Cada hallazgo lleva `path:line` que el receptor va a abrir.** Una cita inventada invalida el
hallazgo entero y el dictamen completo pierde credibilidad. Si no estas seguro de una linea,
di el nombre de la funcion y que no pudiste fijar la linea.

## VERDE — el criterio, fijado antes de que empezaras
**VERDE = cero GRAVE vivo**, con cada GRAVE citado y verificable. MEDIO y MENOR **no impiden**
el VERDE: se anotan y se deciden aparte. No hay tercera ronda: esto se cierra aqui.

## Entregable
`DICTAMEN.md` en la raiz del workspace, en castellano:
1. **Veredicto en la primera linea**: `VERDE` o `ROJO`, y el recuento `N GRAVE / N MEDIO / N MENOR`.
2. Una seccion `###` por hallazgo: clasificacion, `path:line`, que esta mal, por que importa, y
   que habria que hacer. No escribas el arreglo: descríbelo.
3. Una tabla ficha por ficha: la del brief, el veredicto del implementador, y **el tuyo**.
4. `## QUE PUEDE ESTAR MAL EN LA PREMISA DE ESTE ENCARGO` — **obligatoria y no opcional**. El
   brief pudo pedir mal las cosas. Si crees que una ficha describe mal el bug, que el arreglo
   correcto era otro, o que el encargo presupone algo falso del codigo, **es aqui donde vale**.
   Un dictamen que solo dice "presente, colocado y bien formado" no responde a "¿es correcto?".
5. `## LO QUE NO PUDE COMPROBAR` — con el motivo de cada linea.

**No escribas codigo, no propongas parches en diff, no toques ningun fichero salvo `DICTAMEN.md`.**
