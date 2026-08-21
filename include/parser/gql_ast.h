#ifndef LINKLET_GQL_AST_H
#define LINKLET_GQL_AST_H

#include <stdint.h>
#include <stdio.h>

typedef enum GqlNodeKind {
    GQL_NODE_INVALID = 0,

    GQL_PROGRAM,
    GQL_SESSION_SET,
    GQL_SESSION_RESET,
    GQL_SESSION_CLOSE,
    GQL_START_TRANSACTION,
    GQL_COMMIT,
    GQL_ROLLBACK,
    GQL_PROCEDURE_BODY,
    GQL_STATEMENT,
    GQL_NEXT_STATEMENT,

    GQL_CREATE_SCHEMA,
    GQL_DROP_SCHEMA,
    GQL_CREATE_GRAPH,
    GQL_DROP_GRAPH,
    GQL_CREATE_GRAPH_TYPE,
    GQL_DROP_GRAPH_TYPE,

    GQL_CALL_PROCEDURE,

    GQL_NAME,
    GQL_SCHEMA_REFERENCE,
    GQL_CATALOG_PARENT_REFERENCE,
    GQL_GRAPH_REFERENCE,
    GQL_GRAPH_TYPE_REFERENCE,
    GQL_BINDING_TABLE_REFERENCE,
    GQL_PROCEDURE_REFERENCE,
    GQL_HOME_GRAPH,
    GQL_CURRENT_GRAPH,

    GQL_OPEN_GRAPH_TYPE,
    GQL_GRAPH_TYPE_LIKE,
    GQL_NESTED_GRAPH_TYPE,
    GQL_GRAPH_SOURCE,
    GQL_ELEMENT_TYPE_LIST,
    GQL_NODE_TYPE,
    GQL_EDGE_TYPE,
    GQL_PROPERTY_TYPE_LIST,
    GQL_PROPERTY_TYPE,

    GQL_INSERT,
    GQL_SET_STATEMENT,
    GQL_SET_ITEM,
    GQL_REMOVE_STATEMENT,
    GQL_REMOVE_ITEM,
    GQL_DELETE_STATEMENT,

    GQL_COMPOSITE_QUERY,
    GQL_SET_OPERATION,
    GQL_MATCH,
    GQL_OPTIONAL_MATCH,
    GQL_FILTER,
    GQL_LET,
    GQL_FOR,
    GQL_ORDER_BY_PAGE,
    GQL_RETURN,
    GQL_FINISH,
    GQL_SELECT,
    GQL_USE_GRAPH,

    GQL_GRAPH_PATTERN,
    GQL_PATH_PATTERN_LIST,
    GQL_PATH_PATTERN,
    GQL_PATH_EXPRESSION,
    GQL_NODE_PATTERN,
    GQL_EDGE_PATTERN,
    GQL_ELEMENT_FILLER,
    GQL_LABEL_EXPRESSION,
    GQL_QUANTIFIER,
    GQL_INSERT_GRAPH_PATTERN,
    GQL_INSERT_PATH_PATTERN,
    GQL_INSERT_NODE_PATTERN,
    GQL_INSERT_EDGE_PATTERN,

    GQL_WHERE,
    GQL_YIELD,
    GQL_GROUP_BY,
    GQL_ORDER_BY,
    GQL_SORT_SPEC,
    GQL_LIMIT,
    GQL_OFFSET,
    GQL_HAVING,
    GQL_AT_SCHEMA,
    GQL_KEEP,

    GQL_VALUE_EXPR,
    GQL_PREDICATE,
    GQL_LITERAL,
    GQL_PARAM,
    GQL_BINDING_VAR,
    GQL_PROPERTY_REFERENCE,
    GQL_FUNCTION_CALL,
    GQL_AGGREGATE,
    GQL_CASE,
    GQL_CAST,
    GQL_LIST_LITERAL,
    GQL_RECORD_LITERAL,
    GQL_FIELD,
    GQL_PATH_CONSTRUCTOR,
    GQL_GRAPH_EXPR,
    GQL_BINDING_TABLE_EXPR,

    GQL_TYPE,
    GQL_TYPED,

    GQL_PROPERTY_SPEC,
    GQL_VARIABLE_DECL,

    GQL_NODE_KIND_COUNT
} GqlNodeKind;

typedef enum GqlValueOperation {
    GQL_OP_NONE = 0,
    GQL_OP_OR,
    GQL_OP_XOR,
    GQL_OP_AND,
    GQL_OP_NOT,
    GQL_OP_EQ,
    GQL_OP_NE,
    GQL_OP_LT,
    GQL_OP_GT,
    GQL_OP_LE,
    GQL_OP_GE,
    GQL_OP_CONCAT,
    GQL_OP_ADD,
    GQL_OP_SUB,
    GQL_OP_MUL,
    GQL_OP_DIV,
    GQL_OP_NEG,
    GQL_OP_POS,
    GQL_OP_IS_TRUE,
    GQL_OP_IS_NOT_TRUE,
    GQL_OP_IS_FALSE,
    GQL_OP_IS_NOT_FALSE,
    GQL_OP_IS_UNKNOWN,
    GQL_OP_IS_NOT_UNKNOWN,
    GQL_OP_IS_NORMALIZED,
    GQL_OP_IS_NOT_NORMALIZED
} GqlValueOperation;

