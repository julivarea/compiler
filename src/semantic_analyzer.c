#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "semantic_analyzer.h"
#include "type_checker.h"
#include "error_handler.h"
#include "string_builder.h"

static void analyzeNodeSemantics(NodeAST *node, Scope *current_scope);
static void analyzeMethodNode(NodeAST *node, Scope *current_scope);
static void analyzeVariableDeclarationNode(NodeAST *node, Scope *current_scope);
static void analyzeAssignmentNode(NodeAST *node, Scope *current_scope);
static void analyzeReturnNode(NodeAST *node, Scope *current_scope);
static void analyzeIfElseNode(NodeAST *node, Scope *current_scope);
static void analyzeWhileNode(NodeAST *node, Scope *current_scope);
static void checkMainMethodExists(Scope *globalScope);

/**
 * @brief Realiza el analisis semantico de un arbol AST
 *
 * Esta estructura permite determinar si las variables y demás símbolos
 * referenciados son accesibles desde el ámbito en el que se utilizan.
 *
 * @param root Raíz del árbol AST que se va a analizar.
 * @param parent Ámbito padre del que se está analizando (NULL para global).
 * @return Árbol de ámbitos generado a partir del árbol AST.
 */

Scope* analyzeSemantics(NodeAST *root, Scope *parent) {
    if (!root) return NULL;
    
    int isScopeGlobal = (parent == NULL);

    int level = !isScopeGlobal ? parent->level + 1 : 0;
    Symbol *function = !isScopeGlobal ? parent->function : NULL;
    Scope *current_scope = createScope(level, parent, function);
    if (!current_scope) {
        semanticError(root->line, "Error: no se pudo reservar memoria para el ambito.");
        return NULL;
    }

    if (!isScopeGlobal) {
        appendChildScope(parent, current_scope);
    }
    
    for (int i = 0; i < root->childCount; i++) {
        analyzeNodeSemantics(root->children[i], current_scope);
    }

    if (isScopeGlobal) {
        checkMainMethodExists(current_scope);
    }

    return current_scope;
}
/**
 * @brief Verifica que exista el main en el ambito global
 *
 * @param globalScope scope del ambito global
 */

static void checkMainMethodExists(Scope *globalScope) {
    Symbol *mainSymbol = lookupVariableCurrentScope(globalScope, "main");

    if (mainSymbol == NULL) {
        semanticError(0, "El programa no contiene la definicion del metodo 'main'.");
        return;
    }

    if (mainSymbol->symbolType != METHOD_SYMBOL) {
        semanticError(0, "'main' debe ser un metodo, no una variable.");
        return;
    }

    NodeAST *parametersNode = mainSymbol->parameters;
    int paramCount = parametersNode ? parametersNode->childCount : 0;

    if (paramCount != 0) {
        semanticError(0, "El metodo 'main' no debe tener parametros.");
    }
}


/**
 * @brief Determina si un nodo del AST garantiza un retorno en todos los caminos de ejecución.
 * @param node Nodo del AST a analizar.
 * @return true si garantiza un retorno en todos los caminos; false en caso contrario.
 */
static int guaranteesReturn(NodeAST *node) {
    if (node == NULL) return 0;

    switch (node->nodeType) {
        case RETURN_NODE:
            return 1;

        case BLOCK_NODE:
            for (int i = 0; i < node->childCount; i++) {
                if (guaranteesReturn(node->children[i])) return 1;
            }
            return 0;

        case IF_ELSE_NODE: {
            NodeAST *elseBlock = GET_ELSE_BLOCK(node);
            if (elseBlock == NULL) return 0;

            return guaranteesReturn(GET_IF_BLOCK(node)) && guaranteesReturn(elseBlock);
        }

        default:
            return 0;
    }
}

/**
 * @brief Analiza semanticamente la declaracion de un metodo.
 *
 * Registra el metodo en el ambito actual, crea su ambito lexico local,
 * analiza recursivamente sus parametros y cuerpo, y verifica que
 * los metodos con retorno, lo aseguren en todos sus caminos
 *
 * @param node Nodo del AST que representa la declaracion del metodo.
 * @param current_scope Ambito actual en el que se declara el metodo.
 */
