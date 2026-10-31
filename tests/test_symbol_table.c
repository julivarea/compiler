/**
 * @file test_symbol_table.c
 * @brief Pruebas unitarias de la tabla de símbolos y gestión de ámbitos léxicos (Scope).
 *
 * Valida el comportamiento de la tabla de símbolos conforme a las especificaciones
 * del lenguaje C-TDS:
 *  - Estructuración de ámbitos jerárquicos (global, métodos y bloques anidados).
 *  - Búsqueda local para detección de colisiones y redeclaraciones en el mismo ámbito.
 *  - Búsqueda en la cadena de ámbitos para resolución de identificadores.
 *  - Alcance léxico y ocultamiento (shadowing) de declaraciones más externas.
 *  - Identidad única e inequívoca de entidades mediante UID incremental para TAC y código objeto.
 *  - Asociación y propagación de la función contenedora para verificación de sentencias de retorno.
 *  - Crecimiento dinámico de almacenamiento sin límites arbitrarios de capacidad.
 */

#include "unity.h"
#include "symbol_table.h"
#include <stdlib.h>
#include <string.h>

static Scope *testRootScope = NULL;

// Helper local para liberar jerarquías de scopes y sus símbolos al finalizar cada test
static void freeTestScope(Scope *scope) {
    if (scope == NULL) return;

    for (int index = 0; index < scope->children_count; index++) {
        freeTestScope(scope->childrens[index]);
    }
    free(scope->childrens);

    for (int index = 0; index < scope->symbol_count; index++) {
        freeSymbol(scope->symbols[index]);
    }
    free(scope->symbols);
    free(scope);
}

void setUp(void) {
    testRootScope = NULL;
}

void tearDown(void) {
    if (testRootScope != NULL) {
        freeTestScope(testRootScope);
        testRootScope = NULL;
    }
}

/* ------------------------------------------------------------------------
 * Creación e inicialización de símbolos
 * ------------------------------------------------------------------------ */

// newSymbol inicializa correctamente los campos y deja uid en 0 hasta su registro
void test_new_symbol_initialization(void) {
    Symbol *symbol = newSymbol(strdup("variable"), ID_SYMBOL, TYPE_INT, NULL);
    TEST_ASSERT_NOT_NULL(symbol);
    TEST_ASSERT_EQUAL_STRING("variable", symbol->id);
    TEST_ASSERT_EQUAL_INT(ID_SYMBOL, symbol->symbolType);
    TEST_ASSERT_EQUAL_INT(TYPE_INT, symbol->type);
    TEST_ASSERT_EQUAL_INT(0, symbol->uid);
    TEST_ASSERT_NULL(symbol->parameters);
    freeSymbol(symbol);
}

// newSymbol permite identificador nulo para constantes literales anónimas
void test_new_symbol_anonymous_constant(void) {
    Symbol *symbol = newSymbol(NULL, CONSTANT_SYMBOL, TYPE_FLOAT, NULL);
    TEST_ASSERT_NOT_NULL(symbol);
    TEST_ASSERT_NULL(symbol->id);
    TEST_ASSERT_EQUAL_INT(CONSTANT_SYMBOL, symbol->symbolType);
    TEST_ASSERT_EQUAL_INT(TYPE_FLOAT, symbol->type);
    TEST_ASSERT_EQUAL_INT(0, symbol->uid);
    freeSymbol(symbol);
}

// freeSymbol maneja punteros nulos de forma segura sin fallar
void test_free_symbol_null_safe(void) {
    freeSymbol(NULL);
}

/* ------------------------------------------------------------------------
 * Creación de ámbitos léxicos (Scope) y jerarquía
 * ------------------------------------------------------------------------ */

