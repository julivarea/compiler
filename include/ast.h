#ifndef AST_H
#define AST_H


#include <stdio.h>
#include "types.h"
#include "symbol_table.h"

typedef enum NodeType {
    VARIABLE_DECLARATION_NODE,  
    ID_NODE,                   
    ASSIGNMENT_NODE,           
    CONSTANT_NODE,             
    RETURN_NODE,             
    IF_ELSE_NODE,              
    WHILE_NODE,               
    BINARYOPERATOR_NODE,      
    BLOCK_NODE,                
    METHOD_CALL_NODE,         
    PARAMETERS_NODE,            
    METHOD_DECLARATION_NODE
} NodeType;

typedef struct NodeAST {
    NodeType nodeType;            
    DataType type;                  
    Symbol *symbol;                
    OperationType operationType;    
    int line;                     
    struct NodeAST **children;     
    int childCount;               
} NodeAST;




#define GET_EXPRESSION(node)   ((node)->childCount > 0 ? (node)->children[0] : NULL)
#define GET_LEFT(node)         ((node)->childCount > 0 ? (node)->children[0] : NULL)
#define GET_RIGHT(node)        ((node)->childCount > 1 ? (node)->children[1] : NULL)
#define GET_CONDITION(node)    ((node)->childCount > 0 ? (node)->children[0] : NULL)
#define GET_IF_BLOCK(node)     ((node)->childCount > 1 ? (node)->children[1] : NULL)
#define GET_ELSE_BLOCK(node)   ((node)->childCount > 2 ? (node)->children[2] : NULL)
#define GET_WHILE_BLOCK(node)  ((node)->childCount > 1 ? (node)->children[1] : NULL)

NodeAST *newNode(NodeType nodeType, Symbol *symbol, NodeAST *left, NodeAST *mid, NodeAST *right);
NodeAST *newBinaryOperatorNode(OperationType operator, NodeAST *left, NodeAST *right);
NodeAST *newMethodDeclaration(DataType returnType, char *methodName, NodeAST *parameters, NodeAST *body);
NodeAST *newIntLiteralNode(int value);
NodeAST *newFloatLiteralNode(float value);
NodeAST *newBoolLiteralNode(int value);

void fprintAST(FILE *out, NodeAST *node, int level);
void printAST(NodeAST *node, int level);

#endif // AST_H
