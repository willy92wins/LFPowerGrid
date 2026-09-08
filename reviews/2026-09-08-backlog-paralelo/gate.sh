#!/bin/bash
# Gate mecanico de una lane. Uso: gate.sh <workspace-dir>
# Corre SOLO sobre las lineas ANADIDAS del diff. Cuidado: en BRE `\+` es "uno o mas",
# asi que las cadenas se buscan con grep -F sobre el texto ya sin el '+' de columna 1.
WS="$1"
cd "$WS" || exit 127
NAME=$(basename "$WS")
ADD=$(mktemp)
# Lineas anadidas, sin el '+' de columna 1 y SIN comentarios de linea: un separador
# `// --- Fase 1 ---` no es un operador de decremento.
git diff -U0 | grep "^+" | grep -v "^+++" | sed 's/^+//' | sed 's|//.*$||' > "$ADD"

FAIL=0
echo "### $NAME"
echo "lineas anadidas: $(wc -l < "$ADD")"

echo "-- convenciones Enforce --"
for pat in '++' '--' '+=' '-=' '*=' '/=' 'foreach' 'Print('; do
  n=$(grep -cF -- "$pat" "$ADD")
  if [ "$n" != "0" ]; then FAIL=1; printf "  FAIL %-9s %s\n" "$pat" "$n"; grep -nF -- "$pat" "$ADD" | head -3; fi
done
t=$(grep -cE '\?' "$ADD")
if [ "$t" != "0" ]; then printf "  REVISAR ternario? %s lineas con '?'\n" "$t"; grep -nE '\?' "$ADD" | head -3; fi
# `ref` solo vale en MIEMBROS de clase. Un miembro se reconoce por el prefijo m_ / s_.
# Se marcan: ref en lista de parametros, y declaraciones `ref` de algo que no es miembro.
REFBAD='(\(\s*ref |, *ref )'
r=$(grep -cE "$REFBAD" "$ADD")
if [ "$r" != "0" ]; then FAIL=1; printf "  FAIL ref en parametro: %s\n" "$r"; grep -nE "$REFBAD" "$ADD" | head -3; fi
rl=$(grep -E '^\s*ref ' "$ADD" | grep -vE ' (m_|s_)' | wc -l)
if [ "$rl" != "0" ]; then FAIL=1; printf "  FAIL ref en local/no-miembro: %s\n" "$rl"; grep -nE '^\s*ref ' "$ADD" | grep -vE ' (m_|s_)' | head -3; fi

echo "-- alcance: ficheros modificados --"
git status --porcelain | grep -E "^ ?M" | sed 's/^/  /'

echo "-- CRLF --"
for f in $(git diff --name-only); do
  if file "$f" | grep -q CRLF; then echo "  ok   $f"; else echo "  FAIL LF $f"; FAIL=1; fi
done

echo "-- entregable --"
if [ -f INFORME.md ]; then
  echo "  INFORME.md $(wc -c < INFORME.md) B"
  for s in "LO QUE NO PUDE VERIFICAR" "PREMISA"; do
    if grep -qi "$s" INFORME.md; then echo "  ok   seccion '$s'"; else echo "  FAIL falta seccion '$s'"; FAIL=1; fi
  done
else
  echo "  FAIL sin INFORME.md"; FAIL=1
fi

rm -f "$ADD"
if [ "$FAIL" = "0" ]; then echo "GATE: VERDE"; else echo "GATE: ROJO"; fi
exit $FAIL
