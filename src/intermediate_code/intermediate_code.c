#include "intermediate_code.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static TACList *currentCode = NULL;
static DataType currentMethodType = TYPE_VOID;
static bool tacMemoryError = false;
static bool tacUnsupportedType = false;

static int temporaryCount = 0;
static int labelCount = 0;

static NodeAST *findUnsupportedType(NodeAST *node);
static void generateGlobalDeclarationCode(NodeAST *node);
static void generateStatementCode(NodeAST *node);
static void generateMethodDeclarationCode(NodeAST *node);
static void generateBlockCode(NodeAST *node);
static void generateVariableDeclarationCode(NodeAST *node);
static void generateAssignmentCode(NodeAST *node);
static void generateCallStatementCode(NodeAST *node);
static void generateReturnCode(NodeAST *node);
static void generateIfElseCode(NodeAST *node);
static void generateWhileCode(NodeAST *node);

static TACOperand generateExpressionCode(NodeAST *node);
static TACOperand generateOperatorCode(NodeAST *node);
static TACOperand generateShortCircuitCode(NodeAST *node);
static TACOperand generateCallExpressionCode(NodeAST *node);
static TACOperand generateConstantOperand(NodeAST *node);
static void generateCallArguments(NodeAST *node);

static TACOperation getTACOperation(OperationType operation);
static bool lastInstructionIsReturn(void);
static void emit(TACOperation operation, TACOperand result, TACOperand firstOperand, TACOperand secondOperand);
static void emitGlobalDecl(TACOperand variable);
static void emitLocalDecl(TACOperand variable);
static void emitBeginFunc(TACOperand method, int parameterCount);
static void emitEndFunc(TACOperand method);
static void emitParamDecl(TACOperand parameter);
static void emitParam(TACOperand argument);
static void emitCall(TACOperand result, TACOperand method, int argumentCount);
static void emitReturn(TACOperand value);
static void emitReturnVoid(void);
static void emitLabel(TACOperand label);
static void emitGoto(TACOperand label);
static void emitIfFalse(TACOperand condition, TACOperand label);
static void emitIfTrue(TACOperand condition, TACOperand label);
static void emitAssign(TACOperand destination, TACOperand source);
static void emitBinary(TACOperation operation, TACOperand result, TACOperand left, TACOperand right);
static void emitUnary(TACOperation operation, TACOperand result, TACOperand operand);
static TACOperand newTemporary(DataType type);
static TACOperand newLabel(void);

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
    currentMethodType = TYPE_VOID;

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

    DataType previousMethodType = currentMethodType;
    currentMethodType = node->type;

    emitBeginFunc(method, parameterCount);
    for (int i = 0; i < parameterCount; i++) {
        emitParamDecl(tacVariable(parameters->children[i]->symbol));
    }

    generateStatementCode(GET_RIGHT(node));

    bool needsImplicitReturn = (node->type == TYPE_VOID && !lastInstructionIsReturn());
    if (needsImplicitReturn) {
        emitReturnVoid();
    }

    emitEndFunc(method);
    currentMethodType = previousMethodType;
}

static void generateBlockCode(NodeAST *node) {
    for (int i = 0; i < node->childCount; i++) {
        generateStatementCode(node->children[i]);
    }
}

static void generateVariableDeclarationCode(NodeAST *node) {
    if (node->symbol->isGlobal) {
        emitGlobalDecl(tacVariable(node->symbol));
    } else {
        emitLocalDecl(tacVariable(node->symbol));
    }
}

static void generateAssignmentCode(NodeAST *node) {
    TACOperand value = generateExpressionCode(GET_EXPRESSION(node));
    emitAssign(tacVariable(node->symbol), value);
}

static void generateCallStatementCode(NodeAST *node) {
    generateCallArguments(node);
    emitCall(tacNone(), tacMethod(node->symbol->id, node->type), node->childCount);
}

static void generateReturnCode(NodeAST *node) {
    NodeAST *expression = GET_EXPRESSION(node);
    bool hasExpression = (expression != NULL);
    TACOperand value = hasExpression ? generateExpressionCode(expression) : tacNone();

    if (hasExpression) {
        emitReturn(value);
    } else {
        emitReturnVoid();
    }
}

static void generateIfElseCode(NodeAST *node) {
    TACOperand condition = generateExpressionCode(GET_CONDITION(node));
    NodeAST *elseBlock = GET_ELSE_BLOCK(node);
    bool hasElseBlock = (elseBlock != NULL);

    if (!hasElseBlock) {
        TACOperand endIf = newLabel();
        emitIfFalse(condition, endIf);
        generateStatementCode(GET_IF_BLOCK(node));
        emitLabel(endIf);
    } else {
        TACOperand elseIf = newLabel();
        TACOperand endIf = newLabel();
        emitIfFalse(condition, elseIf);
        generateStatementCode(GET_IF_BLOCK(node));
        emitGoto(endIf);
        emitLabel(elseIf);
        generateStatementCode(elseBlock);
        emitLabel(endIf);
    }
}

