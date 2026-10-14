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

ExprResult generateExpressionCode(NodeAST *node) {
    ExprResult result;
    result.place[0] = '\0';

    if (node == NULL) {
        return result;
    }

    switch (node->nodeType) {
        case ID_NODE:
            // TODO: Copiar el identificador de la variable a result.place
            // if (node->symbol && node->symbol->id) {
            //     snprintf(result.place, sizeof(result.place), "%s", node->symbol->id);
            // }
            break;

        case CONSTANT_NODE:
            // TODO: Copiar el valor constante a result.place
            break;

        case BINARYOPERATOR_NODE:
            // TODO: 
            // 1. Generar código para GET_LEFT(node) -> ExprResult left
            // 2. Si no es unario, generar código para GET_RIGHT(node) -> ExprResult right
            // 3. Obtener un nuevo temporal con newTemp() y copiarlo a result.place
            // 4. Emitir instrucción con buildStringf("%s = %s %s %s\n", ...)
            break;

        case METHOD_CALL_NODE:
            for(int i = 0; i < node->paramCount; i++){
                buildStringf("param %s\n", node->parameters[i]->symbol->id);
            }
            buildStringf("call %s , %d\n", node->symbol->id, node->paramCount);
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
            // TODO: Emitir etiqueta de inicio de método (ej: buildStringf("%s:\n", node->symbol->id))
            // y procesar el cuerpo del método
            break;

        case ASSIGNMENT_NODE:
            // TODO: Generar la expresión derecha y luego emitir la asignación
            // ExprResult expr = generateExpressionCode(node->children[1]);
            // buildStringf("%s = %s\n", node->symbol->id, expr.place);
            break;

        case RETURN_NODE:
            
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
