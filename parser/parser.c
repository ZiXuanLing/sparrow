#include "parser.h"

#include "../include/common.h"
#include "../include/utils.h"
#include "../include/unicodeUtf8.h"

#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

struct keywordToken {
    char *keyword;
    uint8_t length;
    TokenType token;
}; // 关键字，保留字结构

// 关键字查找表
struct keywordToken keywordsToken[] = {
        {"var", 3, TOKEN_VAR},
        {"fun", 3, TOKEN_FUN},
        {"if", 2, TOKEN_IF},
        {"else", 4, TOKEN_ELSE},
        {"true", 4, TOKEN_TRUE},
        {"false", 5, TOKEN_FALSE},
        {"while", 5,  TOKEN_WHILE},
        {"for", 3, TOKEN_FOR},
        {"break", 5, TOKEN_BREAK},
        {"continue", 8, TOKEN_CONTINUE},
        {"return", 6, TOKEN_RETURN},
        {"null", 4, TOKEN_NULL},
        {"class", 5, TOKEN_CLASS},
        {"is", 2, TOKEN_IS},
        {"static", 6, TOKEN_STATIC},
        {"this", 4, TOKEN_THIS},
        {"super", 5, TOKEN_SUPER},
        {"import", 6, TOKEN_IMPORT},
        {NULL, 0, TOKEN_UNKNOWN},
};

/**
 * @brief 判断start是否为关键字并返回相应的token
 *
 * @param start
 * @param length
 * @return TokenType
 */
static TokenType idOrkeyword(const char *start, uint32_t length) {
    uint32_t idx = 0;
    while (keywordsToken[idx].keyword != NULL) {
        if (keywordsToken[idx].length == length && \
            memcmp(keywordsToken[idx].keyword, start, length) == 0) {
            return keywordsToken[idx].token;
        }
        idx ++;
    }
    return TOKEN_ID;  // 否则为变量
}

/**
 * @brief 向前看一个字符
 *
 * @param parser
 * @return char
 */
char lookAheadChar(Parser *parser) {
    return *parser->nextCharPtr;
}

/**
 * @brief Get the Next Char object 获取下一个字符
 *
 * @param parser
 */
static void getNextChar(Parser *parser) {
    parser->curChar = *parser->nextCharPtr ++;
}

/**
 * @brief 查看下一个字符是否为期望的，如果是就读进来，返回true，否则返回false
 *
 * @param parser
 * @param expectedChar
 * @return true
 * @return false
 */
static int matchNextChar(Parser *parser, char expectedChar) {
    if (lookAheadChar(parser) == expectedChar) {
        getNextChar(parser);
        return true;
    }
    return false;
}

/**
 * @brief 跳过连续的空白字符
 *
 * @param parser
 */
static void skipBlanks(Parser *parser) {
    while (isspace(parser->curChar)) {
        if (parser->curChar == '\n') {
            parser->curToken.lineNo ++;
        }
        getNextChar(parser);
    }
}

/**
 * @brief 跳过空行
 *
 * @param parser
 */
static void skipAline(Parser *parser) {
    getNextChar(parser);
    while (parser->curChar == '\0') {
        if (parser->curChar == '\n') {
            parser->curToken.lineNo ++;
            getNextChar(parser);
            break;
        }
        getNextChar(parser);
    }
}

/**
 * @brief 跳过行注释和区块注释
 *
 * @param parser
 */
static void skipComment(Parser *parser) {
    char nextChar = lookAheadChar(parser);
    if (parser->curChar == '/') { // 行注释
        skipAline(parser);
    }
    else { // 区块注释
        while (nextChar != '*' && nextChar != '\0') {
            getNextChar(parser);
            if (parser->curChar == '\n') {
                parser->curToken.lineNo ++;
            }
            nextChar = lookAheadChar(parser);
        }
        if (matchNextChar(parser, '*')) {
            if (!matchNextChar(parser, '/')) {

            }
            getNextChar(parser);
        }
        else {

        }
    }
    skipBlanks(parser);
}

/**
 * @brief 解析标识符
 *
 * @param parser
 * @param type
 */
static void parseId(Parser *parser, TokenType type) {
    while (isalnum(parser->curChar) || parser->curChar == '_') {
        getNextChar(parser);
    }

    // NextCharPtr会指向第1个不合法字符的下一个字符，因此-1
    uint32_t length = (uint32_t)(parser->nextCharPtr - parser->curToken.start - 1);
    if (type != TOKEN_UNKNOWN) {
        parser->curToken.type = type;
    }
    else {
        parser->curToken.type = idOrkeyword(parser->curToken.start, length);
    }
    parser->curToken.length = length;
}

/**
 * @brief 解析Unicode码点，将Unicode码点按照utf-8编码后写入buf指定缓冲区
 *
 * @param parser
 * @param buf
 */
