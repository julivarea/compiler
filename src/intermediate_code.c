#include <stdlib.h>
#include <string.h>
#include "intermediate_code.h"

static TACList *current_code = NULL;
static DataType current_method_type = TYPE_VOID;

static int temp_count = 0;
static int label_count = 0;

static void generateStatementCode(NodeAST *node);
static TACOperand generateExpressionCode(NodeAST *node);

static void generateMethodDeclarationCode(NodeAST *node);
static void generateAssignmentCode(NodeAST *node);
static void generateCallStatementCode(NodeAST *node);
static void generateReturnCode(NodeAST *node);
static void generateIfElseCode(NodeAST *node);
static void generateWhileCode(NodeAST *node);
static void generateVariableDeclarationCode(NodeAST *node);
static void generateBlockCode(NodeAST *node);
static void generateGlobalDeclarationCode(NodeAST *node);

static TACOperand generateBinaryOperatorCode(NodeAST *node);
static TACOperand generateCallExpressionCode(NodeAST *node);
static TACOperand generateConstantOperand(NodeAST *node);
static TACOperand generateShortCircuitCode(NodeAST *node);

static void generateCallArguments(NodeAST *node);
static void coerceBinaryOperands(TACOperand *left, TACOperand *right);
static TACOperand coerceToFloat(TACOperand operand);
static TACOperation getTACOpForOperation(OperationType operation);
static int lastInstructionIsReturn(void);
static void emit(TACOperation operation, TACOperand result, TACOperand firstOperand, TACOperand secondOperand);
static TACOperand newTemp(DataType type);
static TACOperand newLabel(void);

/**
 * @brief Genera el código intermedio de tres direcciones para el programa completo.
 *
 * Recorre el AST a partir del bloque raíz, descarta cualquier secuencia previa,
 * reinicia los contadores internos y despacha declaraciones globales y de métodos.
 *
 * @param root Nodo raíz del AST a procesar 
 * @return lista de instrucciones
 */
const TACList *generateIntermediateCode(NodeAST *root) {
    if (root == NULL) {
        return NULL;
    }

    freeTACList(current_code);
    current_code = createTACList();
    temp_count = 0;
    label_count = 0;
    current_method_type = TYPE_VOID;

    for (int i = 0; i < root->childCount; i++) {
        NodeAST *declaration = root->children[i];
        if (declaration == NULL) continue;
        
        // declaraciones de variables globales, las variables locales son procesadas junto con el bloque por statementCode
        if (declaration->nodeType == VARIABLE_DECLARATION_NODE) {
            generateGlobalDeclarationCode(declaration);
        } else {
            generateStatementCode(declaration);
        }
    }
    return current_code;
}

/**
 * @brief Devuelve la lista de instrucciones TAC generada en la última invocación.
 *
 * Permite acceder a la secuencia de cuádruplas para su posterior procesamiento
 * en las etapas de optimización y generación de código objeto.
 *
 * @return Puntero constante a la lista de instrucciones TAC, o NULL si todavía no se ejecutó.
 */
const TACList *getIntermediateCode(void) {
    return current_code;
}

/**
 * @brief Despacha la generación de código intermedio según la categoría sintáctica del nodo.
 *
 * Encapsula la delegación de cada tipo de nodo de sentencia hacia su función
 * generadora correspondiente.
 *
 * @param node Nodo del AST que representa una sentencia o declaración (acepta NULL).
 */
static void generateStatementCode(NodeAST *node) {
    if (node == NULL) {
        return;
    }

    switch (node->nodeType) {
        case METHOD_DECLARATION_NODE:
            generateMethodDeclarationCode(node);
            break;

        case ASSIGNMENT_NODE:
            generateAssignmentCode(node);
            break;

        case METHOD_CALL_NODE:
            generateCallStatementCode(node);
            break;

        case RETURN_NODE:
            generateReturnCode(node);
            break;

        case IF_ELSE_NODE:
            generateIfElseCode(node);
            break;

        case WHILE_NODE:
            generateWhileCode(node);
            break;

        case VARIABLE_DECLARATION_NODE:
            generateVariableDeclarationCode(node);
            break;

        case BLOCK_NODE:
        default:
            generateBlockCode(node);
            break;
    }
}

