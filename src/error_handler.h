#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

extern int semantic_errors;

void semanticError(int line, const char *format, ...);

#endif