// createScope inicializa un ámbito vacío con su nivel léxico correspondiente
void test_create_scope_initial_state(void) {
    testRootScope = createScope(0, NULL, NULL);
    TEST_ASSERT_NOT_NULL(testRootScope);
    TEST_ASSERT_EQUAL_INT(0, testRootScope->level);
    TEST_ASSERT_NULL(testRootScope->parent);
    TEST_ASSERT_NULL(testRootScope->function);
    TEST_ASSERT_EQUAL_INT(0, testRootScope->symbol_count);
    TEST_ASSERT_EQUAL_INT(0, testRootScope->children_count);
}

// createScope anidado mantiene la referencia al padre y asocia el método contenedor
void test_create_nested_scope(void) {
    testRootScope = createScope(0, NULL, NULL);
    Symbol *methodSymbol = newSymbol(strdup("calcular"), METHOD_SYMBOL, TYPE_INT, NULL);
    appendSymbol(testRootScope, methodSymbol);

    Scope *methodScope = createScope(1, testRootScope, methodSymbol);
    appendChildScope(testRootScope, methodScope);

    TEST_ASSERT_NOT_NULL(methodScope);
    TEST_ASSERT_EQUAL_INT(1, methodScope->level);
    TEST_ASSERT_EQUAL_PTR(testRootScope, methodScope->parent);
    TEST_ASSERT_EQUAL_PTR(methodSymbol, methodScope->function);
    TEST_ASSERT_EQUAL_INT(1, testRootScope->children_count);
}

// Los bloques internos dentro de un método conservan la referencia a la función contenedora
void test_nested_blocks_retain_enclosing_function_reference(void) {
    testRootScope = createScope(0, NULL, NULL);
    Symbol *enclosingFunction = newSymbol(strdup("factorial"), METHOD_SYMBOL, TYPE_INT, NULL);
    appendSymbol(testRootScope, enclosingFunction);

    Scope *methodScope = createScope(1, testRootScope, enclosingFunction);
    appendChildScope(testRootScope, methodScope);

    // Sub-bloque anidado dentro del método (ej. cuerpo de un if o while)
    Scope *innerBlockScope = createScope(2, methodScope, methodScope->function);
    appendChildScope(methodScope, innerBlockScope);

    TEST_ASSERT_EQUAL_PTR(enclosingFunction, innerBlockScope->function);
    TEST_ASSERT_EQUAL_PTR(methodScope, innerBlockScope->parent);
}

/* ------------------------------------------------------------------------
 * Registro de símbolos y asignación de UID
 * ------------------------------------------------------------------------ */

// appendSymbol asigna un uid incremental estrictamente positivo a símbolos no registrados
void test_append_symbol_assigns_incremental_positive_uid(void) {
    testRootScope = createScope(0, NULL, NULL);

    Symbol *firstSymbol = newSymbol(strdup("primero"), ID_SYMBOL, TYPE_INT, NULL);
    Symbol *secondSymbol = newSymbol(strdup("segundo"), ID_SYMBOL, TYPE_FLOAT, NULL);

    appendSymbol(testRootScope, firstSymbol);
    appendSymbol(testRootScope, secondSymbol);

    TEST_ASSERT_GREATER_THAN(0, firstSymbol->uid);
    TEST_ASSERT_GREATER_THAN(firstSymbol->uid, secondSymbol->uid);
    TEST_ASSERT_EQUAL_INT(2, testRootScope->symbol_count);
}

// appendSymbol conserva el uid existente si el símbolo ya fue registrado previamente
void test_append_symbol_preserves_existing_uid(void) {
    testRootScope = createScope(0, NULL, NULL);

    Symbol *existingSymbol = newSymbol(strdup("identificador"), ID_SYMBOL, TYPE_INT, NULL);
    existingSymbol->uid = 42;

    appendSymbol(testRootScope, existingSymbol);
    TEST_ASSERT_EQUAL_INT(42, existingSymbol->uid);
}

