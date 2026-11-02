#include "optimizer.h"

#include <stdio.h>
#include <string.h>

#include "tac_instruction.h"
#include "tac_operand.h"

#define MAX_KNOWN_CONSTANTS 256
#define MAX_OPTIMIZATION_ROUNDS 16

static int foldConstants(TACList *tacList);
static int propagateConstants(TACList *tacList);
static int removeUnreachableCode(TACList *tacList);
static int removeUnreferencedLabels(TACList *tacList);
static int simplifyConstantBranches(TACList *tacList);
static int removeRedundantJumps(TACList *tacList);
static int removeSelfAssignments(TACList *tacList);
static TACInstruction *unlinkInstruction(TACList *tacList, TACInstruction *previous, TACInstruction *current);


int optimizeIntermediateCode(TACList *tacList, OptimizationSet enabled) {
    if (tacList == NULL || enabled == OPTIMIZATION_NONE) return 0;

    int totalChanges = 0;

    /* Las dos pasadas se habilitan mutuamente, así que se repiten hasta que
     * ninguna cambie nada. El tope de rondas evita un ciclo infinito si alguna
     * transformación futura no convergiera. */
    for (int round = 0; round < MAX_OPTIMIZATION_ROUNDS; round++) {
        int changes = 0;

        if (isOptimizationEnabled(enabled, OPTIMIZATION_CONST_PROP)) {
            changes += propagateConstants(tacList);
        }
        if (isOptimizationEnabled(enabled, OPTIMIZATION_CONST_FOLD)) {
            changes += foldConstants(tacList);
        }
        if (isOptimizationEnabled(enabled, OPTIMIZATION_CONST_BRANCH)) {
            changes += simplifyConstantBranches(tacList);
        }
        if (isOptimizationEnabled(enabled, OPTIMIZATION_DEAD_CODE)) {
            changes += removeUnreachableCode(tacList);
        }
        if (isOptimizationEnabled(enabled, OPTIMIZATION_SELF_ASSIGN)) {
            changes += removeSelfAssignments(tacList);
        }
        if (isOptimizationEnabled(enabled, OPTIMIZATION_PEEPHOLE)) {
            changes += removeRedundantJumps(tacList);
        }

        bool hasConverged = (changes == 0);
        if (hasConverged) break;

        totalChanges += changes;
    }

    return totalChanges;
}

/* ------------------------------------------------------------------ */
/* Plegado de constantes                                               */
/* ------------------------------------------------------------------ */

static bool isConstant(const TACOperand *operand) {
    return operand->kind == OPERAND_CONSTANT;
}

static float asFloat(const TACOperand *operand) {
    return operand->value.floatValue;
}

static int asInt(const TACOperand *operand) {
    return operand->value.intValue;
}

/*
 * Calcula el resultado de una operación entre dos constantes.
 *
 * Devuelve false cuando el plegado no es seguro y hay que dejar la instrucción
 * como está: es el caso de la división y el módulo por cero, donde evaluar en
 * tiempo de compilación cambiaría el comportamiento en ejecución (el programa
 * original aborta, y uno con la operación plegada no).
 */
static bool evaluateBinary(TACOperation operation, const TACOperand *first,
                           const TACOperand *second, DataType resultType, TACOperand *folded) {
    bool isFloatOperation = (first->type == TYPE_FLOAT);

    switch (operation) {
        case TAC_ADD:
        case TAC_SUB:
        case TAC_MUL:
            if (resultType == TYPE_FLOAT) {
                float left = asFloat(first);
                float right = asFloat(second);
                float value = (operation == TAC_ADD) ? left + right
                            : (operation == TAC_SUB) ? left - right
                                                     : left * right;
                *folded = tacConstantFloat(value);
            } else {
                int left = asInt(first);
                int right = asInt(second);
                int value = (operation == TAC_ADD) ? left + right
                          : (operation == TAC_SUB) ? left - right
                                                   : left * right;
                *folded = tacConstantInt(value);
            }
            return true;

        case TAC_DIV:
            if (resultType == TYPE_FLOAT) {
                float right = asFloat(second);
                if (right == 0.0f) return false;
                *folded = tacConstantFloat(asFloat(first) / right);
            } else {
                int right = asInt(second);
                if (right == 0) return false;
                *folded = tacConstantInt(asInt(first) / right);
            }
            return true;

        case TAC_MOD: {
            /* El análisis semántico ya garantiza que ambos operandos son enteros. */
            int right = asInt(second);
            if (right == 0) return false;
            *folded = tacConstantInt(asInt(first) % right);
            return true;
        }

        case TAC_LT:
            *folded = tacConstantBool(isFloatOperation ? (asFloat(first) < asFloat(second))
                                                       : (asInt(first) < asInt(second)));
            return true;

        case TAC_GT:
            *folded = tacConstantBool(isFloatOperation ? (asFloat(first) > asFloat(second))
                                                       : (asInt(first) > asInt(second)));
            return true;

        case TAC_EQ:
            *folded = tacConstantBool(isFloatOperation ? (asFloat(first) == asFloat(second))
                                                       : (asInt(first) == asInt(second)));
            return true;

        default:
            return false;
    }
}