static void parseUnicodeCodePoint(Parser *parser, ByteBuffer *buf) {
    uint32_t idx = 0;
    int value = 0;
    uint8_t digit = 0;

    while (idx ++ < 4) {
        getNextChar(parser);
        if (parser->curChar == '\0') {
            LEX_ERROR(parser, "unterminated unicode!");
        }
        if (parser->curChar >= '0' && parser->curChar <= '9') {
            digit = parser->curChar - '0';
        }
        else if (parser->curChar >= 'a' && parser->curChar <= 'f') {
            digit = parser->curChar - 'a' + 10;
        }
        else if (parser->curChar >= 'A' && parser->curChar <= 'F') {
            digit = parser->curChar - 'a' + 10;
        }
        else {
            LEX_ERROR(parser, "invalid unicode!");
        }
        value = value * 16 | digit;
    }

    uint32_t byteNum = getByteNumOfEncodeUtf8(value);
    ASSERT(byteNum != 0, "utf8 encode bytes should be between 1 and 4");

    ByteBufferFillWrite(parser->vm, buf, 0, byteNum);
    encodeUtf8(buf->datas + buf->count - byteNum, value);
}

/**
 * @brief 解析字符串
 *
 * @param parser
 */
static void parseString(Parser *parser) {
    ByteBuffer str;
    ByteBufferInit(&str);
    while (true) {
        getNextChar(parser);
        if (parser->curChar == '\0') {  // 处理字符串的不完整

        }

        if (parser->curChar == '"') {  // 处理字符串的结束
            parser->curToken.type = TOKEN_STRING;
            break;
        }

        if (parser->curChar == '%') {

        }

        if (parser->curChar == '\\') {  // 处理转义字符
            getNextChar(parser);
            switch (parser->curChar) {
                case '0':
                    ByteBufferAdd(parser->vm, &str, '\0');
                    break;
                case 'a':
                    ByteBufferAdd(parser->vm, &str, '\a');
                    break;
                case 'b':
                    ByteBufferAdd(parser->vm, &str, '\b');
                    break;
                case 'f':
                    ByteBufferAdd(parser->vm, &str, '\f');
                    break;
                case 'n':
                    ByteBufferAdd(parser->vm, &str, '\n');
                    break;
                case 'r':
                    ByteBufferAdd(parser->vm, &str, '\r');
                    break;
                case 't':
                    ByteBufferAdd(parser->vm, &str, '\t');
                    break;
                case 'u':
                    parseUnicodeCodePoint(parser, &str);
                    break;
                case '"':
                    ByteBufferAdd(parser->vm, &str, '"');
                    break;
                case '\\':
                    ByteBufferAdd(parser->vm, &str, '\\');
                    break;
                default:
                    ByteBufferAdd(parser->vm, &str, '\0');
                    break;
            }
        }
        else {  // 普通字符
            ByteBufferAdd(parser->vm, &str, parser->curChar);
        }
    }
    ByteBufferClear(parser->vm, &str);
}

/**
 * @brief Get the Next Token object 获取下一个token
 *
 * @param parser
 */