// appendSymbol ignora llamadas con scope o símbolo nulo sin alterar el estado
void test_append_symbol_null_safe(void) {
    testRootScope = createScope(0, NULL, NULL);
    Symbol *variableSymbol = newSymbol(strdup("variable"), ID_SYMBOL, TYPE_INT, NULL);

    appendSymbol(NULL, variableSymbol);
    TEST_ASSERT_EQUAL_INT(0, variableSymbol->uid);

    appendSymbol(testRootScope, NULL);
    TEST_ASSERT_EQUAL_INT(0, testRootScope->symbol_count);

    freeSymbol(variableSymbol);
}

/* ------------------------------------------------------------------------
 * Búsqueda en el ámbito actual (lookupVariableCurrentScope)
 * Unicidad de identificadores en el ámbito local
 * ------------------------------------------------------------------------ */

// lookupVariableCurrentScope encuentra símbolos registrados en el ámbito local
void test_lookup_current_scope_finds_symbol(void) {
    testRootScope = createScope(0, NULL, NULL);
    Symbol *localSymbol = newSymbol(strdup("contador"), ID_SYMBOL, TYPE_INT, NULL);
    appendSymbol(testRootScope, localSymbol);

    Symbol *foundSymbol = lookupVariableCurrentScope(testRootScope, "contador");
    TEST_ASSERT_NOT_NULL(foundSymbol);
    TEST_ASSERT_EQUAL_PTR(localSymbol, foundSymbol);
    TEST_ASSERT_EQUAL_STRING("contador", foundSymbol->id);
}

// lookupVariableCurrentScope retorna NULL para símbolos no presentes en el ámbito
void test_lookup_current_scope_returns_null_when_missing(void) {
    testRootScope = createScope(0, NULL, NULL);
    Symbol *localSymbol = newSymbol(strdup("contador"), ID_SYMBOL, TYPE_INT, NULL);
    appendSymbol(testRootScope, localSymbol);

    TEST_ASSERT_NULL(lookupVariableCurrentScope(testRootScope, "otroIdentificador"));
}

// lookupVariableCurrentScope no busca en el ámbito padre para aislar la validación de redeclaraciones
void test_lookup_current_scope_does_not_search_parent(void) {
    testRootScope = createScope(0, NULL, NULL);
    Symbol *globalVariable = newSymbol(strdup("global"), ID_SYMBOL, TYPE_INT, NULL);
    appendSymbol(testRootScope, globalVariable);

    Scope *innerScope = createScope(1, testRootScope, NULL);
    appendChildScope(testRootScope, innerScope);

    TEST_ASSERT_NOT_NULL(lookupVariableCurrentScope(testRootScope, "global"));
    TEST_ASSERT_NULL(lookupVariableCurrentScope(innerScope, "global"));
}

// lookupVariableCurrentScope maneja entradas nulas de forma segura
void test_lookup_current_scope_null_safe(void) {
    testRootScope = createScope(0, NULL, NULL);
    TEST_ASSERT_NULL(lookupVariableCurrentScope(NULL, "variable"));
    TEST_ASSERT_NULL(lookupVariableCurrentScope(testRootScope, NULL));
}

/* ------------------------------------------------------------------------
 * Búsqueda ascendente en la jerarquía y Shadowing (lookupVariable)
 * Visibilidad jerárquica, alcance léxico y shadowing
 * ------------------------------------------------------------------------ */

// lookupVariable resuelve símbolos definidos en el ámbito actual
void test_lookup_variable_in_current_scope(void) {
    testRootScope = createScope(0, NULL, NULL);
    Symbol *localSymbol = newSymbol(strdup("total"), ID_SYMBOL, TYPE_INT, NULL);
    appendSymbol(testRootScope, localSymbol);

    Symbol *foundSymbol = lookupVariable(testRootScope, "total");
    TEST_ASSERT_EQUAL_PTR(localSymbol, foundSymbol);
}

