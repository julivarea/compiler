#include "intermediate_code.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static TACList *currentCode = NULL;
static bool tacMemoryError = false;
static bool tacUnsupportedType = false;

static int temporaryCount = 0;
static int labelCount = 0;

static NodeAST *findUnsupportedType(NodeAST *node);
static void generateStatementCode(NodeAST *node);
static void generateMethodDeclarationCode(NodeAST *node);
static void generateAssignmentCode(NodeAST *node);
static void generateCallStatementCode(NodeAST *node);
static void generateReturnCode(NodeAST *node);
static void generateIfElseCode(NodeAST *node);
static void generateWhileCode(NodeAST *node);
static void generateVariableDeclarationCode(NodeAST *node);
static void generateBlockCode(NodeAST *node);
static bool lastInstructionIsReturn(void);
static TACOperand generateExpressionCode(NodeAST *node);
static TACOperand generateConstantOperand(NodeAST *node);
static TACOperand generateOperatorCode(NodeAST *node);
static TACOperand generateCallExpressionCode(NodeAST *node);
static TACOperand generateShortCircuitCode(NodeAST *node);
static TACOperation getTACOperation(OperationType operation);
static void generateCallArguments(NodeAST *node);
static TACOperand newTemporary(DataType type);
static TACOperand newLabel(void);
static void emit(TACOperation operation, TACOperand result, TACOperand firstOperand, TACOperand secondOperand);

const TACList *generateIntermediateCode(NodeAST *root) {
    if (root == NULL) {
        return NULL;
    }

    freeTACList(currentCode);
    currentCode = NULL;
    tacMemoryError = false;
    tacUnsupportedType = false;

    // El enunciado acota esta etapa a tipos enteros y logicos: un 'float' se
    // rechaza con su linea en vez de generarle codigo que no sabriamos traducir.
    NodeAST *unsupported = findUnsupportedType(root);
    if (unsupported != NULL) {
        fprintf(stderr,
                "\033[1;31mError en la linea %d:\033[0m la generacion de codigo para el tipo"
                " 'float' no esta soportada: esta etapa cubre tipos enteros y logicos.\n",
                unsupported->line);
        tacUnsupportedType = true;
        return NULL;
    }

    currentCode = createTACList();
    if (currentCode == NULL) {
        fprintf(stderr,
                "\033[1;31mError interno del TAC:\033[0m no se pudo reservar memoria para la lista de instrucciones.\n");
        return NULL;
    }
    temporaryCount = 0;
    labelCount = 0;

    for (int i = 0; i < root->childCount; i++) {
        generateStatementCode(root->children[i]);
    }

    if (tacMemoryError) {
        freeTACList(currentCode);
        currentCode = NULL;
        return NULL;
    }

    return currentCode;
}

bool hasUnsupportedTypeError(void) {
    return tacUnsupportedType;
}

static NodeAST *findUnsupportedType(NodeAST *node) {
    if (node == NULL) {
        return NULL;
    }

    bool isFloatNode = (node->type == TYPE_FLOAT);
    bool hasFloatSymbol = (node->symbol != NULL && node->symbol->type == TYPE_FLOAT);
    if (isFloatNode || hasFloatSymbol) {
        return node;
    }

    for (int i = 0; i < node->childCount; i++) {
        NodeAST *found = findUnsupportedType(node->children[i]);
        if (found != NULL) {
            return found;
        }
    }
    return NULL;
}

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

static void generateMethodDeclarationCode(NodeAST *node) {
    temporaryCount = 0;
    NodeAST *parameters = GET_LEFT(node);
    int parameterCount = parameters->childCount;
    TACOperand method = tacMethod(node->symbol->id, node->type);

    emit(TAC_BEGIN_FUNC, method, tacConstantInt(parameterCount), tacNone());
    for (int i = 0; i < parameterCount; i++) {
        emit(TAC_PARAM_DECL, tacVariable(parameters->children[i]->symbol), tacNone(), tacNone());
    }

    generateStatementCode(GET_RIGHT(node));

    bool needsImplicitReturn = (node->type == TYPE_VOID && !lastInstructionIsReturn());
    if (needsImplicitReturn) {
        emit(TAC_RETURN, tacNone(), tacNone(), tacNone());
    }

    emit(TAC_END_FUNC, method, tacNone(), tacNone());
}

static void generateAssignmentCode(NodeAST *node) {
    TACOperand value = generateExpressionCode(GET_EXPRESSION(node));
    emit(TAC_ASSIGN, tacVariable(node->symbol), value, tacNone());
}

static void generateCallStatementCode(NodeAST *node) {
    generateCallArguments(node);
    emit(TAC_CALL, tacNone(), tacMethod(node->symbol->id, node->type), tacConstantInt(node->childCount));
}

static void generateReturnCode(NodeAST *node) {
    NodeAST *expression = GET_EXPRESSION(node);
    TACOperand value = (expression != NULL) ? generateExpressionCode(expression) : tacNone();
    emit(TAC_RETURN, tacNone(), value, tacNone());
}