/**
 * @brief Genera código intermedio para una expresión del AST y retorna su operando resultante.
 *
 * Evalúa recursivamente la expresión según su categoría sintáctica, emitiendo
 * las instrucciones necesarias y encapsulando el resultado en un TACOperand.
 *
 * @param node Nodo del AST representativo de la expresión (acepta NULL).
 * @return Operando TAC que almacena el resultado o temporal de la evaluación.
 */
static TACOperand generateExpressionCode(NodeAST *node) {
    if (node == NULL) {
        return tacNone();
    }

    switch (node->nodeType) {
        case ID_NODE:
            return tacVariable(node->symbol);

        case CONSTANT_NODE:
            return generateConstantOperand(node);

        case BINARYOPERATOR_NODE:
            return generateBinaryOperatorCode(node);

        case METHOD_CALL_NODE:
            return generateCallExpressionCode(node);

        default:
            return tacNone();
    }
}

/**
 * @brief Genera el código intermedio para la declaración de un método.
 *
 * Emite la cabecera de la función, declara sus parámetros formales en orden,
 * procesa el cuerpo y añade un retorno implícito si el método es void y no retornó.
 *
 * @param node Nodo del AST correspondiente a METHOD_DECLARATION_NODE.
 */
static void generateMethodDeclarationCode(NodeAST *node) {
    if (node->symbol == NULL || node->symbol->id == NULL) {
        return;
    }

    NodeAST *parameters = GET_LEFT(node);
    int parameterCount = (parameters != NULL) ? parameters->childCount : 0;
    TACOperand method = tacMethod(node->symbol->id, node->type);

    // La cabecera lleva la cantidad de parametros y cada formal se
    // declara aparte, en orden: el generador de codigo objeto necesita
    // ese orden para asociarlos a los registros de la convencion de
    // llamada.
    DataType previousMethodType = current_method_type;
    current_method_type = node->type;

    emit(TAC_BEGIN_FUNC, method, tacConstantInt(parameterCount), tacNone());
    for (int i = 0; i < parameterCount; i++) {
        emit(TAC_PARAM_DECL, tacNone(), tacVariable(parameters->children[i]->symbol), tacNone());
    }

    generateStatementCode(GET_RIGHT(node));

    // Un metodo void puede terminar sin un return explicito, pero el
    // codigo objeto necesita siempre un epilogo: si el cuerpo no
    // termino retornando, se agrega el retorno implicito. Los metodos
    // no-void no lo necesitan, porque el analisis semantico ya garantiza
    // que todos sus caminos terminan en un return.
    if (node->type == TYPE_VOID && !lastInstructionIsReturn()) {
        emit(TAC_RETURN, tacNone(), tacNone(), tacNone());
    }

    emit(TAC_END_FUNC, method, tacNone(), tacNone());

    current_method_type = previousMethodType;
}

/**
 * @brief Genera el código intermedio para una sentencia de asignación.
 *
 * Evalúa la expresión a asignar, aplica coerción a flotante si la variable
 * de destino lo requiere y emite la cuádrupla de asignación.
 *
 * @param node Nodo del AST correspondiente a ASSIGNMENT_NODE.
 */
static void generateAssignmentCode(NodeAST *node) {
    if (node->symbol != NULL && node->symbol->id != NULL) {
        TACOperand value = generateExpressionCode(GET_EXPRESSION(node));

        if (node->symbol->type == TYPE_FLOAT) {
            value = coerceToFloat(value);
        }

        emit(TAC_ASSIGN, tacVariable(node->symbol), value, tacNone());
    }
}

/**
 * @brief Genera el código intermedio para una llamada a método como sentencia.
 *
 * Emite los argumentos como parámetros y la instrucción de llamada, descartando
 * el resultado devuelto.
 *
 * @param node Nodo del AST correspondiente a METHOD_CALL_NODE.
 */
static void generateCallStatementCode(NodeAST *node) {
    generateCallArguments(node);
    if (node->symbol != NULL && node->symbol->id != NULL) {
        emit(TAC_CALL, tacNone(), tacMethod(node->symbol->id, node->type), tacConstantInt(node->childCount));
    }
}

/**
 * @brief Genera el código intermedio para una sentencia de retorno.
 *
 * Evalúa la expresión devuelta si existe, realiza la coerción a flotante si
 * el método actual lo exige y emite la instrucción de retorno.
 *
 * @param node Nodo del AST correspondiente a RETURN_NODE.
 */