// lookupVariable asciende al ámbito padre inmediato si el símbolo no está en el local
void test_lookup_variable_in_parent_scope(void) {
    testRootScope = createScope(0, NULL, NULL);
    Symbol *globalSymbol = newSymbol(strdup("base"), ID_SYMBOL, TYPE_INT, NULL);
    appendSymbol(testRootScope, globalSymbol);

    Scope *innerScope = createScope(1, testRootScope, NULL);
    appendChildScope(testRootScope, innerScope);

    Symbol *foundSymbol = lookupVariable(innerScope, "base");
    TEST_ASSERT_NOT_NULL(foundSymbol);
    TEST_ASSERT_EQUAL_PTR(globalSymbol, foundSymbol);
}

// lookupVariable resuelve símbolos a través de múltiples niveles de anidamiento
void test_lookup_variable_in_deeply_nested_scope(void) {
    testRootScope = createScope(0, NULL, NULL);
    Symbol *globalSymbol = newSymbol(strdup("profundo"), ID_SYMBOL, TYPE_INT, NULL);
    appendSymbol(testRootScope, globalSymbol);

    Scope *levelOneScope = createScope(1, testRootScope, NULL);
    appendChildScope(testRootScope, levelOneScope);
    Scope *levelTwoScope = createScope(2, levelOneScope, NULL);
    appendChildScope(levelOneScope, levelTwoScope);
    Scope *levelThreeScope = createScope(3, levelTwoScope, NULL);
    appendChildScope(levelTwoScope, levelThreeScope);

    Symbol *foundSymbol = lookupVariable(levelThreeScope, "profundo");
    TEST_ASSERT_NOT_NULL(foundSymbol);
    TEST_ASSERT_EQUAL_PTR(globalSymbol, foundSymbol);
}

// Shadowing: un símbolo en ámbito interno oculta al del mismo nombre en ámbito padre
void test_lookup_variable_shadowing(void) {
    testRootScope = createScope(0, NULL, NULL);
    Symbol *outerVariable = newSymbol(strdup("x"), ID_SYMBOL, TYPE_INT, NULL);
    appendSymbol(testRootScope, outerVariable);

    Scope *innerScope = createScope(1, testRootScope, NULL);
    appendChildScope(testRootScope, innerScope);

    Symbol *innerVariable = newSymbol(strdup("x"), ID_SYMBOL, TYPE_FLOAT, NULL);
    appendSymbol(innerScope, innerVariable);

    // Desde el ámbito interno debe resolver la variable interna
    Symbol *resolvedInner = lookupVariable(innerScope, "x");
    TEST_ASSERT_NOT_NULL(resolvedInner);
    TEST_ASSERT_EQUAL_PTR(innerVariable, resolvedInner);
    TEST_ASSERT_EQUAL_INT(TYPE_FLOAT, resolvedInner->type);

    // Desde el ámbito global sigue resolviendo la variable externa
    Symbol *resolvedOuter = lookupVariable(testRootScope, "x");
    TEST_ASSERT_NOT_NULL(resolvedOuter);
    TEST_ASSERT_EQUAL_PTR(outerVariable, resolvedOuter);
    TEST_ASSERT_EQUAL_INT(TYPE_INT, resolvedOuter->type);
}

// Shadowing multinivel: la búsqueda devuelve siempre la declaración más cercana
void test_lookup_variable_multi_level_shadowing(void) {
    testRootScope = createScope(0, NULL, NULL);
    Symbol *globalVariableX = newSymbol(strdup("x"), ID_SYMBOL, TYPE_INT, NULL);
    appendSymbol(testRootScope, globalVariableX);

    Scope *levelOneScope = createScope(1, testRootScope, NULL);
    appendChildScope(testRootScope, levelOneScope);
    Symbol *levelOneVariableX = newSymbol(strdup("x"), ID_SYMBOL, TYPE_FLOAT, NULL);
    appendSymbol(levelOneScope, levelOneVariableX);

    Scope *levelTwoScope = createScope(2, levelOneScope, NULL);
    appendChildScope(levelOneScope, levelTwoScope);

    // En levelTwoScope (sin declaración propia de x) debe ganar levelOneScope sobre el global
    Symbol *foundSymbol = lookupVariable(levelTwoScope, "x");
    TEST_ASSERT_NOT_NULL(foundSymbol);
    TEST_ASSERT_EQUAL_PTR(levelOneVariableX, foundSymbol);
}

