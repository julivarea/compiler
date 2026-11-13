#include "tac_builder.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "string_builder.h"
#include "symbol_table.h"

#define MAX_LINE_LENGTH 1024
#define MAX_TOKENS 8
#define MAX_DECLARED_NAMES 1024

typedef struct {
    char name[TAC_NAME_LENGTH];   /* nombre calificado, por ejemplo "x@3" */
    DataType type;
    bool isGlobal;
} DeclaredVariable;

typedef struct {
    char name[TAC_NAME_LENGTH];
    bool returnsValue;
} DeclaredMethod;

/* Lo que la pasada previa averigua del texto completo antes de construir. */
static DeclaredVariable declaredVariables[MAX_DECLARED_NAMES];
static int declaredVariableCount = 0;
static DeclaredMethod declaredMethods[MAX_DECLARED_NAMES];
static int declaredMethodCount = 0;

/* Símbolos creados por buildTAC, compartidos por todos los operandos de una misma variable. */
static Symbol **symbolPool = NULL;
static int symbolPoolCount = 0;

static void scanDeclarations(const char *text);
static bool buildInstruction(TACList *list, char **tokens, int tokenCount);
static bool buildAssignment(TACList *list, char **tokens, int tokenCount);
static int tokenize(char *line, char **tokens);
static bool nextLine(const char **cursor, char *line);
static TACOperand operand(const char *token, bool *isValid);
static TACOperand variable(const char *qualifiedName);
static TACOperand method(const char *name);
static bool lookupOperation(const char *symbol, TACOperation *operation);
static DeclaredVariable *findDeclaredVariable(const char *qualifiedName);
static DeclaredMethod *findDeclaredMethod(const char *name);
static bool isTemporaryName(const char *token);
static bool isIntegerLiteral(const char *token);
static DataType parseDataType(const char *token);

TACList *buildTAC(const char *text) {
    declaredVariableCount = 0;
    declaredMethodCount = 0;
    scanDeclarations(text);

    TACList *list = createTACList();
    char line[MAX_LINE_LENGTH];
    const char *cursor = text;
    while (nextLine(&cursor, line)) {
        char *tokens[MAX_TOKENS];
        int tokenCount = tokenize(line, tokens);
        if (tokenCount == 0) continue;

        if (!buildInstruction(list, tokens, tokenCount)) {
            fprintf(stderr, "buildTAC: linea no reconocida: '%s'\n", line);
            freeTACList(list);
            return NULL;
        }
    }
    return list;
}

void releaseTACSymbols(void) {
    for (int i = 0; i < symbolPoolCount; i++) {
        freeSymbol(symbolPool[i]);
    }
    free(symbolPool);
    symbolPool = NULL;
    symbolPoolCount = 0;
}

const char *formatTAC(const TACList *list) {
    resetBuildString();
    formatTACList(list);
    return getBuiltString();
}

/* Registra tipos y alcance de las variables, y si cada método devuelve un valor. */
static void scanDeclarations(const char *text) {
    char line[MAX_LINE_LENGTH];
    const char *cursor = text;
    DeclaredMethod *currentMethod = NULL;

    while (nextLine(&cursor, line)) {
        char *tokens[MAX_TOKENS];
        int tokenCount = tokenize(line, tokens);
        if (tokenCount == 0) continue;

        bool isDeclaration = (strcmp(tokens[0], "global") == 0 || strcmp(tokens[0], "local") == 0 ||
                              strcmp(tokens[0], "param_decl") == 0);
        if (isDeclaration && tokenCount >= 3 && declaredVariableCount < MAX_DECLARED_NAMES) {
            DeclaredVariable *declared = &declaredVariables[declaredVariableCount++];
            snprintf(declared->name, sizeof(declared->name), "%s", tokens[1]);
            declared->type = parseDataType(tokens[2]);
            declared->isGlobal = (strcmp(tokens[0], "global") == 0);
        } else if (strcmp(tokens[0], "begin_func") == 0 && tokenCount >= 2 &&
                   declaredMethodCount < MAX_DECLARED_NAMES) {
            currentMethod = &declaredMethods[declaredMethodCount++];
            snprintf(currentMethod->name, sizeof(currentMethod->name), "%s", tokens[1]);
            currentMethod->returnsValue = false;
        } else if (strcmp(tokens[0], "ret") == 0 && tokenCount == 2 && currentMethod != NULL) {
            currentMethod->returnsValue = true;
        } else if (strcmp(tokens[0], "end_func") == 0) {
            currentMethod = NULL;
        }
    }
}