static bool evaluateUnary(TACOperation operation, const TACOperand *operand,
                          DataType resultType, TACOperand *folded) {
    switch (operation) {
        case TAC_UMINUS:
            if (resultType == TYPE_FLOAT) {
                *folded = tacConstantFloat(-asFloat(operand));
            } else {
                *folded = tacConstantInt(-asInt(operand));
            }
            return true;

        case TAC_NOT:
            *folded = tacConstantBool(!asInt(operand));
            return true;

        case TAC_INT_TO_FLOAT:
            *folded = tacConstantFloat((float)asInt(operand));
            return true;

        default:
            return false;
    }
}

/*
 * Reemplaza cada operación entre constantes por una asignación del resultado.
 *
 * La instrucción no se elimina de la lista: se convierte en `TAC_ASSIGN`, de
 * modo que el destino conserva su valor y cualquier uso posterior sigue siendo
 * válido. Quitar la asignación es trabajo de la propagación y, en última
 * instancia, de la eliminación de código muerto.
 */
static int foldConstants(TACList *tacList) {
    int changes = 0;

    for (TACInstruction *instruction = tacList->head; instruction != NULL; instruction = instruction->next) {
        bool hasConstantFirst = isConstant(&instruction->firstOperand);
        bool hasConstantSecond = isConstant(&instruction->secondOperand);
        TACOperand folded;
        bool wasFolded = false;

        if (hasConstantFirst && hasConstantSecond) {
            wasFolded = evaluateBinary(instruction->operation, &instruction->firstOperand,
                                       &instruction->secondOperand, instruction->result.type, &folded);
        } else if (hasConstantFirst && instruction->secondOperand.kind == OPERAND_NONE) {
            wasFolded = evaluateUnary(instruction->operation, &instruction->firstOperand,
                                      instruction->result.type, &folded);
        }

        if (wasFolded) {
            instruction->operation = TAC_ASSIGN;
            instruction->firstOperand = folded;
            instruction->secondOperand = tacNone();
            changes++;
        }
    }

    return changes;
}

/* ------------------------------------------------------------------ */
/* Propagación de constantes                                           */
/* ------------------------------------------------------------------ */

/**
 * @brief Valores constantes conocidos en el punto actual del recorrido.
 *
 * Se indexan por el nombre del operando, que ya viene calificado con el `uid`
 * del símbolo, así que dos variables homónimas de scopes distintos no se
 * confunden.
 */
typedef struct {
    char name[TAC_NAME_LENGTH];
    OperandKind destinationKind; /**< Si el valor lo guarda un temporal o una variable. */
    TACOperand value;            /**< La constante en sí; su kind es siempre OPERAND_CONSTANT. */
} KnownConstant;

typedef struct {
    KnownConstant entries[MAX_KNOWN_CONSTANTS];
    int count;
} KnownConstantTable;

static const TACOperand *lookupKnownConstant(const KnownConstantTable *table, const TACOperand *operand) {
    bool canHoldConstant = (operand->kind == OPERAND_TEMP || operand->kind == OPERAND_VARIABLE);
    if (!canHoldConstant) return NULL;

    for (int i = 0; i < table->count; i++) {
        if (strcmp(table->entries[i].name, operand->name) == 0) {
            return &table->entries[i].value;
        }
    }
    return NULL;
}

static void forgetConstant(KnownConstantTable *table, const TACOperand *operand) {
    for (int i = 0; i < table->count; i++) {
        if (strcmp(table->entries[i].name, operand->name) == 0) {
            table->entries[i] = table->entries[table->count - 1];
            table->count--;
            return;
        }
    }
}

static void rememberConstant(KnownConstantTable *table, const TACOperand *destination, const TACOperand *value) {
    forgetConstant(table, destination);

    bool isTableFull = (table->count >= MAX_KNOWN_CONSTANTS);
    if (isTableFull) return;

    KnownConstant *entry = &table->entries[table->count];
    snprintf(entry->name, sizeof(entry->name), "%s", destination->name);
    entry->destinationKind = destination->kind;
    entry->value = *value;
    table->count++;
}