// lookupVariable retorna NULL si el símbolo no existe en ningún ámbito de la cadena
void test_lookup_variable_returns_null_when_undeclared(void) {
    testRootScope = createScope(0, NULL, NULL);
    Scope *innerScope = createScope(1, testRootScope, NULL);
    appendChildScope(testRootScope, innerScope);

    TEST_ASSERT_NULL(lookupVariable(innerScope, "noDeclarado"));
}

// lookupVariable maneja punteros nulos de forma segura
void test_lookup_variable_null_safe(void) {
    testRootScope = createScope(0, NULL, NULL);
    TEST_ASSERT_NULL(lookupVariable(NULL, "identificador"));
    TEST_ASSERT_NULL(lookupVariable(testRootScope, NULL));
}

// Las búsquedas coinciden por nombre exacto y no confunden prefijos ni sufijos
void test_lookup_variable_exact_match(void) {
    testRootScope = createScope(0, NULL, NULL);
    Symbol *firstSymbol = newSymbol(strdup("total"), ID_SYMBOL, TYPE_INT, NULL);
    Symbol *secondSymbol = newSymbol(strdup("total_acumulado"), ID_SYMBOL, TYPE_INT, NULL);
    Symbol *thirdSymbol = newSymbol(strdup("tot"), ID_SYMBOL, TYPE_INT, NULL);

    appendSymbol(testRootScope, firstSymbol);
    appendSymbol(testRootScope, secondSymbol);
    appendSymbol(testRootScope, thirdSymbol);

    TEST_ASSERT_EQUAL_PTR(firstSymbol, lookupVariable(testRootScope, "total"));
    TEST_ASSERT_EQUAL_PTR(secondSymbol, lookupVariable(testRootScope, "total_acumulado"));
    TEST_ASSERT_EQUAL_PTR(thirdSymbol, lookupVariable(testRootScope, "tot"));
    TEST_ASSERT_NULL(lookupVariable(testRootScope, "tota"));
}

/* ------------------------------------------------------------------------
 * Crecimiento dinámico y límites (Reallocations)
 * ------------------------------------------------------------------------ */

// appendSymbol redimensiona dinámicamente el arreglo de símbolos al superar su capacidad inicial
void test_append_symbol_grows_beyond_initial_capacity(void) {
    testRootScope = createScope(0, NULL, NULL);

    // Insertar 12 símbolos para forzar la expansión del arreglo interno
    const int TEST_COUNT = 12;
    for (int index = 0; index < TEST_COUNT; index++) {
        char name[16];
        snprintf(name, sizeof(name), "var%d", index);
        appendSymbol(testRootScope, newSymbol(strdup(name), ID_SYMBOL, TYPE_INT, NULL));
    }

    TEST_ASSERT_EQUAL_INT(TEST_COUNT, testRootScope->symbol_count);

    // Verificar que todos los símbolos se conservan íntegros y accesibles
    for (int index = 0; index < TEST_COUNT; index++) {
        char name[16];
        snprintf(name, sizeof(name), "var%d", index);
        TEST_ASSERT_NOT_NULL(lookupVariable(testRootScope, name));
    }
}

// appendSymbol soporta gran cantidad de símbolos (100) redimensionando múltiples veces
void test_append_symbol_many_symbols(void) {
    testRootScope = createScope(0, NULL, NULL);

    const int TOTAL_SYMBOLS = 100;
    for (int index = 0; index < TOTAL_SYMBOLS; index++) {
        char name[32];
        snprintf(name, sizeof(name), "variable_larga_%d", index);
        appendSymbol(testRootScope, newSymbol(strdup(name), ID_SYMBOL, TYPE_INT, NULL));
    }

    TEST_ASSERT_EQUAL_INT(TOTAL_SYMBOLS, testRootScope->symbol_count);

    // Verificar que el primero, el del medio y el último se recuperan correctamente
    TEST_ASSERT_NOT_NULL(lookupVariable(testRootScope, "variable_larga_0"));
    TEST_ASSERT_NOT_NULL(lookupVariable(testRootScope, "variable_larga_49"));
    TEST_ASSERT_NOT_NULL(lookupVariable(testRootScope, "variable_larga_99"));
}