static bool buildInstruction(TACList *list, char **tokens, int tokenCount) {
    const char *keyword = tokens[0];
    bool isValid = true;

    if (tokenCount >= 2 && strcmp(tokens[1], "=") == 0) {
        return buildAssignment(list, tokens, tokenCount);
    }
    if (strcmp(keyword, "global") == 0 || strcmp(keyword, "local") == 0 || strcmp(keyword, "param_decl") == 0) {
        TACOperation operation = (keyword[0] == 'g') ? TAC_GLOBAL_DECL
                               : (keyword[0] == 'l') ? TAC_LOCAL_DECL : TAC_PARAM_DECL;
        appendInstruction(list, operation, variable(tokens[1]), tacNone(), tacNone());
        return true;
    }
    if (strcmp(keyword, "begin_func") == 0 && tokenCount == 3) {
        appendInstruction(list, TAC_BEGIN_FUNC, method(tokens[1]), tacConstantInt(atoi(tokens[2])), tacNone());
        return true;
    }
    if (strcmp(keyword, "end_func") == 0 && tokenCount == 2) {
        appendInstruction(list, TAC_END_FUNC, method(tokens[1]), tacNone(), tacNone());
        return true;
    }
    if (tokenCount == 1 && keyword[strlen(keyword) - 1] == ':') {
        tokens[0][strlen(keyword) - 1] = '\0';
        appendInstruction(list, TAC_LABEL, tacLabel(tokens[0]), tacNone(), tacNone());
        return true;
    }
    if (strcmp(keyword, "goto") == 0 && tokenCount == 2) {
        appendInstruction(list, TAC_GOTO, tacNone(), tacLabel(tokens[1]), tacNone());
        return true;
    }
    if (strcmp(keyword, "if") == 0 && tokenCount == 5 && strcmp(tokens[3], "goto") == 0) {
        TACOperation operation = (strcmp(tokens[2], "true") == 0) ? TAC_IF_TRUE : TAC_IF_FALSE;
        TACOperand condition = operand(tokens[1], &isValid);
        appendInstruction(list, operation, tacNone(), condition, tacLabel(tokens[4]));
        return isValid;
    }
    if (strcmp(keyword, "param") == 0 && tokenCount == 2) {
        appendInstruction(list, TAC_PARAM, tacNone(), operand(tokens[1], &isValid), tacNone());
        return isValid;
    }
    if (strcmp(keyword, "call") == 0 && tokenCount == 3) {
        appendInstruction(list, TAC_CALL, tacNone(), method(tokens[1]), tacConstantInt(atoi(tokens[2])));
        return true;
    }
    if (strcmp(keyword, "ret") == 0 && tokenCount <= 2) {
        TACOperand value = (tokenCount == 2) ? operand(tokens[1], &isValid) : tacNone();
        appendInstruction(list, TAC_RETURN, tacNone(), value, tacNone());
        return isValid;
    }
    return false;
}

/* Las formas "r = call f, n", "r = x", "r = -x", "r = !x" y "r = a op b". */
static bool buildAssignment(TACList *list, char **tokens, int tokenCount) {
    bool isValid = true;
    TACOperand result = operand(tokens[0], &isValid);

    if (tokenCount == 5 && strcmp(tokens[2], "call") == 0) {
        appendInstruction(list, TAC_CALL, result, method(tokens[3]), tacConstantInt(atoi(tokens[4])));
        return isValid;
    }
    if (tokenCount == 3) {
        const char *source = tokens[2];
        bool isNegation = (source[0] == '-' && source[1] != '\0');
        bool isLogicalNot = (source[0] == '!' && source[1] != '\0');
        if (isNegation || isLogicalNot) {
            TACOperation operation = isNegation ? TAC_UMINUS : TAC_NOT;
            result.type = isNegation ? TYPE_INT : TYPE_BOOL;
            appendInstruction(list, operation, result, operand(source + 1, &isValid), tacNone());
        } else {
            appendInstruction(list, TAC_ASSIGN, result, operand(source, &isValid), tacNone());
        }
        return isValid;
    }
    if (tokenCount == 5) {
        TACOperation operation;
        if (!lookupOperation(tokens[3], &operation)) return false;
        bool isRelational = (operation == TAC_LT || operation == TAC_GT || operation == TAC_EQ);
        if (result.kind == OPERAND_TEMP) {
            result.type = isRelational ? TYPE_BOOL : TYPE_INT;
        }
        TACOperand left = operand(tokens[2], &isValid);
        TACOperand right = operand(tokens[4], &isValid);
        appendInstruction(list, operation, result, left, right);
        return isValid;
    }
    return false;
}