static void generateWhileCode(NodeAST *node) {
    TACOperand startOfWhile = newLabel();
    TACOperand endOfWhile = newLabel();

    emitLabel(startOfWhile);

    TACOperand condition = generateExpressionCode(GET_CONDITION(node));
    emitIfFalse(condition, endOfWhile);

    generateStatementCode(GET_WHILE_BLOCK(node));

    emitGoto(startOfWhile);
    emitLabel(endOfWhile);
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

static TACOperand generateOperatorCode(NodeAST *node) {
    bool isLogicalShortCircuit = (node->operationType == OP_AND || node->operationType == OP_OR);
    if (isLogicalShortCircuit) {
        return generateShortCircuitCode(node);
    }

    TACOperand left = generateExpressionCode(GET_LEFT(node));
    TACOperand result = newTemporary(node->type);
    bool isUnary = (node->operationType == OP_UMINUS || node->operationType == OP_NOT);
    if (isUnary) {
        emitUnary(getTACOperation(node->operationType), result, left);
    } else {
        TACOperand right = generateExpressionCode(GET_RIGHT(node));
        emitBinary(getTACOperation(node->operationType), result, left, right);
    }

    return result;
}

static TACOperand generateShortCircuitCode(NodeAST *node) {
    TACOperand left = generateExpressionCode(GET_LEFT(node));
    TACOperand result = newTemporary(node->type);

    emitAssign(result, left);

    bool isAndOperation = (node->operationType == OP_AND);
    TACOperand endLabel = newLabel();
    if (isAndOperation) {
        emitIfFalse(result, endLabel);
    } else {
        emitIfTrue(result, endLabel);
    }

    TACOperand right = generateExpressionCode(GET_RIGHT(node));
    emitAssign(result, right);

    emitLabel(endLabel);
    return result;
}

static TACOperand generateCallExpressionCode(NodeAST *node) {
    generateCallArguments(node);

    TACOperand result = newTemporary(node->type);
    emitCall(result, tacMethod(node->symbol->id, node->type), node->childCount);
    return result;
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

static void generateCallArguments(NodeAST *node) {
    for (int i = 0; i < node->childCount; i++) {
        TACOperand argument = generateExpressionCode(node->children[i]);
        emitParam(argument);
    }
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

static bool lastInstructionIsReturn(void) {
    bool hasInstructions = (currentCode != NULL && currentCode->tail != NULL);
    return hasInstructions && (currentCode->tail->operation == TAC_RETURN);
}

static void emit(TACOperation operation, TACOperand result, TACOperand firstOperand, TACOperand secondOperand) {
    TACInstruction *instruction = createTACInstruction(operation, result, firstOperand, secondOperand);
    if (instruction == NULL || appendTACInstruction(currentCode, instruction) == NULL) {
        fprintf(stderr,
                "\033[1;31mError interno del TAC:\033[0m no se pudo reservar memoria para una instruccion.\n");
        tacMemoryError = true;
    }
}

static void emitGlobalDecl(TACOperand variable) {
    emit(TAC_GLOBAL_DECL, variable, tacNone(), tacNone());
}

static void emitLocalDecl(TACOperand variable) {
    emit(TAC_LOCAL_DECL, variable, tacNone(), tacNone());
}

static void emitBeginFunc(TACOperand method, int parameterCount) {
    emit(TAC_BEGIN_FUNC, method, tacConstantInt(parameterCount), tacNone());
}

static void emitEndFunc(TACOperand method) {
    emit(TAC_END_FUNC, method, tacNone(), tacNone());
}

static void emitParamDecl(TACOperand parameter) {
    emit(TAC_PARAM_DECL, parameter, tacNone(), tacNone());
}

static void emitParam(TACOperand argument) {
    emit(TAC_PARAM, tacNone(), argument, tacNone());
}

static void emitCall(TACOperand result, TACOperand method, int argumentCount) {
    emit(TAC_CALL, result, method, tacConstantInt(argumentCount));
}

static void emitReturn(TACOperand value) {
    emit(TAC_RETURN, tacNone(), value, tacNone());
}

static void emitReturnVoid(void) {
    emit(TAC_RETURN, tacNone(), tacNone(), tacNone());
}

static void emitLabel(TACOperand label) {
    emit(TAC_LABEL, label, tacNone(), tacNone());
}

static void emitGoto(TACOperand label) {
    emit(TAC_GOTO, tacNone(), label, tacNone());
}

static void emitIfFalse(TACOperand condition, TACOperand label) {
    emit(TAC_IF_FALSE, tacNone(), condition, label);
}

static void emitIfTrue(TACOperand condition, TACOperand label) {
    emit(TAC_IF_TRUE, tacNone(), condition, label);
}

static void emitAssign(TACOperand destination, TACOperand source) {
    emit(TAC_ASSIGN, destination, source, tacNone());
}

static void emitBinary(TACOperation operation, TACOperand result, TACOperand left, TACOperand right) {
    emit(operation, result, left, right);
}

static void emitUnary(TACOperation operation, TACOperand result, TACOperand operand) {
    emit(operation, result, operand, tacNone());
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
