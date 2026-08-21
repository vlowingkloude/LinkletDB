#include "gql_lexer.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

enum {
    GQL_DECODED_TEXT_INITIAL_CAPACITY_FACTOR = 4,
    GQL_DECODED_TEXT_CAPACITY_GROWTH_FACTOR = 2,
};

typedef struct KeywordEntry {
    const char *text;
    GqlTokenKind kind;
    int nonreserved;
} KeywordEntry;

static const KeywordEntry keyword_table[] = {
#define GQL_LEXER_TABLE_ITEM(name, text, nonreserved) {text, TOK_##name, nonreserved},
    GQL_KEYWORD_LIST(GQL_LEXER_TABLE_ITEM)
#undef GQL_LEXER_TABLE_ITEM
};

static int ascii_eq_nocase(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (toupper((unsigned char)a[i]) != toupper((unsigned char)b[i])) {
            return 0;
        }
    }
    return 1;
}

static GqlTokenKind keyword_lookup(const char *s, size_t n) {
    const size_t count = sizeof(keyword_table) / sizeof(keyword_table[0]);
    for (size_t i = 0; i < count; i++) {
        const KeywordEntry *e = &keyword_table[i];
        if (strlen(e->text) == n && ascii_eq_nocase(e->text, s, n)) {
            return e->kind;
        }
    }
    return TOK_IDENT;
}

int gql_token_nonreserved(GqlTokenKind kind) {
    const size_t count = sizeof(keyword_table) / sizeof(keyword_table[0]);
    for (size_t i = 0; i < count; i++) {
        if (keyword_table[i].kind == kind) {
            return keyword_table[i].nonreserved;
        }
    }
    return 0;
}