static TACOperand operand(const char *token, bool *isValid) {
    if (isTemporaryName(token)) return tacTemp(token, TYPE_INT);
    if (strcmp(token, "true") == 0) return tacConstantBool(1);
    if (strcmp(token, "false") == 0) return tacConstantBool(0);
    if (isIntegerLiteral(token)) return tacConstantInt(atoi(token));
    if (strchr(token, '@') != NULL) return variable(token);

    fprintf(stderr, "buildTAC: operando no reconocido: '%s'\n", token);
    *isValid = false;
    return tacNone();
}

static TACOperand variable(const char *qualifiedName) {
    for (int i = 0; i < symbolPoolCount; i++) {
        TACOperand existing = tacVariable(symbolPool[i]);
        if (strcmp(existing.name, qualifiedName) == 0) {
            return existing;
        }
    }

    const char *separator = strchr(qualifiedName, '@');
    size_t nameLength = (size_t)(separator - qualifiedName);
    char *id = malloc(nameLength + 1);
    memcpy(id, qualifiedName, nameLength);
    id[nameLength] = '\0';

    DeclaredVariable *declared = findDeclaredVariable(qualifiedName);
    Symbol *symbol = newSymbol(id, VARIABLE_SYMBOL, declared != NULL ? declared->type : TYPE_INT, NULL);
    symbol->uid = atoi(separator + 1);
    symbol->isGlobal = (declared != NULL && declared->isGlobal);

    symbolPool = realloc(symbolPool, sizeof(Symbol *) * (size_t)(symbolPoolCount + 1));
    symbolPool[symbolPoolCount++] = symbol;
    return tacVariable(symbol);
}

static TACOperand method(const char *name) {
    DeclaredMethod *declared = findDeclaredMethod(name);
    bool returnsValue = (declared == NULL || declared->returnsValue);
    return tacMethod(name, returnsValue ? TYPE_INT : TYPE_VOID);
}

static bool lookupOperation(const char *symbol, TACOperation *operation) {
    static const struct { const char *symbol; TACOperation operation; } OPERATIONS[] = {
        {"+", TAC_ADD}, {"-", TAC_SUB}, {"*", TAC_MUL}, {"/", TAC_DIV}, {"%", TAC_MOD},
        {"<", TAC_LT}, {">", TAC_GT}, {"==", TAC_EQ},
    };
    for (size_t i = 0; i < sizeof(OPERATIONS) / sizeof(OPERATIONS[0]); i++) {
        if (strcmp(symbol, OPERATIONS[i].symbol) == 0) {
            *operation = OPERATIONS[i].operation;
            return true;
        }
    }
    return false;
}

static int tokenize(char *line, char **tokens) {
    for (char *c = line; *c != '\0'; c++) {
        if (*c == ',') *c = ' ';
    }
    int count = 0;
    char *saveState = NULL;
    for (char *token = strtok_r(line, " \t", &saveState); token != NULL && count < MAX_TOKENS;
         token = strtok_r(NULL, " \t", &saveState)) {
        tokens[count++] = token;
    }
    return count;
}

static bool nextLine(const char **cursor, char *line) {
    if (**cursor == '\0') return false;
    const char *end = strchr(*cursor, '\n');
    size_t length = (end != NULL) ? (size_t)(end - *cursor) : strlen(*cursor);
    if (length >= MAX_LINE_LENGTH) length = MAX_LINE_LENGTH - 1;
    memcpy(line, *cursor, length);
    line[length] = '\0';
    *cursor = (end != NULL) ? end + 1 : *cursor + strlen(*cursor);
    return true;
}

static DeclaredVariable *findDeclaredVariable(const char *qualifiedName) {
    for (int i = 0; i < declaredVariableCount; i++) {
        if (strcmp(declaredVariables[i].name, qualifiedName) == 0) return &declaredVariables[i];
    }
    return NULL;
}

static DeclaredMethod *findDeclaredMethod(const char *name) {
    for (int i = 0; i < declaredMethodCount; i++) {
        if (strcmp(declaredMethods[i].name, name) == 0) return &declaredMethods[i];
    }
    return NULL;
}

static bool isTemporaryName(const char *token) {
    if (token[0] != 't' || token[1] == '\0') return false;
    for (const char *c = token + 1; *c != '\0'; c++) {
        if (!isdigit((unsigned char)*c)) return false;
    }
    return true;
}

static bool isIntegerLiteral(const char *token) {
    const char *digits = (token[0] == '-') ? token + 1 : token;
    if (*digits == '\0') return false;
    for (const char *c = digits; *c != '\0'; c++) {
        if (!isdigit((unsigned char)*c)) return false;
    }
    return true;
}

static DataType parseDataType(const char *token) {
    if (strcmp(token, "BOOL") == 0) return TYPE_BOOL;
    return TYPE_INT;
}
