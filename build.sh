# Crear directorio de compilación si no existe
mkdir -p build

# Generar archivos de bison y flex en el directorio build
bison -d src/bison.y -o build/bison.tab.c
flex -o build/lex.yy.c src/lexer.l

gcc -Isrc -Ibuild build/bison.tab.c build/lex.yy.c src/ast.c src/symbol_table.c -o mi_compilador
echo "Ejemplo: ./mi_compilador examples/programa.txt"