// appendChildScope redimensiona dinámicamente el arreglo de hijos al superar su capacidad inicial
void test_append_child_scope_grows_beyond_initial_capacity(void) {
    testRootScope = createScope(0, NULL, NULL);

    // Insertar 6 hijos para forzar la expansión del arreglo interno
    const int TEST_COUNT = 6;
    for (int index = 0; index < TEST_COUNT; index++) {
        Scope *childScope = createScope(1, testRootScope, NULL);
        appendChildScope(testRootScope, childScope);
    }

    TEST_ASSERT_EQUAL_INT(TEST_COUNT, testRootScope->children_count);
}

// appendChildScope soporta gran cantidad de hijos (20 sub-ámbitos anidados como hermanos)
void test_append_child_scope_many_children(void) {
    testRootScope = createScope(0, NULL, NULL);

    const int TOTAL_CHILDREN = 20;
    for (int index = 0; index < TOTAL_CHILDREN; index++) {
        Scope *childScope = createScope(1, testRootScope, NULL);
        appendChildScope(testRootScope, childScope);
    }

    TEST_ASSERT_EQUAL_INT(TOTAL_CHILDREN, testRootScope->children_count);
}

// appendChildScope maneja entradas nulas de forma segura
void test_append_child_scope_null_safe(void) {
    testRootScope = createScope(0, NULL, NULL);
    appendChildScope(NULL, testRootScope);
    appendChildScope(testRootScope, NULL);
    TEST_ASSERT_EQUAL_INT(0, testRootScope->children_count);
}

int main(void) {
    UNITY_BEGIN();

    // Creación de símbolos
    RUN_TEST(test_new_symbol_initialization);
    RUN_TEST(test_new_symbol_anonymous_constant);
    RUN_TEST(test_free_symbol_null_safe);

    // Creación de ámbitos y jerarquía
    RUN_TEST(test_create_scope_initial_state);
    RUN_TEST(test_create_nested_scope);
    RUN_TEST(test_nested_blocks_retain_enclosing_function_reference);

    // Inserción y UIDs
    RUN_TEST(test_append_symbol_assigns_incremental_positive_uid);
    RUN_TEST(test_append_symbol_preserves_existing_uid);
    RUN_TEST(test_append_symbol_null_safe);

    // Búsqueda local
    RUN_TEST(test_lookup_current_scope_finds_symbol);
    RUN_TEST(test_lookup_current_scope_returns_null_when_missing);
    RUN_TEST(test_lookup_current_scope_does_not_search_parent);
    RUN_TEST(test_lookup_current_scope_null_safe);

    // Búsqueda jerárquica y Shadowing
    RUN_TEST(test_lookup_variable_in_current_scope);
    RUN_TEST(test_lookup_variable_in_parent_scope);
    RUN_TEST(test_lookup_variable_in_deeply_nested_scope);
    RUN_TEST(test_lookup_variable_shadowing);
    RUN_TEST(test_lookup_variable_multi_level_shadowing);
    RUN_TEST(test_lookup_variable_returns_null_when_undeclared);
    RUN_TEST(test_lookup_variable_null_safe);
    RUN_TEST(test_lookup_variable_exact_match);

    // Crecimiento y capacidades
    RUN_TEST(test_append_symbol_grows_beyond_initial_capacity);
    RUN_TEST(test_append_symbol_many_symbols);
    RUN_TEST(test_append_child_scope_grows_beyond_initial_capacity);
    RUN_TEST(test_append_child_scope_many_children);
    RUN_TEST(test_append_child_scope_null_safe);

    return UNITY_END();
}
