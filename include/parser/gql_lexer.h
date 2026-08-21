#ifndef LINKLET_GQL_LEXER_H
#define LINKLET_GQL_LEXER_H

#include <stddef.h>

// clang-format off
#define GQL_KEYWORD_LIST(X) \
    X(ABS, "ABS", 0) \
    X(ACOS, "ACOS", 0) \
    X(ALL, "ALL", 0) \
    X(ALL_DIFFERENT, "ALL_DIFFERENT", 0) \
    X(AND, "AND", 0) \
    X(ANY, "ANY", 0) \
    X(ARRAY, "ARRAY", 0) \
    X(AS, "AS", 0) \
    X(ASC, "ASC", 0) \
    X(ASCENDING, "ASCENDING", 0) \
    X(ASIN, "ASIN", 0) \
    X(AT, "AT", 0) \
    X(ATAN, "ATAN", 0) \
    X(AVG, "AVG", 0) \
    X(BIG, "BIG", 0) \
    X(BIGINT, "BIGINT", 0) \
    X(BINARY, "BINARY", 0) \
    X(BOOL, "BOOL", 0) \
    X(BOOLEAN, "BOOLEAN", 0) \
    X(BOTH, "BOTH", 0) \
    X(BTRIM, "BTRIM", 0) \
    X(BY, "BY", 0) \
    X(BYTE_LENGTH, "BYTE_LENGTH", 0) \
    X(BYTES, "BYTES", 0) \
    X(CALL, "CALL", 0) \
    X(CARDINALITY, "CARDINALITY", 0) \
    X(CASE, "CASE", 0) \
    X(CAST, "CAST", 0) \
    X(CEIL, "CEIL", 0) \
    X(CEILING, "CEILING", 0) \
    X(CHAR, "CHAR", 0) \
    X(CHAR_LENGTH, "CHAR_LENGTH", 0) \
    X(CHARACTER_LENGTH, "CHARACTER_LENGTH", 0) \
    X(CHARACTERISTICS, "CHARACTERISTICS", 0) \
    X(CLOSE, "CLOSE", 0) \
    X(COALESCE, "COALESCE", 0) \
    X(COLLECT_LIST, "COLLECT_LIST", 0) \
    X(COMMIT, "COMMIT", 0) \
    X(COPY, "COPY", 0) \
    X(COS, "COS", 0) \
    X(COSH, "COSH", 0) \
    X(COT, "COT", 0) \
    X(COUNT, "COUNT", 0) \
    X(CREATE, "CREATE", 0) \
    X(CURRENT_DATE, "CURRENT_DATE", 0) \
    X(CURRENT_GRAPH, "CURRENT_GRAPH", 0) \
    X(CURRENT_PROPERTY_GRAPH, "CURRENT_PROPERTY_GRAPH", 0) \
    X(CURRENT_SCHEMA, "CURRENT_SCHEMA", 0) \
    X(CURRENT_TIME, "CURRENT_TIME", 0) \
    X(CURRENT_TIMESTAMP, "CURRENT_TIMESTAMP", 0) \
    X(DATE, "DATE", 0) \
    X(DATETIME, "DATETIME", 0) \
    X(DAY, "DAY", 0) \
    X(DEC, "DEC", 0) \
    X(DECIMAL, "DECIMAL", 0) \
    X(DEGREES, "DEGREES", 0) \
    X(DELETE, "DELETE", 0) \
    X(DESC, "DESC", 0) \
    X(DESCENDING, "DESCENDING", 0) \
    X(DETACH, "DETACH", 0) \
    X(DISTINCT, "DISTINCT", 0) \
    X(DOUBLE, "DOUBLE", 0) \
    X(DROP, "DROP", 0) \
    X(DURATION, "DURATION", 0) \
    X(DURATION_BETWEEN, "DURATION_BETWEEN", 0) \
    X(ELEMENT_ID, "ELEMENT_ID", 0) \
    X(ELSE, "ELSE", 0) \
    X(END, "END", 0) \
    X(EXCEPT, "EXCEPT", 0) \
    X(EXISTS, "EXISTS", 0) \
    X(EXP, "EXP", 0) \
    X(FILTER, "FILTER", 0) \
    X(FINISH, "FINISH", 0) \
    X(FLOAT, "FLOAT", 0) \
    X(FLOAT16, "FLOAT16", 0) \
    X(FLOAT32, "FLOAT32", 0) \
    X(FLOAT64, "FLOAT64", 0) \
    X(FLOAT128, "FLOAT128", 0) \
    X(FLOAT256, "FLOAT256", 0) \
    X(FLOOR, "FLOOR", 0) \
    X(FOR, "FOR", 0) \
    X(FROM, "FROM", 0) \
    X(GROUP, "GROUP", 0) \
    X(HAVING, "HAVING", 0) \
    X(HOME_GRAPH, "HOME_GRAPH", 0) \
    X(HOME_PROPERTY_GRAPH, "HOME_PROPERTY_GRAPH", 0) \
    X(HOME_SCHEMA, "HOME_SCHEMA", 0) \
    X(HOUR, "HOUR", 0) \
    X(IF, "IF", 0) \
    X(IMPLIES, "IMPLIES", 0) \
    X(IN, "IN", 0) \
    X(INSERT, "INSERT", 0) \
    X(INT, "INT", 0) \
    X(INTEGER, "INTEGER", 0) \
    X(INT8, "INT8", 0) \
    X(INTEGER8, "INTEGER8", 0) \
    X(INT16, "INT16", 0) \
    X(INTEGER16, "INTEGER16", 0) \
    X(INT32, "INT32", 0) \
    X(INTEGER32, "INTEGER32", 0) \
    X(INT64, "INT64", 0) \
    X(INTEGER64, "INTEGER64", 0) \
    X(INT128, "INT128", 0) \
    X(INTEGER128, "INTEGER128", 0) \
    X(INT256, "INT256", 0) \
    X(INTEGER256, "INTEGER256", 0) \
    X(INTERSECT, "INTERSECT", 0) \
    X(INTERVAL, "INTERVAL", 0) \
    X(IS, "IS", 0) \
    X(LEADING, "LEADING", 0) \
    X(LEFT, "LEFT", 0) \
    X(LET, "LET", 0) \
    X(LIKE, "LIKE", 0) \
    X(LIMIT, "LIMIT", 0) \
    X(LIST, "LIST", 0) \
    X(LN, "LN", 0) \
    X(LOCAL, "LOCAL", 0) \
    X(LOCAL_DATETIME, "LOCAL_DATETIME", 0) \
    X(LOCAL_TIME, "LOCAL_TIME", 0) \
    X(LOCAL_TIMESTAMP, "LOCAL_TIMESTAMP", 0) \
    X(LOG_KW, "LOG", 0) \
    X(LOG10, "LOG10", 0) \
    X(LOWER, "LOWER", 0) \
    X(LTRIM, "LTRIM", 0) \
    X(MATCH, "MATCH", 0) \
    X(MAX, "MAX", 0) \
    X(MIN, "MIN", 0) \
    X(MINUTE, "MINUTE", 0) \
    X(MOD, "MOD", 0) \
    X(MONTH, "MONTH", 0) \
    X(NEXT, "NEXT", 0) \
    X(NODETACH, "NODETACH", 0) \
    X(NORMALIZE, "NORMALIZE", 0) \
    X(NOT, "NOT", 0) \
    X(NOTHING, "NOTHING", 0) \
    X(NULL_KW, "NULL", 0) \
    X(NULLS, "NULLS", 0) \
    X(NULLIF, "NULLIF", 0) \
    X(OCTET_LENGTH, "OCTET_LENGTH", 0) \
    X(OF, "OF", 0) \
    X(OFFSET, "OFFSET", 0) \
    X(OPTIONAL, "OPTIONAL", 0) \
    X(OR, "OR", 0) \
    X(ORDER, "ORDER", 0) \
    X(OTHERWISE, "OTHERWISE", 0) \
    X(PARAMETER, "PARAMETER", 0) \
    X(PARAMETERS, "PARAMETERS", 0) \
    X(PATH, "PATH", 0) \
    X(PATH_LENGTH, "PATH_LENGTH", 0) \
    X(PATHS, "PATHS", 0) \
    X(PERCENTILE_CONT, "PERCENTILE_CONT", 0) \
    X(PERCENTILE_DISC, "PERCENTILE_DISC", 0) \
    X(POWER, "POWER", 0) \
    X(PRECISION, "PRECISION", 0) \
    X(PROPERTY_EXISTS, "PROPERTY_EXISTS", 0) \
    X(RADIANS, "RADIANS", 0) \
    X(REAL, "REAL", 0) \
    X(RECORD, "RECORD", 0) \
    X(REMOVE, "REMOVE", 0) \
    X(REPLACE, "REPLACE", 0) \
    X(RESET, "RESET", 0) \
    X(RETURN, "RETURN", 0) \
    X(RIGHT, "RIGHT", 0) \
    X(ROLLBACK, "ROLLBACK", 0) \
    X(RTRIM, "RTRIM", 0) \
    X(SAME, "SAME", 0) \
    X(SCHEMA, "SCHEMA", 0) \
    X(SECOND, "SECOND", 0) \
    X(SELECT, "SELECT", 0) \
    X(SESSION, "SESSION", 0) \
    X(SESSION_USER, "SESSION_USER", 0) \
    X(SET, "SET", 0) \
    X(SIGNED, "SIGNED", 0) \
    X(SIN, "SIN", 0) \
    X(SINH, "SINH", 0) \
    X(SIZE, "SIZE", 0) \
    X(SKIP_RESERVED_WORD, "SKIP", 0) \
    X(SMALL, "SMALL", 0) \
    X(SMALLINT, "SMALLINT", 0) \
    X(SQRT, "SQRT", 0) \
    X(START, "START", 0) \
    X(STDDEV_POP, "STDDEV_POP", 0) \
    X(STDDEV_SAMP, "STDDEV_SAMP", 0) \
    X(STRING, "STRING", 0) \
    X(SUM, "SUM", 0) \
    X(TAN, "TAN", 0) \
    X(TANH, "TANH", 0) \
    X(THEN, "THEN", 0) \
    X(TIME, "TIME", 0) \
    X(TIMESTAMP, "TIMESTAMP", 0) \
    X(TRAILING, "TRAILING", 0) \
    X(TRIM, "TRIM", 0) \
    X(TYPED, "TYPED", 0) \
    X(UBIGINT, "UBIGINT", 0) \
    X(UINT, "UINT", 0) \
    X(UINT8, "UINT8", 0) \
    X(UINT16, "UINT16", 0) \
    X(UINT32, "UINT32", 0) \
    X(UINT64, "UINT64", 0) \
    X(UINT128, "UINT128", 0) \
    X(UINT256, "UINT256", 0) \
    X(UNION, "UNION", 0) \
    X(UNSIGNED, "UNSIGNED", 0) \
    X(UPPER, "UPPER", 0) \
    X(USE, "USE", 0) \
    X(USMALLINT, "USMALLINT", 0) \
    X(VALUE, "VALUE", 0) \
    X(VARBINARY, "VARBINARY", 0) \
    X(VARCHAR, "VARCHAR", 0) \
    X(VARIABLE, "VARIABLE", 0) \
    X(WHEN, "WHEN", 0) \
    X(WHERE, "WHERE", 0) \
    X(WITH, "WITH", 0) \
    X(XOR, "XOR", 0) \
    X(YEAR, "YEAR", 0) \
    X(YIELD, "YIELD", 0) \
    X(ZONED, "ZONED", 0) \
    X(ZONED_DATETIME, "ZONED_DATETIME", 0) \
    X(ZONED_TIME, "ZONED_TIME", 0) \
    X(ABSTRACT, "ABSTRACT", 0) \
    X(AGGREGATE, "AGGREGATE", 0) \
    X(AGGREGATES, "AGGREGATES", 0) \
    X(ALTER, "ALTER", 0) \
    X(CATALOG, "CATALOG", 0) \
    X(CLEAR, "CLEAR", 0) \
    X(CLONE, "CLONE", 0) \
    X(CONSTRAINT, "CONSTRAINT", 0) \
    X(CURRENT_ROLE, "CURRENT_ROLE", 0) \
    X(CURRENT_USER, "CURRENT_USER", 0) \
    X(DATA, "DATA", 0) \
    X(DIRECTORY, "DIRECTORY", 0) \
    X(DRYRUN, "DRYRUN", 0) \
    X(EXACT, "EXACT", 0) \
    X(EXISTING, "EXISTING", 0) \
    X(FUNCTION, "FUNCTION", 0) \
    X(GQLSTATUS, "GQLSTATUS", 0) \
    X(GRANT, "GRANT", 0) \
    X(INSTANT, "INSTANT", 0) \
    X(INFINITY_KW, "INFINITY", 0) \
    X(NUMBER, "NUMBER", 0) \
    X(NUMERIC, "NUMERIC", 0) \
    X(ON, "ON", 0) \
    X(OPEN, "OPEN", 0) \
    X(PARTITION, "PARTITION", 0) \
    X(PROCEDURE, "PROCEDURE", 0) \
    X(PRODUCT, "PRODUCT", 0) \
    X(PROJECT, "PROJECT", 0) \
    X(QUERY, "QUERY", 0) \
    X(RECORDS, "RECORDS", 0) \
    X(REFERENCE, "REFERENCE", 0) \
    X(RENAME, "RENAME", 0) \
    X(REVOKE, "REVOKE", 0) \
    X(SUBSTRING, "SUBSTRING", 0) \
    X(SYSTEM_USER, "SYSTEM_USER", 0) \
    X(TEMPORAL, "TEMPORAL", 0) \
    X(UNIQUE, "UNIQUE", 0) \
    X(UNIT, "UNIT", 0) \
    X(VALUES, "VALUES", 0) \
    X(ACYCLIC, "ACYCLIC", 1) \
    X(BINDING, "BINDING", 1) \
    X(BINDINGS, "BINDINGS", 1) \
    X(CONNECTING, "CONNECTING", 1) \
    X(DESTINATION, "DESTINATION", 1) \
    X(DIFFERENT, "DIFFERENT", 1) \
    X(DIRECTED, "DIRECTED", 1) \
    X(EDGE, "EDGE", 1) \
    X(EDGES, "EDGES", 1) \
    X(ELEMENT, "ELEMENT", 1) \
    X(ELEMENTS, "ELEMENTS", 1) \
    X(FIRST, "FIRST", 1) \
    X(GRAPH, "GRAPH", 1) \
    X(GROUPS, "GROUPS", 1) \
    X(KEEP, "KEEP", 1) \
    X(LABEL, "LABEL", 1) \
    X(LABELED, "LABELED", 1) \
    X(LABELS, "LABELS", 1) \
    X(LAST, "LAST", 1) \
    X(NFC, "NFC", 1) \
    X(NFD, "NFD", 1) \
    X(NFKC, "NFKC", 1) \
    X(NFKD, "NFKD", 1) \
    X(NO, "NO", 1) \
    X(NODE, "NODE", 1) \
    X(NORMALIZED, "NORMALIZED", 1) \
    X(ONLY, "ONLY", 1) \
    X(ORDINALITY, "ORDINALITY", 1) \
    X(PROPERTY, "PROPERTY", 1) \
    X(READ, "READ", 1) \
    X(RELATIONSHIP, "RELATIONSHIP", 1) \
    X(RELATIONSHIPS, "RELATIONSHIPS", 1) \
    X(REPEATABLE, "REPEATABLE", 1) \
    X(SHORTEST, "SHORTEST", 1) \
    X(SIMPLE, "SIMPLE", 1) \
    X(SOURCE, "SOURCE", 1) \
    X(TABLE, "TABLE", 1) \
    X(TO, "TO", 1) \
    X(TRAIL, "TRAIL", 1) \
    X(TRANSACTION, "TRANSACTION", 1) \
    X(TYPE, "TYPE", 1) \
    X(UNDIRECTED, "UNDIRECTED", 1) \
    X(VERTEX, "VERTEX", 1) \
    X(WALK, "WALK", 1) \
    X(WITHOUT, "WITHOUT", 1) \
    X(WRITE, "WRITE", 1) \
    X(ZONE, "ZONE", 1)
