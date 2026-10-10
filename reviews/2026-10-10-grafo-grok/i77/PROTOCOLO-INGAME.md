# PROTOCOLO-INGAME — i77 reloj de cargadores

Estilo de la prueba del 5-oct: un cargador vanilla, cronometro, lectura de energia CompEM / quantity. Tasa declarada `LFPG_CHARGER_ENERGY_PER_SEC = 1.0 u/s`. Tolerancia: ±1,0 u o ±1,0 s (cola de un tick), salvo donde se indique otra.

Dispositivo comun: `BatteryCharger` vanilla en el grafo, alimentado, switch ON, `CarBattery` o `TruckBattery` en `LargeBattery`, energia inicial anotada `E0`. Un solo cargador registrado salvo G02-B.

## G03-a — switch observado tarde

1. Anotar `E0`. Corriente ON, 20 s. Esperado: `E ≈ E0+20` (±1).
2. En t=20 s apagar el **interruptor del cargador** (no cortar el cable). Dejar 30 s. Esperado: incremento ≤ 1 u respecto al valor al apagar (C=1, lote 32, periodo 1 s).
3. Encender de nuevo 15 s. Esperado: `+15` (±1) sobre el valor al reencender.
4. Variante bateria llena: cargar hasta tope; 20 s extra con switch ON y corriente. Esperado: +0 (±1).
5. Si hay N>32 cargadores en el registro, el techo del paso 2 pasa a `ceil(N/32)` u.

## G03-b — misma bateria fuera y de vuelta

1. Corriente ON, 5 s. Esperado `+5` (±1). Anotar `E1`.
2. Extraer la misma bateria. **Leer la energia en la mano / suelo en el instante de sacarla (`E_out`)**. Esa lectura distingue el orden del motor:
   - `E_out ≈ E1 + tiempo desde el ultimo tick` (±1): el slot seguia lleno en `EEItemDetached` (el intervalo abierto se acredito; offline 24 u en la secuencia 1→2→40→63).
   - `E_out ≈ E1` (±1): el slot ya estaba vacio (se perdio el intervalo abierto; offline 23 u).
3. Dejarla 20 s en el suelo, volver a insertarla en el mismo cargador.
4. Esperar 5 s con corriente. Esperado: desde `E_out` sube ~5 u (±1), no ~25. El hueco de 20 s no se acredita.
5. Control negativo: repetir sin extraer (25 s continuos) → `+25` (±1).

## G03-c — reentrada

No hay protocolo de jugador. Si un mod engancha `AddEnergy`, repetir G03-b y G03-a con ese mod cargado: un intervalo de 10 s debe dar +10 (±1), no +20.

## G03-d — reemplazo por otra bateria

1. Bateria A, 5 s de carga, anotar `EA`.
2. Extraer A, insertar B (energia `EB`) **en el mismo segundo si se puede**.
3. 10 s de carga. Esperado: B recibe ~10 u (±1), no ~15. A no sube tras extraerse (salvo ≤1 u de cola si el hook no corrio antes del poll).
4. Perdida maxima documentada de A: `ceil(C/32)` u. Con un cargador: 1 u.

## G02 — tasa vs tamano y backlog

### A. Un cargador, grafo pequeno vs grande

1. Grafo con ~pocos nodos (p.ej. 1 cargador + 1 fuente). 60 s alimentados. Esperado `+60` (±1), como el 5-oct (63,8 s → +63,1).
2. Mismo cargador en un grafo con muchos nodos (cientos). 60 s alimentados. Esperado `+60` (±1), **la misma** energia por segundo alimentado.
3. 30 s de apagon en ambos. Esperado cola ≤1 u, no el tiempo de apagon.

### B. Muchos cargadores (latencia del round-robin)

1. Registrar C=33 cargadores (o el maximo practico). Medir el tiempo entre dos incrementos de **un** cargador concreto. Esperado: ~2 s entre visitas (`ceil(33/32)*1`), no 33 s.
2. Energia en 64 s alimentados en ese cargador: ~64 u (±2), no ~32 u.

### C. Backlog de ProcessDirtyQueue

1. Provocar cola sucia (conectar/desconectar cables en cadena) mientras el cargador esta alimentado 40 s.
2. Esperado: `+40` (±2). El presupuesto de PDQ no debe recortar el reloj (el tick de cargadores es el scheduler de simples, no PDQ).

## Lecturas

- Energia: CompEM `GetEnergy` o quantity normalizada × `GetEnergyMax`.
- Anotar C (cargadores en el grafo), duracion de cada tramo, `E` inicial y final.
- Fallo: energia/tiempo alimentado fuera de `1.0 ± 0.05 u/s` en tramos ≥30 s con C≤32.