typedef enum GqlLiteralKind {
    GQL_LIT_NONE = 0,
    GQL_LIT_INTEGER,
    GQL_LIT_FLOAT,
    GQL_LIT_STRING,
    GQL_LIT_BYTE_STRING,
    GQL_LIT_BOOLEAN,
    GQL_LIT_NULL,
    GQL_LIT_DATE,
    GQL_LIT_TIME,
    GQL_LIT_DATETIME,
    GQL_LIT_TIMESTAMP,
    GQL_LIT_DURATION
} GqlLiteralKind;

typedef enum GqlPredicateKind {
    GQL_PRED_NONE = 0,
    GQL_PRED_EXISTS,
    GQL_PRED_IS_NULL,
    GQL_PRED_IS_NOT_NULL,
    GQL_PRED_IS_TYPED,
    GQL_PRED_IS_NOT_TYPED,
    GQL_PRED_IS_DIRECTED,
    GQL_PRED_IS_NOT_DIRECTED,
    GQL_PRED_IS_LABELED,
    GQL_PRED_IS_NOT_LABELED,
    GQL_PRED_SOURCE_OF,
    GQL_PRED_NOT_SOURCE_OF,
    GQL_PRED_DESTINATION_OF,
    GQL_PRED_NOT_DESTINATION_OF,
    GQL_PRED_ALL_DIFFERENT,
    GQL_PRED_SAME,
    GQL_PRED_PROPERTY_EXISTS
} GqlPredicateKind;

typedef enum GqlSetOperationKind {
    GQL_SET_OPERATION_NONE = 0,
    GQL_SET_OPERATION_UNION,
    GQL_SET_OPERATION_EXCEPT,
    GQL_SET_OPERATION_INTERSECT,
    GQL_SET_OPERATION_OTHERWISE
} GqlSetOperationKind;

typedef enum GqlDirection {
    GQL_DIR_NONE = 0,
    GQL_DIR_RIGHT,
    GQL_DIR_LEFT,
    GQL_DIR_UNDIRECTED,
    GQL_DIR_LEFT_OR_UNDIRECTED,
    GQL_DIR_UNDIRECTED_OR_RIGHT,
    GQL_DIR_LEFT_OR_RIGHT,
    GQL_DIR_ANY
} GqlDirection;

typedef enum GqlQuantifierKind {
    GQL_QUANT_NONE = 0,
    GQL_QUANT_STAR,
    GQL_QUANT_PLUS,
    GQL_QUANT_QUESTION,
    GQL_QUANT_FIXED,
    GQL_QUANT_GENERAL
} GqlQuantifierKind;

typedef enum GqlFunctionKind {
    GQL_FN_NONE = 0,
    GQL_FN_ABS,
    GQL_FN_UPPER,
    GQL_FN_LOWER,
    GQL_FN_TRIM,
    GQL_FN_BTRIM,
    GQL_FN_LTRIM,
    GQL_FN_RTRIM,
    GQL_FN_NORMALIZE,
    GQL_FN_LEFT,
    GQL_FN_RIGHT,
    GQL_FN_CHAR_LENGTH,
    GQL_FN_CHARACTER_LENGTH,
    GQL_FN_BYTE_LENGTH,
    GQL_FN_OCTET_LENGTH,
    GQL_FN_PATH_LENGTH,
    GQL_FN_CARDINALITY,
    GQL_FN_SIZE,
    GQL_FN_MOD,
    GQL_FN_SIN,
    GQL_FN_COS,
    GQL_FN_TAN,
    GQL_FN_COT,
    GQL_FN_SINH,
    GQL_FN_COSH,
    GQL_FN_TANH,
    GQL_FN_ASIN,
    GQL_FN_ACOS,
    GQL_FN_ATAN,
    GQL_FN_DEGREES,
    GQL_FN_RADIANS,
    GQL_FN_LOG,
    GQL_FN_LOG10,
    GQL_FN_LN,
    GQL_FN_EXP,
    GQL_FN_POWER,
    GQL_FN_SQRT,
    GQL_FN_FLOOR,
    GQL_FN_CEIL,
    GQL_FN_CEILING,
    GQL_FN_ELEMENTS,
    GQL_FN_ELEMENT_ID,
    GQL_FN_DATE,
    GQL_FN_TIME,
    GQL_FN_LOCAL_TIME,
    GQL_FN_DATETIME,
    GQL_FN_LOCAL_DATETIME,
    GQL_FN_DURATION,
    GQL_FN_DURATION_BETWEEN,
    GQL_FN_CURRENT_DATE,
    GQL_FN_CURRENT_TIME,
    GQL_FN_CURRENT_TIMESTAMP,
    GQL_FN_LOCAL_TIMESTAMP,
    GQL_FN_ZONED_TIME,
    GQL_FN_ZONED_DATETIME
} GqlFunctionKind;