// clang-format on

typedef enum GqlTokenKind {
    TOK_EOF = 0,
    TOK_ERROR,

    TOK_IDENT,
    TOK_DELIM_IDENT,
    TOK_STR_LIT,
    TOK_BYTE_STRING,
    TOK_INT_LIT,
    TOK_PARAM,
    TOK_BOOLEAN_LITERAL,

    TOK_LPAREN,
    TOK_RPAREN,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_LBRACKET,
    TOK_RBRACKET,
    TOK_COMMA,
    TOK_PERIOD,
    TOK_COLON,
    TOK_SEMICOLON,
    TOK_AMPERSAND,
    TOK_ASTERISK,
    TOK_COMMERCIAL_AT,
    TOK_DOLLAR_SIGN,
    TOK_DOUBLE_QUOTE,
    TOK_EQUALS,
    TOK_EXCLAMATION_MARK,
    TOK_RIGHT_ANGLE,
    TOK_GRAVE_ACCENT,
    TOK_LEFT_ANGLE,
    TOK_MINUS,
    TOK_PERCENT,
    TOK_PLUS,
    TOK_QUESTION_MARK,
    TOK_QUOTE,
    TOK_REVERSE_SOLIDUS,
    TOK_SOLIDUS,
    TOK_TILDE,
    TOK_UNDERSCORE,
    TOK_VERTICAL_BAR,

    TOK_MULTISET_ALTERNATION,
    TOK_BRACKET_RIGHT_ARROW,
    TOK_BRACKET_TILDE_RIGHT_ARROW,
    TOK_CONCATENATION,
    TOK_DOUBLE_COLON,
    TOK_DOUBLE_DOLLAR,
    TOK_DOUBLE_PERIOD,
    TOK_GE,
    TOK_LEFT_ARROW,
    TOK_LEFT_ARROW_TILDE,
    TOK_LEFT_ARROW_BRACKET,
    TOK_LEFT_ARROW_TILDE_BRACKET,
    TOK_LEFT_MINUS_RIGHT,
    TOK_LEFT_MINUS_SLASH,
    TOK_LEFT_TILDE_SLASH,
    TOK_LE,
    TOK_MINUS_LEFT_BRACKET,
    TOK_MINUS_SLASH,
    TOK_NE,
    TOK_RIGHT_ARROW,
    TOK_RIGHT_BRACKET_MINUS,
    TOK_RIGHT_BRACKET_TILDE,
    TOK_RIGHT_DOUBLE_ARROW,
    TOK_SLASH_MINUS,
    TOK_SLASH_MINUS_RIGHT,
    TOK_SLASH_TILDE,
    TOK_SLASH_TILDE_RIGHT,
    TOK_TILDE_LEFT_BRACKET,
    TOK_TILDE_RIGHT_ARROW,
    TOK_TILDE_SLASH,

#define GQL_LEXER_ENUM_ITEM(name, text, nonreserved) TOK_##name,
    GQL_KEYWORD_LIST(GQL_LEXER_ENUM_ITEM)
#undef GQL_LEXER_ENUM_ITEM

    TOK_KIND_COUNT
} GqlTokenKind;

typedef enum GqlQuoteKind {
    GQL_QUOTE_NONE = 0,
    GQL_QUOTE_SINGLE,
    GQL_QUOTE_DOUBLE,
    GQL_QUOTE_ACCENT
} GqlQuoteKind;

typedef struct GqlToken {
    GqlTokenKind kind;
    const char *start;
    size_t length;
    size_t line;
    size_t column;
    GqlQuoteKind quote;
    int is_float;
} GqlToken;

enum {
    GQL_LEXER_TOKEN_CAPACITY = 8,
    GQL_LEXER_COMPACTION_THRESHOLD = 4,
};

typedef struct GqlLexer {
    const char *source;
    size_t length;
    size_t position;
    size_t line;
    size_t column;

    GqlToken tokens[GQL_LEXER_TOKEN_CAPACITY];
    int token_count;
    int current_index;
} GqlLexer;

void gql_lexer_init(GqlLexer *lexer, const char *source);

const GqlToken *gql_lexer_peek(GqlLexer *lexer, int k);

void gql_lexer_advance(GqlLexer *lexer);

const char *gql_token_kind_name(GqlTokenKind kind);

int gql_token_nonreserved(GqlTokenKind kind);

char *gql_token_decoded_text(const GqlToken *token);

#endif
