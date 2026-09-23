#!/bin/bash
# Uso: bash test_suite.sh            -> salida resumida (PASS/FAIL por test)
#      VERBOSE=1 bash test_suite.sh  -> salida cruda, con warnings y mensajes del compilador

mkdir -p build

# Ejecuta un paso de la build; muestra su salida solo si falla o en modo VERBOSE.
step() {
    local desc="$1"; shift
    local out
    out=$("$@" 2>&1)
    local rc=$?
    if [ $rc -ne 0 ]; then
        echo "❌ Error: $desc"
        echo "$out"
        exit 1
    fi
    [ -n "$VERBOSE" ] && [ -n "$out" ] && echo "$out"
}

echo "Generando Flex y Bison y compilando tests..."
step "bison" bison -d bison.y -o build/bison.tab.c
step "flex" flex -o build/lex.yy.c lexer.l
step "compilacion de tests del lexer" \
    gcc -DUNITY_TESTING -I. -Ibuild -I./tests/unity tests/unity/unity.c tests/test_lexer.c build/bison.tab.c build/lex.yy.c -o build/run_lexer_tests
step "compilacion de tests del parser" \
    gcc -DUNITY_TESTING -I. -Ibuild -I./tests/unity tests/unity/unity.c tests/test_parser.c ast.c build/bison.tab.c build/lex.yy.c -o build/run_parser_tests

# Convierte la salida de Unity a una linea por test y descarta los mensajes
# de error que imprime el propio compilador ("[Linea N] ...") en los casos invalidos.
format() {
    grep -v '^\[Linea' | sed -E \
        -e 's/^[^:]+:[0-9]+:([^:]+):PASS$/  [PASS] \1/' \
        -e 's/^[^:]+:([0-9]+):([^:]+):FAIL: ?(.*)$/  [FAIL] \2\n         -> linea \1: \3/' \
        -e 's/^[^:]+:[0-9]+:([^:]+):IGNORE.*$/  [SKIP] \1/' \
        -e '/^-+$/d' -e '/^(OK|FAIL)$/d' -e '/^$/d'
}

run() {
    echo ""
    echo "== $1"
    if [ -n "$VERBOSE" ]; then
        "$2"
    else
        "$2" 2>/dev/null | format
        return "${PIPESTATUS[0]}"
    fi
}

run "LEXER" ./build/run_lexer_tests
LEXER_EXIT=$?
run "PARSER" ./build/run_parser_tests
PARSER_EXIT=$?

echo ""
if [ $LEXER_EXIT -eq 0 ] && [ $PARSER_EXIT -eq 0 ]; then
    echo "✅ TODOS LOS TESTS PASARON"
else
    echo "❌ ALGUNOS TESTS FALLARON"
    exit 1
fi
