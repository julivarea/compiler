#include <stdio.h>
#include <string.h>
#include "intermediate_code.h"
#include "string_builder.h"

static int temp_count = 0;
static int label_count = 0;

void resetTempAndLabelCounters(void) {
    temp_count = 0;
    label_count = 0;
}

char* newTemp(void) {
    static char buffer[32];
    snprintf(buffer, sizeof(buffer), "t%d", temp_count++);
    return buffer;
}

char* newLabel(void) {
    static char buffer[32];
    snprintf(buffer, sizeof(buffer), "L%d", label_count++);
    return buffer;
}

const char* getOperatorSymbol(OperationType op) {
    switch (op) {
        case OP_ADD:      return "+";
        case OP_SUB:      return "-";
        case OP_MUL:      return "*";
        case OP_DIV:      return "/";
        case OP_MOD:      return "%";
        case OP_LT:       return "<";
        case OP_GT:       return ">";
        case OP_EQ:       return "==";
        case OP_AND:      return "&&";
        case OP_OR:       return "||";
        case OP_NEGATIVE: return "-";
        case OP_NEGATION: return "!";
        default:          return "?";
    }
}

Expression generateExpressionCode(NodeAST *node) {
    Expression result;
    result.place[0] = '\0';

    if (node == NULL) {
        return result;
    }

    switch (node->nodeType) {
        case ID_NODE:
            strcpy(result.place, node->symbol->id);
            break;

        case CONSTANT_NODE:
            if (node->symbol != NULL) {
                switch (node->symbol->type) {
                    case TYPE_INT: 
                        snprintf(result.place, sizeof(result.place), "%d", node->symbol->value.int_val);
                        break;
                    case TYPE_BOOL:
                        snprintf(result.place, sizeof(result.place), "%s", node->symbol->value.int_val ? "true" : "false");
                        break;
                    case TYPE_FLOAT:
                        snprintf(result.place, sizeof(result.place), "%f", node->symbol->value.float_val);
                        break;
                    default:
                        result.place[0] = '\0';
                        break;
                }
            }
            break;

        case BINARYOPERATOR_NODE: {
            Expression leftExpression = generateExpressionCode(GET_LEFT(node));
            char* temporalBinaryOperator = newTemp();
            strcpy(result.place, temporalBinaryOperator);

            if (node->operationType == OP_NEGATIVE || node->operationType == OP_NEGATION) {
                buildStringf("%s = %s%s\n", result.place, getOperatorSymbol(node->operationType), leftExpression.place);
            } else {
                Expression rightExpression = generateExpressionCode(GET_RIGHT(node));
                buildStringf("%s = %s %s %s\n", result.place, leftExpression.place, getOperatorSymbol(node->operationType), rightExpression.place);
            }
            break;
        }

        case METHOD_CALL_NODE:
            for (int i = 0; i < node->childCount; i++) {
                Expression arg = generateExpressionCode(node->children[i]);
                buildStringf("param %s\n", arg.place);
            }
            if (node->symbol && node->symbol->id) {
                buildStringf("%s = call %s, %d\n", result.place, node->symbol->id, node->childCount);
            }
            break;

        default:
            break;
    }

    return result;
}

void generateStatementCode(NodeAST *node) {
    if (node == NULL) {
        return;
    }

    switch (node->nodeType) {
        case METHOD_DECLARATION_NODE:
            if (node->symbol && node->symbol->id) {
                buildStringf("%s:\n", node->symbol->id);
            }
            if (node->childCount > 1 && node->children[1] != NULL) {
                generateStatementCode(node->children[1]);
            }
            break;

        case ASSIGNMENT_NODE:
             if (node->symbol && node->symbol->id) {
                buildStringf("%s = %s\n", node->symbol->id, generateExpressionCode(GET_RIGHT(node)).place);
            }
            break;

        case RETURN_NODE:
            if(GET_EXPRESSION(node) == NULL){
                buildStringf("ret");
            } else {
                buildStringf("ret %s\n", generateExpressionCode(GET_EXPRESSION(node)).place);
            }
            break;

        case IF_ELSE_NODE:
            // TODO: Manejo de etiquetas L_else y L_end con saltos condicionales
            break;

        case WHILE_NODE:
            // TODO: Manejo de etiquetas L_start y L_end con saltos condicionales y goto
            break;

        case BLOCK_NODE:
            for (int i = 0; i < node->childCount; i++) {
                generateStatementCode(node->children[i]);
            }
            break;

        default:
            for (int i = 0; i < node->childCount; i++) {
                generateStatementCode(node->children[i]);
            }
            break;
    }
}

void generateIntermediateCode(NodeAST *root) {
    if (root == NULL) {
        return;
    }

    resetTempAndLabelCounters();

    for (int i = 0; i < root->childCount; i++) {
        generateStatementCode(root->children[i]);
    }
}
