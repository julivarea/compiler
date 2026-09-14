#ifndef AST_H
#define AST_H

typedef enum Node_Type {
   ASSIGNMENT_NODE,
   BINOP_NODE,
   CONSTANT_NODE,
   RETURN_NODE,
   DEFINITION_NODE
} Node_Type;


typedef struct ASTNode ASTNode;

typedef struct {
    ASTNode* statements;
} ASTProgram;

typedef struct {
    ASTNode* statement;
    ASTNode* next;
} ASTStatementList;

typedef struct {
    int data_type;
    char* var_name;
} ASTDeclaration;

typedef struct {
    char* var_name;
    ASTNode* expression;
} ASTAssignment;

typedef struct {
    ASTNode* expression;
} ASTReturn;

typedef enum {
    OP_ADD,
    OP_SUB,
    OP_MUL
} Operator;

typedef struct {
    ASTNode* left;
    Operator op;
    ASTNode* right;
} ASTBinOp;

typedef struct {
    int value;
} ASTConstant;

typedef struct {
    char* var_name;
} ASTId;

struct ASTNode {
    Node_Type type;
    void (*free_node)(struct ASTNode*);

    union {
        ASTProgram       program;
        ASTStatementList statement_list;
        ASTDeclaration   declaration;
        ASTAssignment    assignment;
        ASTReturn        return_stmt;
        ASTBinOp         binop;
        ASTConstant      constant;
        ASTId            id;
    } as;
};

ASTNode* create_program_node(ASTNode* statements);
ASTNode* create_statement_list_node(ASTNode* statement, ASTNode* next);
ASTNode* create_declaration_node(int data_type, char* var_name);
ASTNode* create_assignment_node(char* var_name, ASTNode* expression);
ASTNode* create_return_node(ASTNode* expression);
ASTNode* create_binop_node(ASTNode* left, Operator op, ASTNode* right);
ASTNode* create_constant_node(int value);
ASTNode* create_id_node(char* var_name);
void free_ast(ASTNode* node);

#endif // AST_H
