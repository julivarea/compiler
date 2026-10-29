#ifndef STRING_BUILDER_H
#define STRING_BUILDER_H

#include <stddef.h>
#include <stdarg.h>

/**
 * Append a null-terminated C string to the internal buffer.
 *
 * Uso:
 *   buildString("LABEL_1:");
 *   buildString(node->symbol->id);
 *   buildString("\n");
 *
 *   // Se pueden encadenar llamadas; todo queda en el mismo buffer:
 *   buildString(node->symbol->id);   // "foo"
 *   buildString(":");                // "foo:"
 *   buildString("int");              // "foo:int"
 */
void buildString(const char *s);

/**
 * Append a formatted string (printf-style) to the internal buffer.
 * Both buildString and buildStringf share the same buffer — output
 * accumulates across any mix of calls.
 *
 * Uso:
 *   buildStringf("%s:", node->symbol->id);          // "foo:"
 *   buildStringf("t%d = %s + %s", tmp, l, r);      // "t0 = a + b"
 *   buildStringf("if %s goto L%d\n", cond, label); // "if t1 goto L3\n"
 *
 *   // Mezcla con buildString sin perder contenido:
 *   buildString("// metodo: ");
 *   buildStringf("%s/%d params\n", node->symbol->id, paramCount);
 */
void buildStringf(const char *fmt, ...);

/**
 * Retrieve the accumulated string (read-only).
 * Returns "" if nothing has been written yet.
 */
const char *getBuiltString(void);

/**
 * Reset the internal buffer, freeing allocated memory.
 * Call this before starting a new generation pass.
 */
void resetBuildString(void);

#endif /* STRING_BUILDER_H */