static void analyzeMethodNode(NodeAST *node, Scope *current_scope) {
    Symbol *existing = lookupVariableCurrentScope(current_scope, node->symbol->id);
    if (existing != NULL) {
        semanticError(node->line, "El metodo '%s' ya fue declarado previamente en este ambito.", node->symbol->id);
        return;
    }

    appendSymbol(current_scope, node->symbol);

    int level = current_scope->level + 1;
    Scope *method_scope = createScope(level, current_scope, node->symbol);
    if (!method_scope) {
        semanticError(node->line, "Error: no se pudo reservar memoria para el ambito del metodo.");
        return;
    }

    NodeAST *params = GET_LEFT(node);
    if (params) {
        for (int i = 0; i < params->childCount; i++) {
            analyzeNodeSemantics(params->children[i], method_scope);
        }
    }

    NodeAST *body = GET_RIGHT(node);
    if (body) {
        for (int i = 0; i < body->childCount; i++) {
            analyzeNodeSemantics(body->children[i], method_scope);
        }
    }

    if (node->type != TYPE_VOID && !guaranteesReturn(body)) {
        const char *methodName = node->symbol->id ? node->symbol->id : "anonimo";
        semanticError(node->line, "El metodo '%s' no es void: todos sus caminos de ejecucion deben terminar en un return.", methodName);
    }

    appendChildScope(current_scope, method_scope);
}

/**
 * @brief Analiza semanticamente la declaracion de una variable.
 *
 * Verifica que el identificador no haya sido declarado previamente
 * en el ambito actual y lo registra en la tabla de simbolos.
 *
 * @param node Nodo del AST que representa la declaracion de la variable.
 * @param current_scope Ambito actual donde se declara la variable.
 */
static void analyzeVariableDeclarationNode(NodeAST *node, Scope *current_scope) {
    Symbol *existing = lookupVariableCurrentScope(current_scope, node->symbol->id);
    if (existing != NULL) {
        semanticError(node->line, "La variable '%s' ya fue declarada previamente en este ambito.", node->symbol->id);
        return;
    }
    appendSymbol(current_scope, node->symbol);
}

/**
 * @brief Analiza semanticamente una sentencia de asignacion.
 *
 * Verifica que la variable receptora exista y no sea un metodo,
 * evalua el tipo de la expresion asignada y comprueba la compatibilidad
 * de tipos antes de confirmar el tipo de la asignacion.
 *
 * @param node Nodo del AST que representa la asignacion.
 * @param current_scope Ambito actual donde se produce la asignacion.
 */
static void analyzeAssignmentNode(NodeAST *node, Scope *current_scope) {
    node->type = TYPE_ERROR;

    Symbol *resolvedSymbol = lookupVariable(current_scope, node->symbol->id);
    if (!resolvedSymbol) {
        semanticError(node->line, "Asignacion a variable no declarada '%s'.", node->symbol->id);
        return;
    }

    if (resolvedSymbol->symbolType != ID_SYMBOL) {
        semanticError(node->line, "Ese nombre de simbolo corresponde al metodo '%s'.", node->symbol->id);
        return;
    }

    node->symbol = resolvedSymbol;

    NodeAST *expression = GET_EXPRESSION(node);
    if (expression != NULL) {
        DataType exprType = analyzeExpression(expression, current_scope);

        if (exprType != TYPE_ERROR) {
            if (!isAssignable(resolvedSymbol->type, exprType)) {
                semanticError(node->line, "El tipo de la expresion no coincide con la variable '%s'.", node->symbol->id);
            } else {
                node->type = resolvedSymbol->type;
            }
        }
    }
}

/**
 * @brief Analiza semanticamente una sentencia de retorno.
 *
 * Verifica que la sentencia se encuentre dentro del cuerpo de una funcion,
 * que los metodos void no retornen expresiones con valor, y que los metodos
 * con tipo de retorno devuelvan una expresion de tipo compatible.
 *
 * @param node Nodo del AST que representa la sentencia return.
 * @param current_scope Ambito actual, utilizado para identificar la funcion contenedora.
 */
