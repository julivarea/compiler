#include "intermediate_code.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static TACList *currentCode = NULL;
static DataType currentMethodType = TYPE_VOID;

static int temporaryCount = 0;
static int labelCount = 0;

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
static TACOperand generateBinaryOperatorCode(NodeAST *node);
static TACOperand generateShortCircuitCode(NodeAST *node);
static TACOperand generateCallExpressionCode(NodeAST *node);
static TACOperand generateConstantOperand(NodeAST *node);
static void generateCallArguments(NodeAST *node);

static void coerceBinaryOperands(TACOperand *left, TACOperand *right);
static TACOperand coerceToFloat(TACOperand operand);
static TACOperation getTACOperation(OperationType operation);
static bool lastInstructionIsReturn(void);
static void emit(TACOperation operation, TACOperand result, TACOperand firstOperand, TACOperand secondOperand);
static TACOperand newTemporary(DataType type);
static TACOperand newLabel(void);

const TACList *generateIntermediateCode(NodeAST *root) {
    if (root == NULL) {
        return NULL;
    }

    freeTACList(currentCode);
    currentCode = createTACList();
    temporaryCount = 0;
    labelCount = 0;
    currentMethodType = TYPE_VOID;

    for (int i = 0; i < root->childCount; i++) {
        NodeAST *declaration = root->children[i];
        if (declaration == NULL) continue;

        bool isGlobalVariable = (declaration->nodeType == VARIABLE_DECLARATION_NODE);
        if (isGlobalVariable) {
            generateGlobalDeclarationCode(declaration);
        } else {
            generateStatementCode(declaration);
        }
    }

    return currentCode;
}

static void generateGlobalDeclarationCode(NodeAST *node) {
    bool hasSymbol = (node != NULL && node->symbol != NULL);
    if (hasSymbol) {
        emit(TAC_GLOBAL_DECL, tacNone(), tacVariable(node->symbol), tacNone());
    }
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
    bool isValidMethod = (node->symbol != NULL && node->symbol->id != NULL);
    if (!isValidMethod) {
        return;
    }

    NodeAST *parameters = GET_LEFT(node);
    int parameterCount = (parameters != NULL) ? parameters->childCount : 0;
    TACOperand method = tacMethod(node->symbol->id, node->type);

    DataType previousMethodType = currentMethodType;
    currentMethodType = node->type;

    emit(TAC_BEGIN_FUNC, method, tacConstantInt(parameterCount), tacNone());
    for (int i = 0; i < parameterCount; i++) {
        emit(TAC_PARAM_DECL, tacNone(), tacVariable(parameters->children[i]->symbol), tacNone());
    }

    generateStatementCode(GET_RIGHT(node));

    bool needsImplicitReturn = (node->type == TYPE_VOID && !lastInstructionIsReturn());
    if (needsImplicitReturn) {
        emit(TAC_RETURN, tacNone(), tacNone(), tacNone());
    }

    emit(TAC_END_FUNC, method, tacNone(), tacNone());
    currentMethodType = previousMethodType;
}

static void generateBlockCode(NodeAST *node) {
    for (int i = 0; i < node->childCount; i++) {
        generateStatementCode(node->children[i]);
    }
}

static void generateVariableDeclarationCode(NodeAST *node) {
    bool hasSymbol = (node->symbol != NULL);
    if (hasSymbol) {
        emit(TAC_LOCAL_DECL, tacNone(), tacVariable(node->symbol), tacNone());
    }
}

static void generateAssignmentCode(NodeAST *node) {
    bool isValidAssignment = (node->symbol != NULL && node->symbol->id != NULL);
    if (isValidAssignment) {
        TACOperand value = generateExpressionCode(GET_EXPRESSION(node));
        bool needsFloatCoercion = (node->symbol->type == TYPE_FLOAT);
        if (needsFloatCoercion) {
            value = coerceToFloat(value);
        }
        emit(TAC_ASSIGN, tacVariable(node->symbol), value, tacNone());
    }
}

static void generateCallStatementCode(NodeAST *node) {
    generateCallArguments(node);
    bool isValidMethod = (node->symbol != NULL && node->symbol->id != NULL);
    if (isValidMethod) {
        emit(TAC_CALL, tacNone(), tacMethod(node->symbol->id, node->type), tacConstantInt(node->childCount));
    }
}