/*
 * Olvida el valor de todas las variables, conservando el de los temporales.
 *
 * Una llamada puede escribir cualquier variable global, así que después de un
 * `call` no se puede seguir asumiendo lo que valía antes. Los temporales sí se
 * conservan: los genera el compilador, no son visibles desde otro método, y
 * C-TDS pasa los parámetros por valor y no tiene punteros, así que el callee no
 * tiene forma de alcanzar el frame de quien lo invoca.
 *
 * Es conservador con las variables locales, que por el mismo argumento tampoco
 * puede tocar el callee. Distinguirlas de las globales exigiría recorrer los
 * `TAC_GLOBAL_DECL` del programa, y la ganancia no justifica la pasada extra.
 */
static void forgetVariables(KnownConstantTable *table) {
    int kept = 0;
    for (int i = 0; i < table->count; i++) {
        bool isTemporary = (table->entries[i].destinationKind == OPERAND_TEMP);
        if (isTemporary) {
            table->entries[kept++] = table->entries[i];
        }
    }
    table->count = kept;
}

static bool replaceWithKnownConstant(const KnownConstantTable *table, TACOperand *operand) {
    const TACOperand *known = lookupKnownConstant(table, operand);
    if (known == NULL) return false;

    *operand = *known;
    return true;
}

/*
 * Reemplaza los usos de temporales y variables cuyo valor constante se conoce.
 *
 * La tabla de valores conocidos se descarta por completo en cada etiqueta y en
 * cada salto: a una etiqueta se puede llegar desde cualquier parte del
 * programa, así que lo que valía antes del salto no vale necesariamente
 * después. Es conservador —se pierden oportunidades dentro de un ciclo— pero
 * nunca incorrecto, que es la prioridad de la etapa.
 */
static int propagateConstants(TACList *tacList) {
    KnownConstantTable table = { .count = 0 };
    int changes = 0;

    for (TACInstruction *instruction = tacList->head; instruction != NULL; instruction = instruction->next) {
        switch (instruction->operation) {
            case TAC_LABEL:
            case TAC_GOTO:
            case TAC_IF_FALSE:
            case TAC_IF_TRUE:
            case TAC_BEGIN_FUNC:
            case TAC_END_FUNC:
                /* En los saltos condicionales conviene propagar la condición
                 * antes de olvidar todo: es lo que habilita descartar la rama. */
                if (instruction->operation == TAC_IF_FALSE || instruction->operation == TAC_IF_TRUE) {
                    if (replaceWithKnownConstant(&table, &instruction->firstOperand)) changes++;
                }
                table.count = 0;
                continue;

            case TAC_CALL:
                forgetVariables(&table);
                forgetConstant(&table, &instruction->result);
                continue;

            case TAC_GLOBAL_DECL:
            case TAC_LOCAL_DECL:
            case TAC_PARAM_DECL:
                continue;

            default:
                break;
        }

        /* Los operandos leídos se reemplazan por su valor conocido. */
        if (replaceWithKnownConstant(&table, &instruction->firstOperand)) changes++;
        if (replaceWithKnownConstant(&table, &instruction->secondOperand)) changes++;

        /* Y el destino escrito pasa a ser conocido, o deja de serlo. */
        bool writesResult = (instruction->result.kind == OPERAND_TEMP ||
                             instruction->result.kind == OPERAND_VARIABLE);
        if (!writesResult) continue;

        bool assignsConstant = (instruction->operation == TAC_ASSIGN &&
                                isConstant(&instruction->firstOperand));
        if (assignsConstant) {
            rememberConstant(&table, &instruction->result, &instruction->firstOperand);
        } else {
            forgetConstant(&table, &instruction->result);
        }
    }

    return changes;
}

/* ------------------------------------------------------------------ */
/* Eliminación de código inalcanzable                                  */
/* ------------------------------------------------------------------ */

/*
 * Saca la instrucción de la lista y la libera.
 *
 * @param previous Instrucción anterior, o NULL si @p current es la cabeza.
 * @return La instrucción que quedó en el lugar de la eliminada.
 */
static TACInstruction *unlinkInstruction(TACList *tacList, TACInstruction *previous, TACInstruction *current) {
    TACInstruction *next = current->next;

    if (previous == NULL) {
        tacList->head = next;
    } else {
        previous->next = next;
    }
    if (tacList->tail == current) {
        tacList->tail = previous;
    }

    freeTACInstruction(current);
    tacList->count--;

    return next;
}