static int is_ascii_digit(int c) {
    return c >= '0' && c <= '9';
}
static int is_hex_digit(int c) {
    return is_ascii_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static int is_ident_start_byte(unsigned char c) {
    if (c >= 0x80) {
        return 1;
    }
    if (c == '_') {
        return 1;
    }
    if (c >= 'a' && c <= 'z') {
        return 1;
    }
    if (c >= 'A' && c <= 'Z') {
        return 1;
    }
    return 0;
}

static int is_ident_cont_byte(unsigned char c) {
    if (c >= 0x80) {
        return 1;
    }
    if (c == '_') {
        return 1;
    }
    if (is_ascii_digit(c)) {
        return 1;
    }
    if (c >= 'a' && c <= 'z') {
        return 1;
    }
    if (c >= 'A' && c <= 'Z') {
        return 1;
    }
    return 0;
}

static size_t utf8_ws_len(const unsigned char *p, size_t avail) {
    if (avail >= 2 && p[0] == 0xC2 && p[1] == 0xA0) {
        return 2;
    }
    if (avail >= 3 && p[0] == 0xE1 && p[1] == 0x9A && p[2] == 0x80) {
        return 3;
    }
    if (avail >= 3 && p[0] == 0xE1 && p[1] == 0xA0 && p[2] == 0x8E) {
        return 3;
    }
    if (avail >= 3 && p[0] == 0xE2 && p[1] == 0x80) {
        unsigned char c = p[2];
        if (c >= 0x80 && c <= 0x8A) {
            return 3;
        }
        if (c == 0xA8 || c == 0xA9 || c == 0xAF) {
            return 3;
        }
    }
    if (avail >= 3 && p[0] == 0xE2 && p[1] == 0x81 && p[2] == 0x9F) {
        return 3;
    }
    if (avail >= 3 && p[0] == 0xE3 && p[1] == 0x80 && p[2] == 0x80) {
        return 3;
    }
    return 0;
}

static int is_ws_byte(unsigned char c) {
    switch (c) {
    case ' ':
    case '\t':
    case '\n':
    case '\v':
    case '\f':
    case '\r':
    case 0x1C:
    case 0x1D:
    case 0x1E:
    case 0x1F:
        return 1;
    default:
        return 0;
    }
}

static int at_end(const GqlLexer *lexer) {
    return lexer->position >= lexer->length;
}
static int peek_byte(const GqlLexer *lexer, size_t off) {
    return (lexer->position + off < lexer->length)
               ? (unsigned char)lexer->source[lexer->position + off]
               : -1;
}

static void bump(GqlLexer *lexer) {
    unsigned char c = (unsigned char)lexer->source[lexer->position];
    lexer->position++;
    if (c == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else if (c == '\r') {
        if (lexer->position < lexer->length && lexer->source[lexer->position] == '\n') {
            lexer->position++;
        }
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }
}

static void skip_ws_and_comments(GqlLexer *lexer) {
    for (;;) {
        if (at_end(lexer)) {
            return;
        }
        unsigned char c = (unsigned char)lexer->source[lexer->position];

        size_t whitespace_length =
            utf8_ws_len((const unsigned char *)lexer->source + lexer->position,
                        lexer->length - lexer->position);
        if (whitespace_length > 0) {
            lexer->position += whitespace_length;
            lexer->column++;
            continue;
        }
        if (is_ws_byte(c)) {
            bump(lexer);
            continue;
        }

        if (c == '/' && peek_byte(lexer, 1) == '*') {
            bump(lexer);
            bump(lexer);
            for (;;) {
                if (at_end(lexer)) {
                    return;
                }
                if (lexer->source[lexer->position] == '*' && peek_byte(lexer, 1) == '/') {
                    bump(lexer);
                    bump(lexer);
                    break;
                }
                bump(lexer);
            }
            continue;
        }
        if (c == '/' && peek_byte(lexer, 1) == '/') {
            bump(lexer);
            bump(lexer);
            while (!at_end(lexer) && lexer->source[lexer->position] != '\n' &&
                   lexer->source[lexer->position] != '\r') {
                bump(lexer);
            }
            continue;
        }
        if (c == '-' && peek_byte(lexer, 1) == '-') {
            bump(lexer);
            bump(lexer);
            while (!at_end(lexer) && lexer->source[lexer->position] != '\n' &&
                   lexer->source[lexer->position] != '\r') {
                bump(lexer);
            }
            continue;
        }
        return;
    }
}

static int hex_value(int c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static size_t utf8_encode(unsigned long codepoint, char *output) {
    if (codepoint <= 0x7F) {
        output[0] = (char)codepoint;
        return 1;
    }
    if (codepoint <= 0x7FF) {
        output[0] = (char)(0xC0 | (codepoint >> 6));
        output[1] = (char)(0x80 | (codepoint & 0x3F));
        return 2;
    }
    if (codepoint <= 0xFFFF) {
        output[0] = (char)(0xE0 | (codepoint >> 12));
        output[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        output[2] = (char)(0x80 | (codepoint & 0x3F));
        return 3;
    }
    output[0] = (char)(0xF0 | (codepoint >> 18));
    output[1] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
    output[2] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
    output[3] = (char)(0x80 | (codepoint & 0x3F));
    return 4;
}

static char *decode_quoted(const char *s, size_t length, char delimiter) {

    if (length < 2) {
        char *output = malloc(1);
        if (output) {
            output[0] = '\0';
        }
        return output;
    }
    const char *body = s + 1;
    const char *body_end = s + length - 1;

    size_t capacity = (length + 1) * GQL_DECODED_TEXT_INITIAL_CAPACITY_FACTOR;
    char *output = malloc(capacity);
    if (!output) {
        return NULL;
    }
    size_t n = 0;

    while (body < body_end) {
        unsigned char c = (unsigned char)*body;
        if (c == '\\') {
            body++;
            if (body >= body_end) {
                break;
            }
            int e = (unsigned char)*body;
            char buffer[4];
            size_t bytes_written = 1;
            switch (e) {
            case '\\':
                buffer[0] = '\\';
                break;
            case '\'':
                buffer[0] = '\'';
                break;
            case '"':
                buffer[0] = '"';
                break;
            case '`':
                buffer[0] = '`';
                break;
            case 't':
                buffer[0] = '\t';
                break;
            case 'b':
                buffer[0] = '\b';
                break;
            case 'n':
                buffer[0] = '\n';
                break;
            case 'r':
                buffer[0] = '\r';
                break;
            case 'f':
                buffer[0] = '\f';
                break;
            case 'u':
            case 'U': {
                int ndigits = (e == 'u') ? 4 : 6;
                unsigned long codepoint = 0;
                int ok = 1;
                for (int i = 0; i < ndigits; i++) {
                    body++;
                    if (body >= body_end) {
                        ok = 0;
                        break;
                    }
                    int h = hex_value((unsigned char)*body);
                    if (h < 0) {
                        ok = 0;
                        break;
                    }
                    codepoint = codepoint * 16 + (unsigned long)h;
                }
                if (!ok) {
                    buffer[0] = '?';
                    bytes_written = 1;
                } else {
                    bytes_written = utf8_encode(codepoint, buffer);
                }
                break;
            }
            default:
                buffer[0] = (char)e;
                break;
            }
            if (n + bytes_written + 1 > capacity) {
                capacity = capacity * GQL_DECODED_TEXT_CAPACITY_GROWTH_FACTOR + bytes_written;
                char *grow = realloc(output, capacity);
                if (!grow) {
                    free(output);
                    return NULL;
                }
                output = grow;
            }
            memcpy(output + n, buffer, bytes_written);
            n += bytes_written;
            body++;
        } else if (c == (unsigned char)delimiter && body + 1 < body_end &&
                   (unsigned char)body[1] == (unsigned char)delimiter) {

            output[n++] = delimiter;
            body += 2;
        } else {
            output[n++] = (char)c;
            body++;
        }
    }
    output[n] = '\0';
    return output;
}

char *gql_token_decoded_text(const GqlToken *token) {
    if (token->kind == TOK_STR_LIT || token->kind == TOK_DELIM_IDENT) {
        char delimiter;
        switch (token->quote) {
        case GQL_QUOTE_ACCENT:
            delimiter = '`';
            break;
        case GQL_QUOTE_DOUBLE:
            delimiter = '"';
            break;
        default:
            delimiter = '\'';
            break;
        }
        return decode_quoted(token->start, token->length, delimiter);
    }
    size_t n = token->length;
    char *output = malloc(n + 1);
    if (!output) {
        return NULL;
    }
    memcpy(output, token->start, n);
    output[n] = '\0';
    return output;
}

static void lex_string(GqlLexer *lexer, GqlToken *output, char delimiter, GqlQuoteKind quote,
                       int no_escape) {
    (void)no_escape;
    size_t start = lexer->position;
    size_t line = lexer->line, column = lexer->column;
    bump(lexer);
    for (;;) {
        if (at_end(lexer)) {

            output->kind = TOK_ERROR;
            output->start = lexer->source + start;
            output->length = lexer->position - start;
            output->line = line;
            output->column = column;
            output->quote = GQL_QUOTE_NONE;
            output->is_float = 0;
            return;
        }
        unsigned char c = (unsigned char)lexer->source[lexer->position];
        if (c == (unsigned char)delimiter) {
            if (peek_byte(lexer, 1) == (int)(unsigned char)delimiter) {
                bump(lexer);
                bump(lexer);
                continue;
            }
            bump(lexer);
            break;
        }
        if (c == '\\') {
            bump(lexer);
            if (!at_end(lexer)) {
                bump(lexer);
            }
            continue;
        }
        bump(lexer);
    }
    output->kind = (delimiter == '`') ? TOK_DELIM_IDENT : TOK_STR_LIT;
    output->start = lexer->source + start;
    output->length = lexer->position - start;
    output->line = line;
    output->column = column;
    output->quote = quote;
    output->is_float = 0;
}

static void lex_byte_string(GqlLexer *lexer, GqlToken *output) {
    size_t start = lexer->position;
    size_t line = lexer->line, column = lexer->column;
    bump(lexer);
    bump(lexer);
    int bad = 0;
    for (;;) {
        if (at_end(lexer)) {
            bad = 1;
            break;
        }
        unsigned char c = (unsigned char)lexer->source[lexer->position];
        if (c == '\'') {
            bump(lexer);
            break;
        }
        if (c == ' ' || is_hex_digit(c)) {
            bump(lexer);
        } else {
            bad = 1;
            bump(lexer);
        }
    }
    output->kind = bad ? TOK_ERROR : TOK_BYTE_STRING;
    output->start = lexer->source + start;
    output->length = lexer->position - start;
    output->line = line;
    output->column = column;
    output->quote = GQL_QUOTE_NONE;
    output->is_float = 0;
}

static void lex_identifier(GqlLexer *lexer, GqlToken *output) {
    size_t start = lexer->position;
    size_t line = lexer->line, column = lexer->column;
    bump(lexer);
    while (!at_end(lexer) && is_ident_cont_byte((unsigned char)lexer->source[lexer->position])) {
        bump(lexer);
    }
    size_t n = lexer->position - start;
    const char *s = lexer->source + start;

    if (n == 4 && ascii_eq_nocase(s, "TRUE", 4)) {
        output->kind = TOK_BOOLEAN_LITERAL;
    } else if (n == 5 && ascii_eq_nocase(s, "FALSE", 5)) {
        output->kind = TOK_BOOLEAN_LITERAL;
    } else if (n == 7 && ascii_eq_nocase(s, "UNKNOWN", 7)) {
        output->kind = TOK_BOOLEAN_LITERAL;
    } else {
        output->kind = keyword_lookup(s, n);
    }
    output->start = s;
    output->length = n;
    output->line = line;
    output->column = column;
    output->quote = GQL_QUOTE_NONE;
    output->is_float = 0;
}

static void lex_number(GqlLexer *lexer, GqlToken *output) {
    size_t start = lexer->position;
    size_t line = lexer->line, column = lexer->column;
    int is_float = 0;

    if (lexer->source[lexer->position] == '0' &&
        (peek_byte(lexer, 1) == 'x' || peek_byte(lexer, 1) == 'X')) {
        bump(lexer);
        bump(lexer);
        int any = 0;
        while (!at_end(lexer)) {
            unsigned char c = (unsigned char)lexer->source[lexer->position];
            if (c == '_') {
                bump(lexer);
                continue;
            }
            if (is_hex_digit(c)) {
                any = 1;
                bump(lexer);
                continue;
            }
            break;
        }
        if (!any) {
            output->kind = TOK_ERROR;
            goto number_error;
        }
    } else if (lexer->source[lexer->position] == '0' &&
               (peek_byte(lexer, 1) == 'o' || peek_byte(lexer, 1) == 'O')) {
        bump(lexer);
        bump(lexer);
        int any = 0;
        while (!at_end(lexer)) {
            unsigned char c = (unsigned char)lexer->source[lexer->position];
            if (c == '_') {
                bump(lexer);
                continue;
            }
            if (c >= '0' && c <= '7') {
                any = 1;
                bump(lexer);
                continue;
            }
            break;
        }
        if (!any) {
            output->kind = TOK_ERROR;
            goto number_error;
        }
    } else if (lexer->source[lexer->position] == '0' &&
               (peek_byte(lexer, 1) == 'b' || peek_byte(lexer, 1) == 'B')) {
        bump(lexer);
        bump(lexer);
        int any = 0;
        while (!at_end(lexer)) {
            unsigned char c = (unsigned char)lexer->source[lexer->position];
            if (c == '_') {
                bump(lexer);
                continue;
            }
            if (c == '0' || c == '1') {
                any = 1;
                bump(lexer);
                continue;
            }
            break;
        }
        if (!any) {
            output->kind = TOK_ERROR;
            goto number_error;
        }
    } else {

        while (!at_end(lexer) && (is_ascii_digit((unsigned char)lexer->source[lexer->position]) ||
                                  lexer->source[lexer->position] == '_')) {
            bump(lexer);
        }
        if (!at_end(lexer) && lexer->source[lexer->position] == '.') {
            is_float = 1;
            bump(lexer);
            while (!at_end(lexer) &&
                   (is_ascii_digit((unsigned char)lexer->source[lexer->position]) ||
                    lexer->source[lexer->position] == '_')) {
                bump(lexer);
            }
        }
        if (!at_end(lexer) &&
            (lexer->source[lexer->position] == 'e' || lexer->source[lexer->position] == 'E')) {
            is_float = 1;
            bump(lexer);
            if (!at_end(lexer) &&
                (lexer->source[lexer->position] == '+' || lexer->source[lexer->position] == '-')) {
                bump(lexer);
            }
            while (!at_end(lexer) &&
                   is_ascii_digit((unsigned char)lexer->source[lexer->position])) {
                bump(lexer);
            }
        }
        if (!at_end(lexer)) {
            unsigned char c = (unsigned char)lexer->source[lexer->position];
            if (c == 'F' || c == 'f' || c == 'D' || c == 'd') {
                is_float = 1;
                bump(lexer);
            } else if (c == 'M' || c == 'm') {
                bump(lexer);
            }
        }
    }

    output->kind = TOK_INT_LIT;
    output->start = lexer->source + start;
    output->length = lexer->position - start;
    output->line = line;
    output->column = column;
    output->quote = GQL_QUOTE_NONE;
    output->is_float = is_float;
    return;

number_error:
    output->start = lexer->source + start;
    output->length = lexer->position - start;
    output->line = line;
    output->column = column;
    output->quote = GQL_QUOTE_NONE;
    output->is_float = 0;
}

static void lex_param(GqlLexer *lexer, GqlToken *output) {
    size_t start = lexer->position;
    size_t line = lexer->line, column = lexer->column;
    int is_double = 0;
    if (lexer->source[lexer->position] == '$' && peek_byte(lexer, 1) == '$') {
        bump(lexer);
        bump(lexer);
        is_double = 1;
    } else {
        bump(lexer);
    }

    if (!at_end(lexer)) {
        unsigned char c = (unsigned char)lexer->source[lexer->position];
        if (c == '"' || c == '\'' || c == '`') {
            char delimiter = (char)c;
            bump(lexer);
            while (true) {
                if (at_end(lexer)) {
                    break;
                }
                unsigned char cc = (unsigned char)lexer->source[lexer->position];
                if (cc == (unsigned char)delimiter) {
                    bump(lexer);
                    break;
                }
                if (cc == '\\') {
                    bump(lexer);
                    if (!at_end(lexer)) {
                        bump(lexer);
                    }
                    continue;
                }
                bump(lexer);
            }
        } else if (is_ident_start_byte(c)) {
            bump(lexer);
            while (!at_end(lexer) &&
                   is_ident_cont_byte((unsigned char)lexer->source[lexer->position])) {
                bump(lexer);
            }
        }
    }
    (void)is_double;
    output->kind = TOK_PARAM;
    output->start = lexer->source + start;
    output->length = lexer->position - start;
    output->line = line;
    output->column = column;
    output->quote = GQL_QUOTE_NONE;
    output->is_float = 0;
}

static int try_op(GqlLexer *lexer, const char *s) {
    size_t n = strlen(s);
    if (lexer->position + n <= lexer->length &&
        memcmp(lexer->source + lexer->position, s, n) == 0) {
        while (n--) {
            bump(lexer);
        }
        return 1;
    }
    return 0;
}

static void lex_one(GqlLexer *lexer, GqlToken *output) {
    skip_ws_and_comments(lexer);

    size_t start = lexer->position;
    size_t line = lexer->line, column = lexer->column;

    if (at_end(lexer)) {
        output->kind = TOK_EOF;
        output->start = lexer->source + start;
        output->length = 0;
        output->line = line;
        output->column = column;
        output->quote = GQL_QUOTE_NONE;
        output->is_float = 0;
        return;
    }

    unsigned char c = (unsigned char)lexer->source[lexer->position];

    if ((c == 'x' || c == 'X') && peek_byte(lexer, 1) == '\'') {
        lex_byte_string(lexer, output);
        return;
    }

    if (c == '@' &&
        (peek_byte(lexer, 1) == '\'' || peek_byte(lexer, 1) == '"' || peek_byte(lexer, 1) == '`')) {
        bump(lexer);
        int nxt = peek_byte(lexer, 0);
        if (nxt == '\'') {
            lex_string(lexer, output, '\'', GQL_QUOTE_SINGLE, 1);
            return;
        }
        if (nxt == '"') {
            lex_string(lexer, output, '"', GQL_QUOTE_DOUBLE, 1);
            return;
        }
        if (nxt == '`') {
            lex_string(lexer, output, '`', GQL_QUOTE_ACCENT, 1);
            return;
        }
    }

    if (c == '\'') {
        lex_string(lexer, output, '\'', GQL_QUOTE_SINGLE, 0);
        return;
    }
    if (c == '"') {
        lex_string(lexer, output, '"', GQL_QUOTE_DOUBLE, 0);
        return;
    }
    if (c == '`') {
        lex_string(lexer, output, '`', GQL_QUOTE_ACCENT, 0);
        return;
    }

    if (c == '$') {
        lex_param(lexer, output);
        return;
    }

    if (is_ascii_digit(c)) {
        lex_number(lexer, output);
        return;
    }
    if (is_ident_start_byte(c)) {
        lex_identifier(lexer, output);
        return;
    }

#define OP3(tk, str)                                                                               \
    if (try_op(lexer, str)) {                                                                      \
        output->kind = tk;                                                                         \
        goto done_op;                                                                              \
    }
#define OP2(tk, str)                                                                               \
    if (try_op(lexer, str)) {                                                                      \
        output->kind = tk;                                                                         \
        goto done_op;                                                                              \
    }

    OP3(TOK_MULTISET_ALTERNATION, "|+|")
    OP3(TOK_BRACKET_TILDE_RIGHT_ARROW, "]~>")
    OP3(TOK_BRACKET_RIGHT_ARROW, "]->")
    OP3(TOK_LEFT_ARROW_TILDE_BRACKET, "<~[")
    OP3(TOK_LEFT_MINUS_RIGHT, "<->")
    OP3(TOK_LEFT_TILDE_SLASH, "<~/")
    OP3(TOK_LEFT_MINUS_SLASH, "<-/")
    OP3(TOK_LEFT_ARROW_BRACKET, "<-[")
    OP3(TOK_SLASH_MINUS_RIGHT, "/->")
    OP3(TOK_SLASH_TILDE_RIGHT, "/~>")
    OP2(TOK_CONCATENATION, "||")
    OP2(TOK_DOUBLE_COLON, "::")
    OP2(TOK_DOUBLE_DOLLAR, "$$")
    OP2(TOK_DOUBLE_PERIOD, "..")
    OP2(TOK_GE, ">=")
    OP2(TOK_LEFT_ARROW, "<-")
    OP2(TOK_LEFT_ARROW_TILDE, "<~")
    OP2(TOK_LE, "<=")
    OP2(TOK_NE, "<>")
    OP2(TOK_RIGHT_ARROW, "->")
    OP2(TOK_MINUS_LEFT_BRACKET, "-[")
    OP2(TOK_MINUS_SLASH, "-/")
    OP2(TOK_RIGHT_BRACKET_MINUS, "]-")
    OP2(TOK_RIGHT_BRACKET_TILDE, "]~")
    OP2(TOK_RIGHT_DOUBLE_ARROW, "=>")
    OP2(TOK_SLASH_MINUS, "/-")
    OP2(TOK_SLASH_TILDE, "/~")
    OP2(TOK_TILDE_LEFT_BRACKET, "~[")
    OP2(TOK_TILDE_RIGHT_ARROW, "~>")
    OP2(TOK_TILDE_SLASH, "~/")

#undef OP3
#undef OP2

    switch (c) {
    case '&':
        output->kind = TOK_AMPERSAND;
        break;
    case '*':
        output->kind = TOK_ASTERISK;
        break;
    case ':':
        output->kind = TOK_COLON;
        break;
    case ',':
        output->kind = TOK_COMMA;
        break;
    case '@':
        output->kind = TOK_COMMERCIAL_AT;
        break;
    case '=':
        output->kind = TOK_EQUALS;
        break;
    case '!':
        output->kind = TOK_EXCLAMATION_MARK;
        break;
    case '>':
        output->kind = TOK_RIGHT_ANGLE;
        break;
    case '{':
        output->kind = TOK_LBRACE;
        break;
    case '[':
        output->kind = TOK_LBRACKET;
        break;
    case '(':
        output->kind = TOK_LPAREN;
        break;
    case '<':
        output->kind = TOK_LEFT_ANGLE;
        break;
    case '-':
        output->kind = TOK_MINUS;
        break;
    case '%':
        output->kind = TOK_PERCENT;
        break;
    case '.':
        output->kind = TOK_PERIOD;
        break;
    case '+':
        output->kind = TOK_PLUS;
        break;
    case '?':
        output->kind = TOK_QUESTION_MARK;
        break;
    case '}':
        output->kind = TOK_RBRACE;
        break;
    case ']':
        output->kind = TOK_RBRACKET;
        break;
    case ')':
        output->kind = TOK_RPAREN;
        break;
    case '/':
        output->kind = TOK_SOLIDUS;
        break;
    case '~':
        output->kind = TOK_TILDE;
        break;
    case '|':
        output->kind = TOK_VERTICAL_BAR;
        break;
    case ';':
        output->kind = TOK_SEMICOLON;
        break;
    default:
        output->kind = TOK_ERROR;
        bump(lexer);
        output->start = lexer->source + start;
        output->length = lexer->position - start;
        output->line = line;
        output->column = column;
        output->quote = GQL_QUOTE_NONE;
        output->is_float = 0;
        return;
    }
    bump(lexer);

done_op:
    output->start = lexer->source + start;
    output->length = lexer->position - start;
    output->line = line;
    output->column = column;
    output->quote = GQL_QUOTE_NONE;
    output->is_float = 0;
}

void gql_lexer_init(GqlLexer *lexer, const char *source) {
    *lexer = (GqlLexer){0};
    lexer->source = source ? source : "";
    lexer->length = strlen(lexer->source);
    lexer->line = 1;
    lexer->column = 1;
    lexer->token_count = 0;
    lexer->current_index = 0;
}

static void ensure_token(GqlLexer *lexer, int index) {
    while (lexer->token_count <= index) {
        lex_one(lexer, &lexer->tokens[lexer->token_count]);
        lexer->token_count++;
    }
}

const GqlToken *gql_lexer_peek(GqlLexer *lexer, int k) {
    ensure_token(lexer, lexer->current_index + k);
    return &lexer->tokens[lexer->current_index + k];
}

void gql_lexer_advance(GqlLexer *lexer) {
    ensure_token(lexer, lexer->current_index + 1);
    lexer->current_index++;
    if (lexer->current_index >= GQL_LEXER_COMPACTION_THRESHOLD) {
        int remaining_count = lexer->token_count - lexer->current_index;
        if (remaining_count > 0) {
            memmove(&lexer->tokens[0], &lexer->tokens[lexer->current_index],
                    (size_t)remaining_count * sizeof(GqlToken));
        }
        lexer->token_count = remaining_count;
        lexer->current_index = 0;
    }
}

const char *gql_token_kind_name(GqlTokenKind kind) {
    switch (kind) {
    case TOK_EOF:
        return "end of input";
    case TOK_ERROR:
        return "invalid token";
    case TOK_IDENT:
        return "identifier";
    case TOK_DELIM_IDENT:
        return "delimited identifier";
    case TOK_STR_LIT:
        return "string literal";
    case TOK_BYTE_STRING:
        return "byte string literal";
    case TOK_INT_LIT:
        return "number";
    case TOK_PARAM:
        return "parameter";
    case TOK_BOOLEAN_LITERAL:
        return "boolean literal";
    default:
        break;
    }
    if (kind >= TOK_KIND_COUNT) {
        return "unknown token";
    }

    {
        const size_t count = sizeof(keyword_table) / sizeof(keyword_table[0]);
        for (size_t i = 0; i < count; i++) {
            if (keyword_table[i].kind == kind) {
                return keyword_table[i].text;
            }
        }
    }
    static const char *punct[] = {"(", ")", "{", "}", "[",  "]",  ",", ".", ":", ";",
                                  "&", "*", "@", "$", "\"", "=",  "!", ">", "`", "<",
                                  "-", "%", "+", "?", "'",  "\\", "/", "~", "_", "|"};

    if (kind >= TOK_LPAREN && kind <= TOK_VERTICAL_BAR) {
        return punct[kind - TOK_LPAREN];
    }
    static const char *operator_spellings[] = {
        "|+|", "]->", "]~>", "||",  "::",  "$$", "..",  ">=", "<-", "<~",
        "<-[", "<~[", "<->", "<-/", "<~/", "<=", "-[",  "-/", "<>", "->",
        "]-",  "]~",  "=>",  "/-",  "/->", "/~", "/~>", "~[", "~>", "~/"};
    if (kind >= TOK_MULTISET_ALTERNATION && kind <= TOK_TILDE_SLASH) {
        return operator_spellings[kind - TOK_MULTISET_ALTERNATION];
    }
    return "token";
}
