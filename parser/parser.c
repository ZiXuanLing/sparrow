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
        if (keywordsToken[idx].length == length && memcpy(keywordsToken[idx].keyword, start, length) == 0) {
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
static bool matchNextChar(Parser *parser, char expectedChar) {
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

}

/**
 * @brief 解析字符串
 * 
 * @param parser 
 */
static void parseString(Parser *parser) {

}