/*
 * Una instrucción marca el fin del flujo lineal cuando después de ella la
 * ejecución no continúa con la siguiente: un `ret` sale del método y un `goto`
 * salta a otro lado. A partir de ahí, lo único que puede volver a traer el
 * control es una etiqueta.
 *
 * El enunciado habla de lo que sigue a un `return`; `goto` se incluye porque el
 * argumento es idéntico y es donde aparece la mayor parte del código muerto
 * real, en las ramas de los `if`.
 */
static bool endsControlFlow(TACOperation operation) {
    return operation == TAC_RETURN || operation == TAC_GOTO;
}

/*
 * Una instrucción es estructural cuando no se ejecuta: no describe una acción
 * sino una propiedad del programa que las etapas siguientes necesitan.
 *
 * Las etiquetas son destino de saltos, así que reanudan el flujo. Los
 * marcadores de método delimitan las funciones. Y las declaraciones le dicen al
 * generador de código objeto qué espacio reservar: borrar un `local` porque
 * quedó después de un `return` le sacaría su lugar en el frame a una variable,
 * sin ganar nada en tiempo de ejecución.
 */
static bool isStructural(TACOperation operation) {
    switch (operation) {
        case TAC_LABEL:
        case TAC_BEGIN_FUNC:
        case TAC_END_FUNC:
        case TAC_GLOBAL_DECL:
        case TAC_LOCAL_DECL:
        case TAC_PARAM_DECL:
            return true;
        default:
            return false;
    }
}

/*
 * Elimina las instrucciones que no pueden ejecutarse nunca.
 *
 * Recorre la lista marcando si el flujo está vivo. Tras un `ret` o un `goto`
 * pasa a estar muerto, y toda instrucción ejecutable que aparezca se descarta,
 * hasta que una etiqueta lo reanime.
 */
/*
 * Elimina las etiquetas a las que ya no salta nadie.
 *
 * Hace falta para que la eliminación de código inalcanzable sirva de algo
 * después de descartar una rama: al resolver un `if` con condición constante
 * desaparece el salto, pero la etiqueta a la que apuntaba queda, y como una
 * etiqueta reanuda el flujo, todo el bloque que la seguía se seguiría
 * considerando alcanzable. Sacándola primero, la rama muerta queda expuesta.
 */
static int removeUnreferencedLabels(TACList *tacList) {
    /* Qué etiquetas son destino de algún salto. */
    char referenced[MAX_KNOWN_CONSTANTS][TAC_NAME_LENGTH];
    int referencedCount = 0;

    for (const TACInstruction *instruction = tacList->head; instruction != NULL; instruction = instruction->next) {
        const TACOperand *target = NULL;

        if (instruction->operation == TAC_GOTO) {
            target = &instruction->firstOperand;
        } else if (instruction->operation == TAC_IF_FALSE || instruction->operation == TAC_IF_TRUE) {
            target = &instruction->secondOperand;
        }

        bool hasRoom = (referencedCount < MAX_KNOWN_CONSTANTS);
        if (target != NULL && hasRoom) {
            snprintf(referenced[referencedCount], TAC_NAME_LENGTH, "%s", target->name);
            referencedCount++;
        }
    }

    int changes = 0;
    TACInstruction *previous = NULL;
    TACInstruction *current = tacList->head;

    while (current != NULL) {
        bool isLabel = (current->operation == TAC_LABEL);
        bool isReferenced = false;

        for (int i = 0; isLabel && i < referencedCount; i++) {
            if (strcmp(referenced[i], current->result.name) == 0) {
                isReferenced = true;
            }
        }

        if (isLabel && !isReferenced) {
            current = unlinkInstruction(tacList, previous, current);
            changes++;
        } else {
            previous = current;
            current = current->next;
        }
    }

    return changes;
}

static int removeUnreachableCode(TACList *tacList) {
    int changes = removeUnreferencedLabels(tacList);
    bool isReachable = true;

    TACInstruction *previous = NULL;
    TACInstruction *current = tacList->head;

    while (current != NULL) {
        TACInstruction *next = current->next;

        bool mustKeep = (isReachable || isStructural(current->operation));

        if (mustKeep) {
            /* Una etiqueta vuelve a traer el control; un ret o un goto lo cortan. */
            if (current->operation == TAC_LABEL || current->operation == TAC_BEGIN_FUNC) {
                isReachable = true;
            } else if (endsControlFlow(current->operation)) {
                isReachable = false;
            }

            previous = current;
        } else {
            unlinkInstruction(tacList, previous, current);
            changes++;
        }

        current = next;
    }

    return changes;
}

/* ------------------------------------------------------------------ */
/* Condición constante en if y while                                    */
/* ------------------------------------------------------------------ */