static void generateIfElseCode(NodeAST *node) {
    TACOperand condition = generateExpressionCode(GET_CONDITION(node));
    NodeAST *elseBlock = GET_ELSE_BLOCK(node);
    bool hasElseBlock = (elseBlock != NULL);

    if (!hasElseBlock) {
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

static void generateVariableDeclarationCode(NodeAST *node) {
    TACOperation declOp = node->symbol->isGlobal ? TAC_GLOBAL_DECL : TAC_LOCAL_DECL;
    emit(declOp, tacVariable(node->symbol), tacNone(), tacNone());
}

static void generateBlockCode(NodeAST *node) {
    for (int i = 0; i < node->childCount; i++) {
        generateStatementCode(node->children[i]);
    }
}

static bool lastInstructionIsReturn(void) {
    bool hasInstructions = (currentCode != NULL && currentCode->tail != NULL);
    return hasInstructions && (currentCode->tail->operation == TAC_RETURN);
}

static TACOperand generateExpressionCode(NodeAST *node) {
    if (node == NULL) {
        return tacNone();
    }

    switch (node->nodeType) {
        case ID_NODE:
            return tacVariable(node->symbol);

        case CONSTANT_NODE:
            return generateConstantOperand(node);

        case OPERATOR_NODE:
            return generateOperatorCode(node);

        case METHOD_CALL_NODE:
            return generateCallExpressionCode(node);

        default:
            return tacNone();
    }
}

static TACOperand generateConstantOperand(NodeAST *node) {
    switch (node->symbol->type) {
        case TYPE_INT:
            return tacConstantInt(node->symbol->value.intValue);
        case TYPE_BOOL:
            return tacConstantBool(node->symbol->value.intValue);
        default:
            return tacNone();
    }
}

static TACOperand generateOperatorCode(NodeAST *node) {
    bool isLogicalShortCircuit = (node->operationType == OP_AND || node->operationType == OP_OR);
    if (isLogicalShortCircuit) {
        return generateShortCircuitCode(node);
    }

    TACOperand left = generateExpressionCode(GET_LEFT(node));
    TACOperand result = newTemporary(node->type);
    NodeAST *rightNode = GET_RIGHT(node);
    TACOperand right = (rightNode != NULL) ? generateExpressionCode(rightNode) : tacNone();

    emit(getTACOperation(node->operationType), result, left, right);
    return result;
}

static TACOperand generateCallExpressionCode(NodeAST *node) {
    generateCallArguments(node);

    TACOperand result = newTemporary(node->type);
    emit(TAC_CALL, result, tacMethod(node->symbol->id, node->type), tacConstantInt(node->childCount));
    return result;
}

static TACOperand generateShortCircuitCode(NodeAST *node) {
    TACOperand left = generateExpressionCode(GET_LEFT(node));
    TACOperand result = newTemporary(node->type);

    emit(TAC_ASSIGN, result, left, tacNone());

    TACOperation jumpOp = (node->operationType == OP_AND) ? TAC_IF_FALSE : TAC_IF_TRUE;
    TACOperand endLabel = newLabel();
    emit(jumpOp, tacNone(), result, endLabel);

    TACOperand right = generateExpressionCode(GET_RIGHT(node));
    emit(TAC_ASSIGN, result, right, tacNone());

    emit(TAC_LABEL, endLabel, tacNone(), tacNone());
    return result;
}

// OP_AND y OP_OR no se mapean aqui porque se traducen a saltos de corto circuito antes de esta funcion.
static TACOperation getTACOperation(OperationType operation) {
    switch (operation) {
        case OP_ADD:      return TAC_ADD;
        case OP_SUB:      return TAC_SUB;
        case OP_MUL:      return TAC_MUL;
        case OP_DIV:      return TAC_DIV;
        case OP_MOD:      return TAC_MOD;
        case OP_LT:       return TAC_LT;
        case OP_GT:       return TAC_GT;
        case OP_EQ:       return TAC_EQ;
        case OP_UMINUS:   return TAC_UMINUS;
        case OP_NOT:      return TAC_NOT;
        default:
            fprintf(stderr,
                    "\033[1;31mError interno del TAC:\033[0m tipo de operacion no soportada (%d).\n",
                    operation);
            return TAC_ASSIGN;
    }
}

static void generateCallArguments(NodeAST *node) {
    for (int i = 0; i < node->childCount; i++) {
        TACOperand argument = generateExpressionCode(node->children[i]);
        emit(TAC_PARAM, tacNone(), argument, tacNone());
    }
}

static TACOperand newTemporary(DataType type) {
    char name[TAC_NAME_LENGTH];
    snprintf(name, sizeof(name), "t%d", temporaryCount++);
    return tacTemp(name, type);
}

static TACOperand newLabel(void) {
    char name[TAC_NAME_LENGTH];
    snprintf(name, sizeof(name), "L%d", labelCount++);
    return tacLabel(name);
}

static void emit(TACOperation operation, TACOperand result, TACOperand firstOperand, TACOperand secondOperand) {
    if (appendInstruction(currentCode, operation, result, firstOperand, secondOperand) == NULL) {
        fprintf(stderr,
                "\033[1;31mError interno del TAC:\033[0m no se pudo reservar memoria para una instruccion.\n");
        tacMemoryError = true;
    }
}