static void analyzeReturnNode(NodeAST *node, Scope *current_scope) {
    if (current_scope == NULL || current_scope->function == NULL) {
        semanticError(node->line, "Sentencia 'return' fuera del cuerpo de una funcion.");
        return;
    }

    Symbol *function = current_scope->function;
    DataType expectedType = function->type;
    const char *funcName = function->id ? function->id : "anonima";

    NodeAST *expression = GET_EXPRESSION(node);

    int hasReturn = (expression != NULL);

    if (!hasReturn) {
        if (expectedType != TYPE_VOID) {
            semanticError(node->line, "El metodo '%s' espera un valor de retorno de tipo %s, pero no se especifico ninguno.", funcName, getDataTypeName(expectedType));
        }
        return;
    }
    if (expectedType == TYPE_VOID) {
        semanticError(node->line, "El metodo '%s' es void y no deberia retornar un valor.", funcName);
        return;
    }

    DataType actualType = analyzeExpression(expression, current_scope);

    if (actualType != TYPE_ERROR && !isAssignable(expectedType, actualType)) {
        semanticError(node->line, "El tipo de retorno no coincide en el metodo '%s': se esperaba %s pero se obtuvo %s.", funcName, getDataTypeName(expectedType), getDataTypeName(actualType));
    }
}

/**
 * @brief Analiza semanticamente una sentencia de control while
 *
 * Evalua la condicion verificando que sea de tipo bool y analiza  recursivamente el cuerpo del bucle en el ambito actual.
 *
 * @param node Nodo del AST que representa el bucle while.
 * @param current_scope Ambito actual donde se evalua el while.
 */
static void analyzeWhileNode(NodeAST *node, Scope *current_scope) {
    NodeAST *condition = GET_CONDITION(node);
    DataType conditionType = analyzeExpression(condition, current_scope);

    if (conditionType != TYPE_ERROR && conditionType != TYPE_BOOL) {
        semanticError(node->line, "La condicion del while debe ser de tipo BOOL, pero es %s.", getDataTypeName(conditionType));
    }

    NodeAST *body = GET_IF_BLOCK(node);
    analyzeNodeSemantics(body, current_scope);
}

/**
 * @brief Analiza semanticamente una sentencia condicional if o if/else
 *
 * Verifica que la condicion evaluada sea de tipo bool y analiza
 * recursivamente los bloques then y else (en caso de existir)
 *
 * @param node Nodo del AST que representa la estructura if-else.
 * @param current_scope Ambito actual donde se evalua la sentencia.
 */
static void analyzeIfElseNode(NodeAST *node, Scope *current_scope) {
    NodeAST *condition = GET_CONDITION(node);
    DataType conditionType = analyzeExpression(condition, current_scope);

    if (conditionType != TYPE_ERROR && conditionType != TYPE_BOOL) {
        semanticError(node->line, "La condicion del if debe ser de tipo BOOL, pero es %s.", getDataTypeName(conditionType));
    }

    NodeAST *ifBlock = GET_IF_BLOCK(node);
    analyzeNodeSemantics(ifBlock, current_scope);

    NodeAST *elseBlock = GET_ELSE_BLOCK(node);
    if (elseBlock != NULL) {
        analyzeNodeSemantics(elseBlock, current_scope);
    }
}

/**
 * @brief Delega recursivamente el analisis semantico segun el tipo de nodo.
 *
 * Direcciona cada nodo del AST a su funcion analizadora correspondiente
 *
 * @param node Nodo del AST a analizar.
 * @param current_scope Ambito actual bajo el cual se procesa el nodo.
 */
static void analyzeNodeSemantics(NodeAST *node, Scope *current_scope) {
    if (!node) return;
    
    switch (node->nodeType) {
        case BLOCK_NODE:
            analyzeSemantics(node, current_scope);
            break;

        case METHOD_DECLARATION_NODE:
            analyzeMethodNode(node, current_scope);
            break;
            
        case METHOD_CALL_NODE:
            analyzeMethodCall(node, current_scope, 0); // 0 porque viene de Statement
            break;

        case VARIABLE_DECLARATION_NODE:
            analyzeVariableDeclarationNode(node, current_scope);
            break;
            
        case ASSIGNMENT_NODE:
            analyzeAssignmentNode(node, current_scope);
            break;

        case RETURN_NODE:
            analyzeReturnNode(node, current_scope);
            break;
 
        case IF_ELSE_NODE:
            analyzeIfElseNode(node, current_scope);
            break;

        case WHILE_NODE:
            analyzeWhileNode(node, current_scope);
            break;

        default:
            for (int i = 0; i < node->childCount; i++) {
                analyzeNodeSemantics(node->children[i], current_scope);
            }
            break;
    }
}