typedef enum GqlAggregateKind {
    GQL_AGG_NONE = 0,
    GQL_AGG_COUNT,
    GQL_AGG_AVG,
    GQL_AGG_MAX,
    GQL_AGG_MIN,
    GQL_AGG_SUM,
    GQL_AGG_COLLECT_LIST,
    GQL_AGG_STDDEV_SAMP,
    GQL_AGG_STDDEV_POP,
    GQL_AGG_PERCENTILE_CONT,
    GQL_AGG_PERCENTILE_DISC
} GqlAggregateKind;

typedef enum GqlTypeCategory {
    GQL_TYPE_NONE = 0,
    GQL_TYPE_PREDEFINED,
    GQL_TYPE_LIST,
    GQL_TYPE_RECORD,
    GQL_TYPE_PATH,
    GQL_TYPE_OPEN_UNION,
    GQL_TYPE_PROPERTY_VALUE,
    GQL_TYPE_CLOSED_UNION,
    GQL_TYPE_GRAPH_REF,
    GQL_TYPE_BINDING_TABLE_REF,
    GQL_TYPE_NODE_REF,
    GQL_TYPE_EDGE_REF,
    GQL_TYPE_NOTHING
} GqlTypeCategory;

typedef enum GqlSetQuantifier {
    GQL_SQ_NONE = 0,
    GQL_SQ_ALL,
    GQL_SQ_DISTINCT
} GqlSetQuantifier;

typedef enum GqlOrdering {
    GQL_ORDER_NONE = 0,
    GQL_ORDER_ASC,
    GQL_ORDER_DESC
} GqlOrdering;

typedef enum GqlBooleanValue {
    GQL_BOOLEAN_TRUE = 0,
    GQL_BOOLEAN_FALSE,
    GQL_BOOLEAN_UNKNOWN,
} GqlBooleanValue;

typedef enum GqlLabelExpressionKind {
    GQL_LABEL_NEGATION = 1,
    GQL_LABEL_CONJUNCTION,
    GQL_LABEL_DISJUNCTION,
    GQL_LABEL_WILDCARD,
    GQL_LABEL_NAME,
} GqlLabelExpressionKind;

typedef enum GqlSetItemKind {
    GQL_SET_PROPERTY = 1,
    GQL_SET_PROPERTIES,
    GQL_SET_LABEL,
} GqlSetItemKind;

typedef enum GqlRemoveItemKind {
    GQL_REMOVE_PROPERTY = 1,
    GQL_REMOVE_LABEL,
} GqlRemoveItemKind;

typedef enum GqlDeleteKind {
    GQL_DELETE_NORMAL = 0,
    GQL_DELETE_DETACH,
    GQL_DELETE_NODETACH,
} GqlDeleteKind;

typedef enum GqlNullOrdering {
    GQL_NULL_ORDER_NONE = 0,
    GQL_NULL_ORDER_FIRST,
    GQL_NULL_ORDER_LAST,
} GqlNullOrdering;

typedef enum GqlOffsetKind {
    GQL_OFFSET_OFFSET = 1,
    GQL_OFFSET_SKIP,
} GqlOffsetKind;

typedef enum GqlCatalogModificationOption {
    GQL_CATALOG_MODIFICATION_NONE = 0,
    GQL_CATALOG_MODIFICATION_CONDITIONAL,
    GQL_CATALOG_MODIFICATION_OR_REPLACE,
} GqlCatalogModificationOption;

typedef enum GqlPathExpressionKind {
    GQL_PATH_EXPRESSION_NONE = 0,
    GQL_PATH_MULTISET_ALTERNATION,
    GQL_PATH_ALTERNATION,
    GQL_PATH_PARENTHESIZED,
    GQL_PATH_SIMPLIFIED,
} GqlPathExpressionKind;

typedef enum GqlCaseKind {
    GQL_CASE_STANDARD = 0,
    GQL_CASE_NULLIF,
    GQL_CASE_COALESCE,
} GqlCaseKind;

typedef enum GqlNullability {
    GQL_NULLABLE = 0,
    GQL_NOT_NULL,
} GqlNullability;

typedef struct GqlNode {
    GqlNodeKind kind;
    int subkind;
    int integer_value;
    uint64_t unsigned_integer_value;
    double floating_value;
    char *text;
    struct GqlNode **children;
    size_t child_count;
    size_t child_capacity;
} GqlNode;

GqlNode *gql_node_new(GqlNodeKind kind);
void gql_node_add_child(GqlNode *node, GqlNode *child);

void gql_node_set_text(GqlNode *node, const char *text);

void gql_node_take_text(GqlNode *node, char *text);

GqlNode *gql_node_child(const GqlNode *node, size_t index);

void gql_node_free(GqlNode *node);

const char *gql_node_kind_name(GqlNodeKind kind);

void gql_ast_dump(const GqlNode *root, FILE *out);

#endif
