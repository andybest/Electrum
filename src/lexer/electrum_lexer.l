
%top {
#include <LexerDefs.h>
#include <iostream>
%}

%option freespace
%option unicode
%option lexer=Lexer
%option namespace=electrum
%option header-file

%class {
public:
    std::shared_ptr<std::string> filename;

    std::vector<Token> getTokens() {
        std::vector<Token> tokens;

        int r = 0;
        r = yylex();

        while(r != kTokenTypeEOF) {
            if(r != kTokenTypeNoToken) {
                Token t;
                t.line = lineno();
                t.column = columno();
                t.text = str();
                t.type = static_cast<TokenType>(r);
                t.filename = filename;
                tokens.push_back(t);
            }
            r = yylex();
        }

        return tokens;
    }
%}

%o flex freespace dotall unicode

LINE_COMMENT        ;([^\n]|\\\n)*\n
L_PAREN             "("
R_PAREN             ")"

QUOTE               "'"
QUASIQUOTE          "`"
UNQUOTE             ","
SPLICE_UNQUOTE      ",@"

/* Numbers */

DIGIT               [0-9]
DIGIT1              [1-9]
DIGITS              {DIGIT}+
INTEGER             -? {DIGITS} | -? {DIGIT1} {DIGITS}
FRACTIONAL          \. {DIGITS}
FLOAT               {INTEGER} {FRACTIONAL}

BOOLEAN             "#t" | "#f" | "#true" | "#false"

NIL                 "nil"

OTHER_UNICODE       [\u{1F300}-\u{1F5FF}\u{1F900}-\u{1F9FF}\u{1F600}-\u{1F64F}\u{1F680}-\u{1F6FF}]
IDENTIFIER_START    \p{UnicodeIdentifierStart} | [+\-*<>=/.@?^_~&!] | {OTHER_UNICODE}
IDENTIFIER_REST     \p{UnicodeIdentifierPart} | [+\-*<>=/.@?^_~:&!] | {OTHER_UNICODE}
IDENTIFIER          {IDENTIFIER_START} {IDENTIFIER_REST}*

KEYWORD             ":" {IDENTIFIER}

INTRALINE_WS        " " | "\t"
LINE_ENDING         "\r\n" | "\n" | "\r"
WHITESPACE          {INTRALINE_WS} | {LINE_ENDING}

STRING              \"([^\\\"]|\\\")*\"

%%

{L_PAREN}           { return kTokenTypeLParen; }
{R_PAREN}           { return kTokenTypeRParen; }

"'"                 { return kTokenTypeQuote; }
"`"                 { return kTokenTypeQuasiQuote; }
",@"                { return kTokenTypeUnquoteSplice; }
","                 { return kTokenTypeUnquote; }

{LINE_COMMENT}      /* Line Comment */
{WHITESPACE}        /* Ignore whitespace */

{FLOAT}             { return kTokenTypeFloat; }
{INTEGER}           { return kTokenTypeInteger; }
{BOOLEAN}           { return kTokenTypeBoolean; }
{NIL}               { return kTokenTypeNil; }
{STRING}            { return kTokenTypeString; }
{QUOTE}             { return kTokenTypeQuote; }
{IDENTIFIER}        { return kTokenTypeSymbol; }
{KEYWORD}           { return kTokenTypeKeyword; }



<<EOF>>             { return kTokenTypeEOF; }

.                   { std::cout << "** ERROR ** '" << text() << "' at line " << lineno() << " column " << columno() << std::endl; }

%%
