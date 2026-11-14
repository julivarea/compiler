#!/bin/bash
# set -e aborta el script apenas falla bison, flex o gcc. Sin esto, el codigo de
# salida del script era el del ultimo echo, siempre 0, y una build rota se veia
# como exitosa: rompia el "./build.sh && ./test_suite.sh" del README, porque el
# && nunca cortaba.
set -e

# Crear directorio de compilación si no existe
mkdir -p build

# Generar archivos de bison y flex en el directorio build
bison -d src/parser/bison.y -o build/bison.tab.c
flex -o build/lex.yy.c src/parser/lexer.l

gcc -Iinclude -Ibuild build/bison.tab.c build/lex.yy.c src/main.c src/core/driver.c src/parser/ast.c src/parser/parser_helpers.c src/semantic/symbol_table.c src/semantic/type_checker.c src/semantic/semantic_analyzer.c src/core/error_handler.c src/core/string_builder.c src/intermediate_code/tac.c src/optimizer/live_variable_analysis.c src/intermediate_code/intermediate_code.c src/optimizer/optimizer.c src/object_code/function_frame.c src/object_code/object_code.c src/core/cli.c -o mi_compilador
echo "Ejemplo: ./mi_compilador examples/valido_completo.ctds"