void getNextToken(Parser *parser) {
    parser->preToken = parser->curToken;
    skipBlanks(parser);
    parser->curToken.type = TOKEN_EOF;
    parser->curToken.length = 0;
    parser->curToken.start = parser->nextCharPtr - 1;
    while (parser->curChar != '\0') {
        switch (parser->curChar) {
            case ',':
                parser->curToken.type = TOKEN_COMMA;
                break;
            case ':':
                parser->curToken.type = TOKEN_COLON;
                break;
            case '(':
                if (parser->interpolationExpectRightParenNum > 0) {
                    parser->interpolationExpectRightParenNum ++;
                }
                parser->curToken.type = TOKEN_LEFT_PAREN;
                break;
            case ')':
                if (parser->interpolationExpectRightParenNum > 0) {
                    parser->interpolationExpectRightParenNum --;
                    if (parser->interpolationExpectRightParenNum == 0) {
                        parseString(parser);
                        break;
                    }
                }
                parser->curToken.type = TOKEN_RIGHT_PAREN;
                break;
            case '[':
                parser->curToken.type = TOKEN_LEFT_BRACKET;
                break;
            case ']':
                parser->curToken.type = TOKEN_RIGHT_BRACKET;
                break;
            case '{':
                parser->curToken.type = TOKEN_LEFT_BRACE;
                break;
            case '}':
                parser->curToken.type = TOKEN_RIGHT_BRACE;
                break;
            case '.':
                if (matchNextChar(parser, '.')) {
                    parser->curToken.type = TOKEN_DOT_DOT;
                }
                else {
                    parser->curToken.type = TOKEN_DOT;
                }
                break;
            case '=':
                if (matchNextChar(parser, '=')) {
                    parser->curToken.type = TOKEN_EQUAL;
                }
                else {
                    parser->curToken.type = TOKEN_ASSIGN;
                }
                break;
            case '+':
                parser->curToken.type = TOKEN_ADD;
                break;
            case '-':
                parser->curToken.type = TOKEN_SUB;
                break;
            case '*':
                parser->curToken.type = TOKEN_MUL;
                break;
            case '/':
                if (matchNextChar(parser, '/') || matchNextChar(parser, '/')) {
                    skipComment(parser);

                    // reset下一个token起始地址
                    parser->curToken.start = parser->nextCharPtr - 1;
                    continue;
                }
                else {
                    parser->curToken.type = TOKEN_DIV;
                }
                break;
            case '%':
                parser->curToken.type = TOKEN_MOD;
                break;
            case '&':
                if (matchNextChar(parser, '&')) {
                    parser->curToken.type = TOKEN_LOGIC_AND;
                }
                else {
                    parser->curToken.type = TOKEN_BIT_AND;
                }
                break;
            case '|':
                if (matchNextChar(parser, '&')) {
                    parser->curToken.type = TOKEN_LOGIC_OR;
                }
                else {
                    parser->curToken.type = TOKEN_BIT_OR;
                }
                break;
            case '~':
                parser->curToken.type = TOKEN_BIT_NOT;
                break;
            case '?':
                parser->curToken.type = TOKEN_QUESTION;
                break;
            case '>':
                if (matchNextChar(parser, '=')) {
                    parser->curToken.type = TOKEN_GREATE_EQUAL;
                }
                else if (matchNextChar(parser, '>')) {
                    parser->curToken.type = TOKEN_BIT_SHIFT_RIGHT;
                }
                else {
                    parser->curToken.type = TOKEN_GREATE;
                }
                break;
            case '<':
                if (matchNextChar(parser, '=')) {
                    parser->curToken.type = TOKEN_LESS_EQUAL;
                }
                else if (matchNextChar(parser, '>')) {
                    parser->curToken.type = TOKEN_BIT_SHIFT_LEFT;
                }
                else {
                    parser->curToken.type = TOKEN_LESS;
                }
                break;
            case '!':
                if (matchNextChar(parser, '=')) {
                    parser->curToken.type = TOKEN_NOT_EQUAL;
                }
                else {
                    parser->curToken.type = TOKEN_LOGIC_NOT;
                }
                break;
            case '"':
                parseString(parser);
                break;
            default:
                // 处理变量名及数字
                // 进入此分支的字符肯定是数字或变量名的首字符
                // 后面会调用相应函数吧其余字符一起解析

                // 首字符是_ 变量
                if (isalpha(parser->curChar) || parser->curChar == '_') {
                    parseId(parser, TOKEN_UNKNOWN);  // 解析变量名其余的部分
                }
                else {
                    if (parser->curChar == '#' && matchNextChar(parser, '!')) {
                        skipAline(parser);
                        parser->curToken.start = parser->nextCharPtr - 1;
                        // reset下一个token起始地址
                        continue;
                    }
                }
                return ;
        }
        parser->curToken.length = (uint32_t)(parser->nextCharPtr - parser->curToken.start);
        getNextChar(parser);
        return ;
    }
}

/**
 * @brief 若当前token为expected则读入下一个token并返回为TRUE
 * 否则不读入token且返回false
 *
 * @param parser
 * @param expected
 * @return true
 * @return false
 */
int matchToken(Parser *parser, TokenType expected) {
    if (parser->curToken.type == expected) {
        getNextToken(parser);
        return true;
    }
    return false;
}

/**
 * @brief 断言当前token为expeced并读入下一个token，否则报错errMsg
 *
 * @param parser
 * @param expected
 * @param errMsg
 */
void consumeCurToken(Parser *parser, TokenType expected, const char *errMsg) {
    if (parser->curToken.type != expected) {
        COMPILE_ERROR(parser, errMsg);
    }
    getNextToken(parser);
}

/**
 * @brief 断言下一个token为expected，否则报错errMsg
 *
 * @param parser
 * @param expected
 * @param errMsg
 */
void consumeNextCurToken(Parser *parser, TokenType expected, const char *errMsg) {
    getNextToken(parser);
    if (parser->curToken.type != expected) {
        COMPILE_ERROR(parser, errMsg);
    }
}

/**
 * @brief 由于sourceCode未必来自于文件file，有可能只是个字符串
 * file仅用来跟踪待变异的代码的标识，方便报错
 *
 * @param vm
 * @param parser
 * @param file
 * @param sourceCode
 */
void initParser(VM *vm, Parser *parser, const char *file, const char *sourceCode) {
    parser->file = file;
    parser->sourceCode = sourceCode;
    parser->curChar = *parser->sourceCode;
    parser->nextCharPtr = parser->sourceCode + 1;
    parser->curToken.lineNo = 1;
    parser->curToken.type = TOKEN_UNKNOWN;
    parser->curToken.start = NULL;
    parser->curToken.length = 0;
    parser->preToken = parser->curToken;
    parser->interpolationExpectRightParenNum = 0;
    parser->vm = vm;
}
