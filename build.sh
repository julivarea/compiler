# Crear directorio de compilación si no existe
mkdir -p build

# Generar archivos de bison y flex en el directorio build
bison -d bison.y -o build/bison.tab.c
flex -o build/lex.yy.c lexer.l

echo "Compilando parser e interprete..."
gcc -I. -Ibuild build/bison.tab.c build/lex.yy.c -o mi_compilador

echo "Build finalizado. El ejecutable se encuentra en ./mi_compilador"
echo "Ejemplo: ./mi_compilador prueba.c--"