static void generateReturnCode(NodeAST *node) {
    NodeAST *expression = GET_EXPRESSION(node);
    TACOperand value = (expression == NULL) ? tacNone() : generateExpressionCode(expression);

    if (expression != NULL && current_method_type == TYPE_FLOAT) {
        value = coerceToFloat(value);
    }

    emit(TAC_RETURN, tacNone(), value, tacNone());
}

/**
 * @brief Genera el código intermedio para una estructura de control condicional if-else.
 *
 * Emite la evaluación de la condición, saltos condicionales y etiquetas
 * correspondientes a la rama if y a la rama opcional else.
 *
 * @param node Nodo del AST correspondiente a IF_ELSE_NODE.
 */
static void generateIfElseCode(NodeAST *node) {
    TACOperand condition = generateExpressionCode(GET_CONDITION(node));
    NodeAST *elseBlock = GET_ELSE_BLOCK(node);

    if (elseBlock == NULL) {
        // Sin rama else, la condicion falsa saltea el bloque entero.
        TACOperand endIf = newLabel();

        emit(TAC_IF_FALSE, tacNone(), condition, endIf);
        generateStatementCode(GET_IF_BLOCK(node));
        emit(TAC_LABEL, endIf, tacNone(), tacNone());
    } else {
        TACOperand elseIf = newLabel();
        TACOperand endIf = newLabel();

        emit(TAC_IF_FALSE, tacNone(), condition, elseIf);
        generateStatementCode(GET_IF_BLOCK(node));
        emit(TAC_GOTO, tacNone(), endIf, tacNone());
        emit(TAC_LABEL, elseIf, tacNone(), tacNone());
        generateStatementCode(elseBlock);
        emit(TAC_LABEL, endIf, tacNone(), tacNone());
    }
}

/**
 * @brief Genera el código intermedio para un bucle iterativo while.
 *
 * Emite etiquetas de inicio y fin, evalúa la condición de continuación
 * y genera el salto incondicional de repetición.
 *
 * @param node Nodo del AST correspondiente a WHILE_NODE.
 */
static void generateWhileCode(NodeAST *node) {
    TACOperand startOfWhile = newLabel();
    TACOperand endOfWhile = newLabel();

    emit(TAC_LABEL, startOfWhile, tacNone(), tacNone());

    TACOperand condition = generateExpressionCode(GET_CONDITION(node));
    emit(TAC_IF_FALSE, tacNone(), condition, endOfWhile);

    generateStatementCode(GET_WHILE_BLOCK(node));

    emit(TAC_GOTO, tacNone(), startOfWhile, tacNone());
    emit(TAC_LABEL, endOfWhile, tacNone(), tacNone());
}

/**
 * @brief Genera el código intermedio para la declaración de una variable local.
 *
 * Emite la instrucción TAC_LOCAL_DECL para dimensionar el marco de pila del método.
 *
 * @param node Nodo del AST correspondiente a VARIABLE_DECLARATION_NODE.
 */
static void generateVariableDeclarationCode(NodeAST *node) {
    // Toda declaracion que llega hasta aca esta dentro del cuerpo de un
    // metodo (las globales las maneja generateIntermediateCode), incluidas
    // las de los bloques anidados, que se emiten en el lugar donde
    // aparecen. El generador de codigo objeto recorre el rango entre
    // begin_func y end_func para dimensionar el frame.
    if (node->symbol != NULL) {
        emit(TAC_LOCAL_DECL, tacNone(), tacVariable(node->symbol), tacNone());
    }
}

/**
 * @brief Genera el código intermedio para un bloque de sentencias.
 *
 * Itera secuencialmente sobre todas las sentencias contenidas en el bloque.
 *
 * @param node Nodo del AST correspondiente a BLOCK_NODE.
 */
static void generateBlockCode(NodeAST *node) {
    for (int i = 0; i < node->childCount; i++) {
        generateStatementCode(node->children[i]);
    }
}

/**
 * @brief Genera el código intermedio para la declaración de una variable global.
 *
 * Emite la instrucción TAC_GLOBAL_DECL en el nivel superior del programa.
 *
 * @param node Nodo del AST correspondiente a VARIABLE_DECLARATION_NODE en ámbito global.
 */
