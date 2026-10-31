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
    OPERATOR_NODE,      
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




#define GET_CHILD(node, index) ((node)->childCount > (index) ? (node)->children[(index)] : NULL)
#define GET_EXPRESSION(node)   GET_CHILD(node, 0)
#define GET_LEFT(node)         GET_CHILD(node, 0)
#define GET_RIGHT(node)        GET_CHILD(node, 1)
#define GET_CONDITION(node)    GET_CHILD(node, 0)
#define GET_IF_BLOCK(node)     GET_CHILD(node, 1)
#define GET_ELSE_BLOCK(node)   GET_CHILD(node, 2)
#define GET_WHILE_BLOCK(node)  GET_CHILD(node, 1)

NodeAST *newNode(NodeType nodeType, Symbol *symbol, NodeAST *left, NodeAST *mid, NodeAST *right);
NodeAST *newOperatorNode(OperationType operator, NodeAST *left, NodeAST *right);
NodeAST *newMethodDeclaration(DataType returnType, char *methodName, NodeAST *parameters, NodeAST *body);
NodeAST *newIntLiteralNode(int value);
NodeAST *newFloatLiteralNode(float value);
NodeAST *newBoolLiteralNode(int value);

void fprintAST(FILE *out, NodeAST *node, int level);
void printAST(NodeAST *node, int level);

#endif // AST_H
