#ifndef __SPARROW_PARSER_H__

#define __SPARROW_PARSER_H__

#include "../include/common.h"
#include "../vm/vm.h"
#include "../include/utils.h"
#include "../object/meta_obj.h"
#include "../compiler/compiler.h"

typedef enum {
    TOKEN_UNKNOWN,
    // 数据类型
    TOKEN_NUM, // 数字
    TOKEN_STRING, // 字符串
    TOKEN_ID,  // 变量名
    TOKEN_INTERPOLATION, // 内嵌表达式

    // 关键字
    TOKEN_VAR, // var
    TOKEN_FUN, // fun
    TOKEN_IF, // if
    TOKEN_ELSE, // else
    TOKEN_TRUE, // true
    TOKEN_FALSE, // false
    TOKEN_WHILE, // while
    TOKEN_FOR, // for
    TOKEN_BREAK, // break
    TOKEN_CONTINUE, // continue
    TOKEN_RETURN, // return
    TOKEN_NULL,  // null

    // 以下是关于类和模块导入的token
    TOKEN_CLASS, // class
    TOKEN_THIS, // this
    TOKEN_STATIC, // static
    TOKEN_IS, // is
    TOKEN_SUPER, // super
    TOKEN_IMPORT, // import

    // 分隔符
    TOKEN_COMMA, // ,
    TOKEN_COLON, // :
    TOKEN_LEFT_PAREN, // (
    TOKEN_RIGHT_PAREN, // )
    TOKEN_LEFT_BRACKET, // [
    TOKEN_RIGHT_BRACKET, // ]
    TOKEN_LEFT_BRACE, // {
    TOKEN_RIGHT_BRACE, // }
    TOKEN_DOT, // .
    TOKEN_DOT_DOT,  // ..

    // 简单双目运算符
    TOKEN_ADD, // +
    TOKEN_SUB, // -
    TOKEN_MUL, // *
    TOKEN_DIV, // /
    TOKEN_MOD, // %

    // 赋值运算符
    TOKEN_ASSIGN, // =

    // 位运算符
    TOKEN_BIT_AND, // &
    TOKEN_BIT_OR, // |
    TOKEN_BIT_NOT, // ~
    TOKEN_BIT_SHIFT_RIGHT, // >>
    TOKEN_BIT_SHIFT_LEFT, // <<

    // 逻辑运算符
    TOKEN_LOGIC_AND, // &&
    TOKEN_LOGIC_OR, // ||
    TOKEN_LOGIC_NOT, // !

    // 关系运算符
    TOKEN_EQUAL, // ==
    TOKEN_NOT_EQUAL, // !=
    TOKEN_GREATE, // >
    TOKEN_GREATE_EQUAL, // >=
    TOKEN_LESS, // <
    TOKEN_LESS_EQUAL, // <=

    TOKEN_QUESTION, // ?

    // 文件结束标记，仅词法分析时使用
    TOKEN_EOF // EOF
} TokenType;

typedef struct {  // 词法分析器合成周边信息的复合结构
    TokenType type;
    const char *start;  // 源码串中单词的起始地址
    uint32_t length;  // 该单词的长度
    uint32_t lineNo;  // 单词在源码中的行号
    Value value;
} Token;

struct parser {  // 词法分析器结构
    const char *file;  // 源码文件名
    const char *sourceCode;  // 源码
    const char *nextCharPtr; // 执行源码中下一个字符
    char curChar; // 识别到的当前字符
    Token curToken; // 当前的token
    Token preToken;  // 前一个token
    ObjModule *curModule; // 当前正在编译的模块
    CompileUnit *curCompileUnit; // 当前编译单元

    // 处于内嵌表达式中，期望的右括号数量
    // 用于跟踪小括号对的嵌套
    int interpolationExpectRightParenNum;
    struct parser *parent; // 指向父parser
    struct vm* vm;  // parser隶属于哪一个vm，在词法分析过程中需要指定vm
};


int matchToken(Parser *parser, TokenType expected);
static TokenType idOrkeyword(const char *start, uint32_t length);
static int matchNextChar(Parser *parser, char expectedChar);
static void getNextChar(Parser *parser);
static void skipBlanks(Parser *parser);
static void skipAline(Parser *parser);
static void skipComment(Parser *parser);
static void parseId(Parser *parser, TokenType type);
static void parseUnicodeCodePoint(Parser *parser, ByteBuffer *buf);
static void parseString(Parser *parser);
char lookAheadChar(Parser *parser);
void getNextToken(Parser *parser);
// void getNextToken(Parser *parser);
void consumeCurToken(Parser *parser, TokenType expected, const char *errMsg);
void consumeNextCurToken(Parser *parser, TokenType expected, const char *errMsg);
void initParser(VM *vm, Parser *parser, const char *file, const char *sourceCode, ObjModule *objModule);
static void parseOctNum(Parser *parser);
static void parseDecNum(Parser *parser);
static void parseNum(Parser *parser);
static void parseHexNum(Parser *parser);

#endif // !__SPARROW_PARSER_H__