static void generateGlobalDeclarationCode(NodeAST *node) {
    if (node->symbol != NULL) {
        emit(TAC_GLOBAL_DECL, tacNone(), tacVariable(node->symbol), tacNone());
    }
}

/**
 * @brief Genera código intermedio para expresiones con operadores binarios o unarios.
 *
 * Maneja cortocircuito para operadores lógicos and/or, operaciones unarias,
 * y operaciones binarias con coerción implícita de operandos.
 *
 * @param node Nodo del AST correspondiente a BINARYOPERATOR_NODE.
 * @return Operando TAC temporal que contiene el resultado evaluado.
 */
static TACOperand generateBinaryOperatorCode(NodeAST *node) {
    if (node->operationType == OP_AND || node->operationType == OP_OR) {
        return generateShortCircuitCode(node);
    }

    TACOperand left = generateExpressionCode(GET_LEFT(node));
    TACOperand result = newTemp(node->type);

    if (node->operationType == OP_NEGATIVE || node->operationType == OP_NEGATION) {
        emit(getTACOpForOperation(node->operationType), result, left, tacNone());
    } else {
        TACOperand right = generateExpressionCode(GET_RIGHT(node));
        coerceBinaryOperands(&left, &right);
        emit(getTACOpForOperation(node->operationType), result, left, right);
    }

    return result;
}

/**
 * @brief Genera código intermedio para una llamada a método dentro de una expresión.
 *
 * Evalúa y emite los argumentos, solicita un nuevo temporal y emite la instrucción TAC_CALL.
 *
 * @param node Nodo del AST correspondiente a METHOD_CALL_NODE.
 * @return Operando TAC temporal con el valor retornado por el método.
 */
static TACOperand generateCallExpressionCode(NodeAST *node) {
    generateCallArguments(node);
    if (node->symbol == NULL || node->symbol->id == NULL) {
        return tacNone();
    }

    // El temporal se pide despues de los argumentos, asi su numero
    // queda despues de los temporales que estos hayan consumido.
    TACOperand result = newTemp(node->type);
    emit(TAC_CALL, result, tacMethod(node->symbol->id, node->type), tacConstantInt(node->childCount));

    return result;
}

/**
 * @brief Construye un operando constante a partir del valor de un símbolo constante.
 *
 * @param node Nodo del AST correspondiente a CONSTANT_NODE.
 * @return Operando TAC constante (entero, booleano o flotante).
 */
static TACOperand generateConstantOperand(NodeAST *node) {
    if (node->symbol == NULL) return tacNone();

    switch (node->symbol->type) {
        case TYPE_INT:
            return tacConstantInt(node->symbol->value.int_val);
        case TYPE_BOOL:
            return tacConstantBool(node->symbol->value.int_val);
        case TYPE_FLOAT:
            return tacConstantFloat(node->symbol->value.float_val);
        default:
            return tacNone();
    }
}

/**
 * @brief Genera código con evaluación de cortocircuito para operadores lógicos and y or.
 *
 * && y || no se traducen a una instruccion binaria sino a control de flujo,
 * porque la letra exige evaluacion de corto circuito: el operando derecho no
 * se evalua si el izquierdo ya determina el resultado de toda la expresion.
 *
 *   result = a && b              result = a || b
 *   --------------------         --------------------
 *   result = a                   result = a
 *   if result false goto L       if result true goto L
 *   result = b                   result = b
 *   L:                           L:
 *
 * @param node Nodo del AST con operación OP_AND u OP_OR.
 * @return Operando TAC temporal con el resultado booleano.
 */
static TACOperand generateShortCircuitCode(NodeAST *node) {
    TACOperand left = generateExpressionCode(GET_LEFT(node));
    TACOperand result = newTemp(node->type);

    emit(TAC_ASSIGN, result, left, tacNone());

    TACOperation shortCircuitJump = (node->operationType == OP_AND) ? TAC_IF_FALSE : TAC_IF_TRUE;
    TACOperand endLabel = newLabel();
    emit(shortCircuitJump, tacNone(), result, endLabel);

    TACOperand right = generateExpressionCode(GET_RIGHT(node));
    emit(TAC_ASSIGN, result, right, tacNone());

    emit(TAC_LABEL, endLabel, tacNone(), tacNone());

    return result;
}