/*
 * Resuelve en tiempo de compilación los saltos cuya condición ya es constante.
 *
 * Un `if <cond> false goto L` salta cuando la condición es falsa, y un
 * `if <cond> true goto L` cuando es verdadera. Si la condición es un literal, se
 * sabe de antemano cuál de las dos cosas pasa:
 *
 *   - El salto **nunca** se toma: la instrucción no hace nada y se elimina. El
 *     bloque que seguía pasa a ejecutarse siempre, sin preguntar la condición.
 *   - El salto **siempre** se toma: se reemplaza por un `goto` incondicional.
 *     Lo que venía después queda inalcanzable, y lo elimina §3.2 en la misma
 *     ronda, porque esta pasada corre antes.
 *
 * Las etiquetas se dejan en su lugar aunque queden sin referencias: son
 * estructurales y no generan código de máquina. Quitar las que nadie referencia
 * sería una limpieza aparte.
 */
static int simplifyConstantBranches(TACList *tacList) {
    int changes = 0;

    TACInstruction *previous = NULL;
    TACInstruction *current = tacList->head;

    while (current != NULL) {
        bool isConditionalJump = (current->operation == TAC_IF_FALSE ||
                                  current->operation == TAC_IF_TRUE);
        bool hasConstantCondition = isConditionalJump && isConstant(&current->firstOperand);

        if (!hasConstantCondition) {
            previous = current;
            current = current->next;
            continue;
        }

        bool conditionHolds = (asInt(&current->firstOperand) != 0);
        bool jumpIsTaken = (current->operation == TAC_IF_TRUE) ? conditionHolds : !conditionHolds;

        if (jumpIsTaken) {
            /* El destino del salto condicional viaja en el segundo operando, y
             * el de un goto en el primero. */
            current->operation = TAC_GOTO;
            current->firstOperand = current->secondOperand;
            current->secondOperand = tacNone();

            previous = current;
            current = current->next;
        } else {
            current = unlinkInstruction(tacList, previous, current);
        }

        changes++;
    }

    return changes;
}

/* ------------------------------------------------------------------ */
/* Saltos redundantes                                          */
/* ------------------------------------------------------------------ */

/*
 * Elimina los saltos cuyo destino es la etiqueta inmediatamente siguiente.
 *
 * Saltar a la instrucción que de todos modos venía a continuación no hace
 * nada. Aparecen sobre todo después de resolver un `if` con condición
 * constante (§3.3), que deja un `goto L` pegado a su propio `L:`.
 *
 * Es la única de las patrones de mirilla que conviene resolver sobre el TAC: no
 * depende de registros ni de la codificación de las instrucciones, solo de la
 * contigüidad de dos instrucciones.
 */
static int removeRedundantJumps(TACList *tacList) {
    int changes = 0;

    TACInstruction *previous = NULL;
    TACInstruction *current = tacList->head;

    while (current != NULL) {
        bool isJump = (current->operation == TAC_GOTO);
        bool jumpsToNextInstruction = false;

        if (isJump && current->next != NULL) {
            const TACInstruction *next = current->next;
            jumpsToNextInstruction = (next->operation == TAC_LABEL &&
                                      strcmp(next->result.name, current->firstOperand.name) == 0);
        }

        if (jumpsToNextInstruction) {
            current = unlinkInstruction(tacList, previous, current);
            changes++;
        } else {
            previous = current;
            current = current->next;
        }
    }

    return changes;
}

/* ------------------------------------------------------------------ */
/* Asignaciones de una variable a sí misma (sobre el TAC)              */
/* ------------------------------------------------------------------ */

/*
 * Elimina las asignaciones redundantes donde el origen y destino son idénticos.
 *
 * `x = x` no tiene efecto: en el TAC se traduce a `x@N = x@N`. Dado que el
 * nombre en el TAC ya incluye el `@uid` único de cada declaración, esta
 * comparación en el TAC es naturalmente segura ante ocultamiento de variables.
 */
static int removeSelfAssignments(TACList *tacList) {
    int changes = 0;
    TACInstruction *previous = NULL;
    TACInstruction *current = tacList->head;

    while (current != NULL) {
        bool isAssignment = (current->operation == TAC_ASSIGN);
        bool isVariableOrTemp = (current->result.kind == OPERAND_VARIABLE || current->result.kind == OPERAND_TEMP);
        bool isSelf = isAssignment && isVariableOrTemp &&
                      (current->firstOperand.kind == current->result.kind) &&
                      (strcmp(current->result.name, current->firstOperand.name) == 0);

        if (isSelf) {
            current = unlinkInstruction(tacList, previous, current);
            changes++;
        } else {
            previous = current;
            current = current->next;
        }
    }

    return changes;
}
