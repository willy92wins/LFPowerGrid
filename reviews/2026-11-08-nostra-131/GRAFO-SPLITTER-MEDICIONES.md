# Mediciones: puertas logicas y Laser detras de un Splitter o en paralelo a Power

Medido en partida el 2026-11-09 (00:17-01:08 hora local) en DayZDiag con la mision `lfpg_grafo` y el
banco GS-02 (`wire`, `switch_set`, `query`, `logic_state`, `mcell_state`, `laser_state`, `veh_*`).
Estos dos fallos no se tocan en la rama hasta que Guillermo decida: requieren cambiar el grafo.

- **BASE** = PBO `1326FCC3` (b225206, PR #96).
- **NUEVO** = PBO de 734906f para A y B; PBO `CB3E6588` (b5834ff) para D5. El grafo no cambia entre
  ellos ni respecto a BASE.

Logs completos en `C:\tmp\lfpg-grok\runs\n131\evidence\` de esa maquina (`base-*.log`, `new-*.log`).
Los ejes de las tablas estan pegados tal como salieron.

## 1. Puertas logicas detras de un Splitter

Montaje A del plan: `generador -> Splitter; Splitter o1 -> interruptor A -> puerta in0;
Splitter o2 -> interruptor B -> puerta in1; puerta out -> lampara (LFPG_CeilingLight, 10 u/s)`.

Tabla de la lampara, 3 s por combinacion:

| Puerta | Esperada | BASE | NUEVO |
|---|---|---|---|
| AND | 0001 | 0000 | 0000 |
| OR | 0111 | 0001 | 0001 |
| XOR | 0110 | 0000 | 0000 |

Control, el mismo montaje sin Splitter (cada interruptor en su salida del generador):

| Puerta | Esperada | BASE | NUEVO |
|---|---|---|---|
| AND | 0001 | 0001 | 0001 |
| OR | 0111 | 0111 | 0111 |
| XOR | 0110 | 0110 | 0110 |

Ciclo observado en BASE, XOR con A encendido y B apagado, una muestra por segundo. El ciclo dura unos
4 s y la lampara no enciende nunca:

```
00   gen[p1 i0 o50] spl[p1 i2 o2]   a[p1 i1 o1]  b[p1 i1 o1] g[p0 i0 o1]  l[p0 i0 o10] gate in 0 0 out 0
t=1  gen[p1 i0 o50] spl[p1 i11 o2]  a[p1 i1 o1]  b[p1 i1 o1] g[p1 i1 o10] l[p0 i0 o10] gate in 1 0 out 1
t=2  gen[p1 i0 o50] spl[p1 i2 o2]   a[p1 i1 o1]  b[p1 i1 o1] g[p1 i1 o10] l[p0 i0 o10] gate in 1 0 out 1
t=3  gen[p1 i0 o50] spl[p1 i2 o11]  a[p0 i0 o10] b[p0 i0 o1] g[p0 i0 o10] l[p0 i0 o10] gate in 0 0 out 1
t=4  gen[p1 i0 o50] spl[p1 i11 o11] a[p0 i0 o10] b[p0 i0 o1] g[p0 i0 o10] l[p0 i0 o10] gate in 0 0 out 0
t=5  gen[p1 i0 o50] spl[p1 i11 o2]  a[p1 i1 o1]  b[p1 i1 o1] g[p1 i1 o10] l[p0 i0 o10] gate in 1 0 out 1
```

Lectura: `p` powered, `i` entrada, `o` salida, por nodo.

Caso A4 en BASE (XOR, salida sin cable, A encendido): **73 cambios de estado en 60 s**. Tramo final
(`t`, in0, in1, out, powered):

```
t=51.2 (1, 0, 1, 1)
t=52.0 (0, 0, 0, 0)
t=52.8 (0, 0, 1, 0)
t=53.6 (1, 0, 1, 1)
t=54.3 (0, 0, 0, 0)
t=55.0 (0, 0, 1, 0)
t=55.8 (0, 0, 0, 0)
t=57.3 (1, 0, 1, 1)
t=58.1 (0, 0, 0, 0)
t=58.8 (0, 0, 1, 0)
t=59.6 (1, 0, 1, 1)
```

XOR con Door Controller (caso B), con plazo de 5 s por cambio:

| Montaje | BASE | NUEVO |
|---|---|---|
| Detras de Splitter | FALLA: `A on` sin abrir a los 5,3 s y la secuencia queda invertida | Pasa en esa corrida, respuestas de 0,2-0,8 s, **no fiable**: una sola corrida y sin muestreo sostenido |
| Sin Splitter | PASA (2,0-2,9 s) | PASA (2,0 s) |

## 2. Laser -> Set de una Memory Cell con Power aparte (escenario de Nostra)

Montaje: `generador o1 -> Laser in; generador o2 -> celda Power; generador o3 -> pulsador -> Reset;
Laser out -> celda Set; celda Output -> lampara`. NUEVO, coche con conductor cruzando el haz:

```
side 10 gate 0 set 0 active 0 lamp False
side 6  gate 0 set 0 active 0 lamp False
side 3  gate 0 set 0 active 0 lamp False
side 2  gate 1 set 0 active 0 lamp False
side 1  gate 1 set 0 active 0 lamp False
side 0  gate 1 set 0 active 0 lamp False
side -1 gate 1 set 0 active 0 lamp False
side -2 gate 1 set 0 active 0 lamp False
side -3 gate 1 set 0 active 0 lamp False
side -6 gate 0 set 0 active 0 lamp False
```

La compuerta abre y Set no recibe nunca. Este montaje no se corrio en BASE: alli el coche no abre la
compuerta (caso D2).

Variantes en NUEVO:

- **Sin cable en Power** (Laser como unica alimentacion), con jugador a pie:
  `gate 1 set 1 active 1 lamp True`; al salir del haz, `gate 0 set 0 active 1 pow 0 lamp False`. El
  latch se pone, pero la lampara se apaga con el Laser.
- **Laser -> interruptor encendido -> Set, con Power aparte**, jugador a pie: `gate 1` con `set 0 active 0`. Tampoco.
- **Interruptor en Set con Power aparte, sin Laser** (con y sin Splitter): `set 1 active 1 lamp True`. Funciona.

Causa por lectura de codigo, coherente con las medidas pero sin medir dentro del grafo:

- `scripts/5_Mission/LFPG_ElecGraphImpl.c:4022-4034`, `PassthroughSupplyAt`: la oferta de un PASSTHROUGH
  es `entrada + virtual - consumo`. Con el Laser pidiendo solo sus 5 u/s de autoconsumo sale 5 - 5 = 0.
- `:4015`, `IncludeInMergerVector`: un proveedor con oferta 0 no entra en el reparto de la demanda de la
  celda. Power cubre toda la demanda, el Laser no recibe demanda de la celda y su salida sigue a 0.

Un interruptor consume 0, asi que su oferta con la sonda de 1 u/s es mayor que 0 y si entra en el reparto.
Eso explicaria por que el interruptor funciona y el Laser no. La variante con interruptor detras del Laser
no queda explicada por esta lectura: ese interruptor tambien se queda sin energia.

## Lo que no se ha medido

- Si los dos fallos tienen la misma raiz en el grafo o son dos problemas.
- PressurePad y otros aparatos con autoconsumo en la misma posicion que el Laser: por lectura caerian en
  el mismo caso, sin probar.
- El XOR con Door Controller detras de Splitter, con varias corridas y muestreo sostenido.