static void generateReturnCode(NodeAST *node) {
    NodeAST *expression = GET_EXPRESSION(node);
    bool hasExpression = (expression != NULL);
    TACOperand value = hasExpression ? generateExpressionCode(expression) : tacNone();

    bool needsFloatCoercion = (hasExpression && currentMethodType == TYPE_FLOAT);
    if (needsFloatCoercion) {
        value = coerceToFloat(value);
    }

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

static TACOperand generateBinaryOperatorCode(NodeAST *node) {
    bool isLogicalShortCircuit = (node->operationType == OP_AND || node->operationType == OP_OR);
    if (isLogicalShortCircuit) {
        return generateShortCircuitCode(node);
    }

    TACOperand left = generateExpressionCode(GET_LEFT(node));
    TACOperand result = newTemporary(node->type);
    bool isUnary = (node->operationType == OP_NEGATIVE || node->operationType == OP_NEGATION);

    if (isUnary) {
        emit(getTACOperation(node->operationType), result, left, tacNone());
    } else {
        TACOperand right = generateExpressionCode(GET_RIGHT(node));
        coerceBinaryOperands(&left, &right);
        emit(getTACOperation(node->operationType), result, left, right);
    }

    return result;
}

static TACOperand generateShortCircuitCode(NodeAST *node) {
    TACOperand left = generateExpressionCode(GET_LEFT(node));
    TACOperand result = newTemporary(node->type);

    emit(TAC_ASSIGN, result, left, tacNone());

    bool isAndOperation = (node->operationType == OP_AND);
    TACOperation shortCircuitJump = isAndOperation ? TAC_IF_FALSE : TAC_IF_TRUE;
    TACOperand endLabel = newLabel();
    emit(shortCircuitJump, tacNone(), result, endLabel);

    TACOperand right = generateExpressionCode(GET_RIGHT(node));
    emit(TAC_ASSIGN, result, right, tacNone());

    emit(TAC_LABEL, endLabel, tacNone(), tacNone());
    return result;
}

static TACOperand generateCallExpressionCode(NodeAST *node) {
    generateCallArguments(node);
    bool isValidMethod = (node->symbol != NULL && node->symbol->id != NULL);
    if (!isValidMethod) {
        return tacNone();
    }

    TACOperand result = newTemporary(node->type);
    emit(TAC_CALL, result, tacMethod(node->symbol->id, node->type), tacConstantInt(node->childCount));
    return result;
}

static TACOperand generateConstantOperand(NodeAST *node) {
    bool hasSymbol = (node->symbol != NULL);
    if (!hasSymbol) {
        return tacNone();
    }

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

static void generateCallArguments(NodeAST *node) {
    NodeAST *formals = (node->symbol != NULL) ? node->symbol->parameters : NULL;

    for (int i = 0; i < node->childCount; i++) {
        TACOperand argument = generateExpressionCode(node->children[i]);
        bool hasFormal = (formals != NULL && i < formals->childCount);
        bool needsFloatCoercion = hasFormal && (formals->children[i]->type == TYPE_FLOAT);

        if (needsFloatCoercion) {
            argument = coerceToFloat(argument);
        }

        emit(TAC_PARAM, tacNone(), argument, tacNone());
    }
}

static void coerceBinaryOperands(TACOperand *left, TACOperand *right) {
    bool leftFloatRightInt = (left->type == TYPE_FLOAT && right->type == TYPE_INT);
    bool leftIntRightFloat = (left->type == TYPE_INT && right->type == TYPE_FLOAT);

    if (leftFloatRightInt) {
        *right = coerceToFloat(*right);
    } else if (leftIntRightFloat) {
        *left = coerceToFloat(*left);
    }
}

static TACOperand coerceToFloat(TACOperand operand) {
    bool isInt = (operand.type == TYPE_INT);
    if (!isInt) {
        return operand;
    }

    TACOperand result = newTemporary(TYPE_FLOAT);
    emit(TAC_INT_TO_FLOAT, result, operand, tacNone());
    return result;
}

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
        case OP_AND:      return TAC_AND;
        case OP_OR:       return TAC_OR;
        case OP_NEGATIVE: return TAC_NEG;
        case OP_NEGATION: return TAC_NOT;
        default:          return TAC_ASSIGN;
    }
}

static bool lastInstructionIsReturn(void) {
    bool hasInstructions = (currentCode != NULL && currentCode->tail != NULL);
    return hasInstructions && (currentCode->tail->operation == TAC_RETURN);
}

static void emit(TACOperation operation, TACOperand result, TACOperand firstOperand, TACOperand secondOperand) {
    appendInstruction(currentCode, operation, result, firstOperand, secondOperand);
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
