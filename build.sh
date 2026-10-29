#!/bin/bash
# set -e aborta el script apenas falla bison, flex o gcc. Sin esto, el codigo de
# salida del script era el del ultimo echo, siempre 0, y una build rota se veia
# como exitosa: rompia el "./build.sh && ./test_suite.sh" del README, porque el
# && nunca cortaba.
set -e

# Crear directorio de compilación si no existe
mkdir -p build

# Generar archivos de bison y flex en el directorio build
bison -d src/bison.y -o build/bison.tab.c
flex -o build/lex.yy.c src/lexer.l

gcc -Iinclude -Isrc -Ibuild build/bison.tab.c build/lex.yy.c src/ast.c src/parser_helpers.c src/symbol_table.c src/type_checker.c src/semantic_analyzer.c src/error_handler.c src/string_builder.c src/tac_operand.c src/tac_instruction.c src/tac_list.c src/intermediate_code.c src/asm_instruction.c src/object_code.c src/cli.c -o mi_compilador
echo "Ejemplo: ./mi_compilador examples/programa.ctds"