/**
 * @brief Emite los argumentos de una llamada como instrucciones TAC_PARAM.
 *
 * Evalúa los argumentos de izquierda a derecha y aplica coerción explícita
 * a flotante si el parámetro formal correspondiente lo requiere.
 *
 * @param node Nodo del AST que contiene los argumentos de la invocación.
 */
static void generateCallArguments(NodeAST *node) {
    NodeAST *formals = (node->symbol != NULL) ? node->symbol->parameters : NULL;

    for (int i = 0; i < node->childCount; i++) {
        TACOperand argument = generateExpressionCode(node->children[i]);

        // Pasar un entero a un parametro float es una coercion valida, y tiene
        // que quedar explicita antes de empujar el argumento.
        if (formals != NULL && i < formals->childCount
            && formals->children[i]->type == TYPE_FLOAT) {
            argument = coerceToFloat(argument);
        }

        emit(TAC_PARAM, tacNone(), argument, tacNone());
    }
}

/**
 * @brief Coerciona los operandos de una operación binaria si mezclan entero y flotante.
 *
 * @param left Puntero al operando izquierdo.
 * @param right Puntero al operando derecho.
 */
static void coerceBinaryOperands(TACOperand *left, TACOperand *right) {
    if (left->type == TYPE_FLOAT && right->type == TYPE_INT) {
        *right = coerceToFloat(*right);
    } else if (left->type == TYPE_INT && right->type == TYPE_FLOAT) {
        *left = coerceToFloat(*left);
    }
}

/**
 * @brief Devuelve un operando flotante equivalente emitiendo la conversión si era entero.
 *
 * @param operand Operando a verificar y eventualmente convertir.
 * @return Operando TAC flotante resultante.
 */
static TACOperand coerceToFloat(TACOperand operand) {
    if (operand.type != TYPE_INT) {
        return operand;
    }

    TACOperand result = newTemp(TYPE_FLOAT);
    emit(TAC_INT_TO_FLOAT, result, operand, tacNone());

    return result;
}

/**
 * @brief Mapea una operación del AST al código de operación TAC correspondiente.
 *
 * @param operation Tipo de operación sintáctica del nodo.
 * @return Operación TAC correspondiente.
 */
static TACOperation getTACOpForOperation(OperationType operation) {
    switch (operation) {
        case OP_ADD:      return TAC_ADD;
        case OP_SUB:      return TAC_SUB;
        case OP_MUL:      return TAC_MUL;
        case OP_DIV:      return TAC_DIV;
        case OP_MOD:      return TAC_MOD;
        case OP_LT:       return TAC_LT;
        case OP_GT:       return TAC_GT;
        case OP_EQ:       return TAC_EQ;
        case OP_AND:      return TAC_AND;
        case OP_OR:       return TAC_OR;
        case OP_NEGATIVE: return TAC_NEG;
        case OP_NEGATION: return TAC_NOT;
        default:          return TAC_ASSIGN;
    }
}

/**
 * @brief Verifica si la última instrucción emitida en la lista TAC es de retorno.
 *
 * @return 1 si la última instrucción es TAC_RETURN; 0 en caso contrario.
 */
static int lastInstructionIsReturn(void) {
    return current_code != NULL
        && current_code->tail != NULL
        && current_code->tail->operation == TAC_RETURN;
}

/**
 * @brief Agrega una instrucción cuádrupla a la lista de código intermedio actual.
 *
 * @param operation Código de la operación TAC.
 * @param result Operando de destino o resultado.
 * @param firstOperand Primer operando de la instrucción.
 * @param secondOperand Segundo operando de la instrucción.
 */
static void emit(TACOperation operation, TACOperand result, TACOperand firstOperand, TACOperand secondOperand) {
    appendInstruction(current_code, operation, result, firstOperand, secondOperand);
}

/**
 * @brief Crea un nuevo operando temporal correlativo.
 *
 * @param type Tipo de dato del temporal.
 * @return Operando TAC temporal.
 */
static TACOperand newTemp(DataType type) {
    char name[TAC_NAME_LENGTH];
    snprintf(name, sizeof(name), "t%d", temp_count++);
    return tacTemp(name, type);
}

/**
 * @brief Crea una nueva etiqueta correlativa.
 *
 * @return Operando TAC de etiqueta.
 */
static TACOperand newLabel(void) {
    char name[TAC_NAME_LENGTH];
    snprintf(name, sizeof(name), "L%d", label_count++);
    return tacLabel(name);
}
