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

echo "Compilando dependencias y suites de tests..."
step "bison" bison -d src/parser/bison.y -o build/bison.tab.c
step "flex" flex -o build/lex.yy.c src/parser/lexer.l
step "compilacion de tests del lexer" \
    gcc -DUNITY_TESTING -Iinclude -Ibuild -I./tests/unity tests/unity/unity.c tests/test_lexer.c build/bison.tab.c build/lex.yy.c src/parser/ast.c src/parser/parser_helpers.c src/semantic/symbol_table.c src/core/error_handler.c -o build/run_lexer_tests
step "compilacion de tests del parser" \
    gcc -DUNITY_TESTING -Iinclude -Ibuild -I./tests/unity tests/unity/unity.c tests/test_parser.c src/parser/ast.c src/parser/parser_helpers.c src/semantic/symbol_table.c src/core/error_handler.c build/bison.tab.c build/lex.yy.c -o build/run_parser_tests
step "compilacion de tests del AST" \
    gcc -DUNITY_TESTING -Iinclude -Ibuild -I./tests/unity tests/unity/unity.c tests/test_ast.c src/parser/ast.c src/parser/parser_helpers.c src/semantic/symbol_table.c src/core/error_handler.c build/bison.tab.c build/lex.yy.c -o build/run_ast_tests
step "compilacion de tests de la tabla de simbolos" \
    gcc -DUNITY_TESTING -Iinclude -I./tests/unity tests/unity/unity.c tests/test_symbol_table.c src/semantic/symbol_table.c -o build/run_symbol_table_tests
# Cada suite unitaria enlaza solo los fuentes de su etapa mas los modulos de datos que
# esa etapa recibe (AST, simbolos, TAC). El Arrange arma la salida de la etapa anterior
# con tests/support, asi que un defecto de una etapa previa no hace fallar la suite.
TEST_FLAGS="-DUNITY_TESTING -Iinclude -I./tests/unity -I./tests/support"
AST_DATA="src/parser/ast.c src/parser/parser_helpers.c src/semantic/symbol_table.c tests/support/lexer_stub.c tests/support/ast_builder.c"
TAC_DATA="src/intermediate_code/tac.c src/parser/ast.c src/semantic/symbol_table.c src/core/string_builder.c tests/support/lexer_stub.c tests/support/tac_builder.c"
step "compilacion de tests del analisis semantico" \
    gcc $TEST_FLAGS tests/unity/unity.c tests/test_semantic.c src/semantic/semantic_analyzer.c src/semantic/type_checker.c src/core/error_handler.c $AST_DATA -o build/run_semantic_tests
step "compilacion de tests del TAC" \
    gcc $TEST_FLAGS tests/unity/unity.c tests/test_tac.c src/intermediate_code/intermediate_code.c src/intermediate_code/tac.c src/core/error_handler.c src/core/string_builder.c $AST_DATA -o build/run_tac_tests
step "compilacion de tests del analisis de variables vivas" \
    gcc $TEST_FLAGS tests/unity/unity.c tests/test_live_variable_analysis.c src/optimizer/live_variable_analysis.c $TAC_DATA -o build/run_live_variable_tests
step "compilacion de tests del optimizador" \
    gcc $TEST_FLAGS tests/unity/unity.c tests/test_optimizer.c src/optimizer/optimizer.c src/optimizer/live_variable_analysis.c $TAC_DATA -o build/run_optimizer_tests
step "compilacion de tests de codigo objeto" \
    gcc $TEST_FLAGS tests/unity/unity.c tests/test_object_code.c src/object_code/object_code.c src/object_code/function_frame.c $TAC_DATA -o build/run_object_code_tests
# La suite de integracion es la unica que atraviesa todas las etapas: fuente -> ejecutable.
step "compilacion de tests de integracion" \
    gcc -DUNITY_TESTING -Iinclude -Ibuild -I./tests/unity tests/unity/unity.c tests/test_integration.c build/bison.tab.c build/lex.yy.c src/parser/ast.c src/parser/parser_helpers.c src/semantic/symbol_table.c src/semantic/type_checker.c src/semantic/semantic_analyzer.c src/core/error_handler.c src/core/string_builder.c src/intermediate_code/intermediate_code.c src/intermediate_code/tac.c src/optimizer/optimizer.c src/optimizer/live_variable_analysis.c src/object_code/object_code.c src/object_code/function_frame.c -o build/run_integration_tests
step "compilacion del compilador" \
    gcc -Iinclude -Ibuild build/bison.tab.c build/lex.yy.c src/main.c src/core/driver.c src/parser/ast.c src/parser/parser_helpers.c src/semantic/symbol_table.c src/semantic/type_checker.c src/semantic/semantic_analyzer.c src/core/error_handler.c src/core/string_builder.c src/intermediate_code/tac.c src/optimizer/live_variable_analysis.c src/intermediate_code/intermediate_code.c src/optimizer/optimizer.c src/object_code/function_frame.c src/object_code/object_code.c src/core/cli.c -o mi_compilador
step "compilacion de tests de la linea de comandos" \
    gcc -DUNITY_TESTING -Iinclude -Ibuild -I./tests/unity tests/unity/unity.c tests/test_cli.c src/core/cli.c -o build/run_cli_tests

# Convierte la salida de Unity a una linea por test y descarta los mensajes
# de error que imprime el propio compilador ("Error <etapa> en la linea N: ...")
# en los casos invalidos.
format() {
    grep -v 'Error [a-z]* en la linea' | sed -E \
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
run "AST" ./build/run_ast_tests
AST_EXIT=$?
run "SYMBOL TABLE" ./build/run_symbol_table_tests
SYMBOL_TABLE_EXIT=$?
run "SEMANTIC" ./build/run_semantic_tests
SEMANTIC_EXIT=$?
run "TAC" ./build/run_tac_tests
TAC_EXIT=$?
run "LIVE VARIABLES" ./build/run_live_variable_tests
LIVE_VAR_EXIT=$?
run "OPTIMIZER" ./build/run_optimizer_tests
OPTIMIZER_EXIT=$?
run "OBJECT CODE" ./build/run_object_code_tests
OBJECT_CODE_EXIT=$?
run "INTEGRATION" ./build/run_integration_tests
INTEGRATION_EXIT=$?
run "CLI" ./build/run_cli_tests
CLI_EXIT=$?

echo ""
if [ $LEXER_EXIT -eq 0 ] && [ $PARSER_EXIT -eq 0 ] && [ $AST_EXIT -eq 0 ] && [ $SYMBOL_TABLE_EXIT -eq 0 ] && [ $SEMANTIC_EXIT -eq 0 ] && [ $TAC_EXIT -eq 0 ] && [ $LIVE_VAR_EXIT -eq 0 ] && [ $OBJECT_CODE_EXIT -eq 0 ] && [ $OPTIMIZER_EXIT -eq 0 ] && [ $INTEGRATION_EXIT -eq 0 ] && [ $CLI_EXIT -eq 0 ]; then
    echo "✅ TODOS LOS TESTS PASARON"
else
    echo "❌ ALGUNOS TESTS FALLARON"
    exit 1
fi
