#include <karm-io/expr.h>
#include <karm-logger/logger.h>

#include "parser.h"

namespace Web::Xml {

// 2 MARK: Documents
// https://www.w3.org/TR/xml/#sec-documents

Res<Strong<Dom::Document>> Parser::parse(Io::SScan &s, Ns ns) {
    // document :: = prolog element Misc *

    logDebug("Parsing XML document");

    auto doc = makeStrong<Dom::Document>();
    try$(_parseProlog(s, *doc));
    doc->appendChild(try$(_parseElement(s, ns)));
    while (_parseMisc(s, *doc))
        ;

    return Ok(doc);
}

// 2.2 MARK: Characters
// https://www.w3.org/TR/xml/#charsets

static constexpr auto RE_CHAR =
    '\x09'_re | '\x0A'_re | '\x0D'_re | Re::range(0x20, 0xD7FF) |
    Re::range(0xE000, 0xFFFD) | Re::range(0x10000, 0x10FFFF);

// 2.3 MARK: Common Syntactic Constructs
// https://www.w3.org/TR/xml/#sec-common-syn

static constexpr auto RE_S =
    Re::single(' ', '\t', '\r', '\n');

static constexpr auto RE_NAME_START_CHAR =
    ':'_re | Re::range('A', 'Z') | '_'_re | Re::range('a', 'z') | Re::range(0xC0, 0xD6) |
    Re::range(0xD8, 0xF6) | Re::range(0xF8, 0x2FF) | Re::range(0x370, 0x37D) |
    Re::range(0x37F, 0x1FFF) | Re::range(0x200C, 0x200D) | Re::range(0x2070, 0x218F) |
    Re::range(0x2C00, 0x2FEF) | Re::range(0x3001, 0xD7FF) | Re::range(0xF900, 0xFDCF) |
    Re::range(0xFDF0, 0xFFFD) | Re::range(0x10000, 0xEFFFF);

static constexpr auto RE_NAME_CHAR =
    RE_NAME_START_CHAR | '-'_re | '.'_re | Re::range('0', '9') | '\xB7'_re |
    Re::range(0x0300, 0x036F) | Re::range(0x203F, 0x2040);

static constexpr auto RE_NAME = RE_NAME_START_CHAR & Re::zeroOrMore(RE_NAME_CHAR);

Res<> Parser::_parseS(Io::SScan &s) {
    // S ::= (#x20 | #x9 | #xD | #xA)+

    logDebug("Parsing whitespace");
    s.eat(Re::oneOrMore(RE_S));

    return Ok();
}

Res<Str> Parser::_parseName(Io::SScan &s) {
    // Name ::= NameStartChar (NameChar)*

    logDebug("Parsing name");

    auto name = s.token(RE_NAME);
    if (isEmpty(name))
        return Error::invalidData("expected name");
    return Ok(name);
}

// 2.4 MARK: Character Data and Markup
// https://www.w3.org/TR/xml/#syntax

static constexpr auto RE_CHARDATA = Re::negate(Re::single('<', '&'));

Res<> Parser::_parseCharData(Io::SScan &s) {
    // CharData ::= [^<&]* - ([^<&]* ']]>' [^<&]*)

    logDebug("Parsing character data");

    while (
        s.ahead(RE_CHARDATA) and
        not s.ahead("]]>"_re) and
        not s.ended()
    )
        _append(s.next());

    return Ok();
}

// 2.5 MARK: Comments
// https://www.w3.org/TR/xml/#sec-comments

static constexpr auto RE_COMMENT_START = "<!--"_re;
static constexpr auto RE_COMMENT_END = "-->"_re;

Res<Strong<Dom::Comment>> Parser::_parseComment(Io::SScan &s) {
    // 	Comment ::= '<!--' ((Char - '-') | ('-' (Char - '-')))* '-->'

    logDebug("Parsing comment");

    auto rollback = s.rollbackPoint();

    if (not s.skip(RE_COMMENT_START))
        return Error::invalidData("expected '<!--'");

    StringBuilder sb;
    while (not s.ahead(RE_COMMENT_END) and not s.ended()) {
        auto chrs = s.token(RE_CHAR);
        if (isEmpty(chrs))
            return Error::invalidData("expected character data");
        sb.append(chrs);
    }

    if (not s.skip(RE_COMMENT_END))
        return Error::invalidData("expected '-->'");

    rollback.disarm();
    return Ok(makeStrong<Dom::Comment>(sb.take()));
}

// 2.6 MARK: Processing Instructions
// https://www.w3.org/TR/xml/#sec-pi

static constexpr auto RE_PI_START = "<?"_re;
static constexpr auto RE_PI_END = "?>"_re;

Res<> Parser::_parsePi(Io::SScan &s) {
    // PI ::= '<?' PITarget (S (Char* - (Char* '?>' Char*)))? '?>

    logDebug("Parsing processing instruction");

    auto rollback = s.rollbackPoint();

    if (not s.skip(RE_PI_START))
        return Error::invalidData("expected '<?'");
    try$(_parsePiTarget(s));

    while (not s.ahead(RE_PI_END) and not s.ended()) {
        auto chrs = s.token(RE_CHAR);
        if (isEmpty(chrs))
            return Error::invalidData("expected character data");
    }

    if (not s.skip(RE_PI_END))
        return Error::invalidData("expected '?>'");

    rollback.disarm();
    return Ok();
}

Res<> Parser::_parsePiTarget(Io::SScan &s) {
    // PITarget ::= Name - (('X' | 'x') ('M' | 'm') ('L' | 'l'))

    logDebug("Parsing processing instruction target");

    auto name = try$(_parseName(s));
    if (eqCi(name, "xml"s))
        return Error::invalidData("expected name to not be 'xml'");
    return Ok();
}

// 2.7 MARK: CDATA Sections
// https://www.w3.org/TR/xml/#sec-cdata-sect

Res<> Parser::_parseCDSect(Io::SScan &s) {
    // CDStart ::= '<![CDATA['
    // CData ::= (Char* - (Char* ']]>' Char*))
    // CDEnd ::= ']]>'

    logDebug("Parsing CDATA section");

    auto rollback = s.rollbackPoint();

    if (not s.skip("<![CDATA["_re))
        return Error::invalidData("expected '<![CDATA['");

    while (s.match("]]>"_re) == Match::NO and not s.ended())
        _append(s.next());

    if (not s.skip("]]>"_re))
        return Error::invalidData("expected ']]>'");

    rollback.disarm();
    return Ok();
}

// 2.8 MARK: Prolog and Document Type Declaration
// https://www.w3.org/TR/xml/#sec-prolog-dtd

static constexpr auto RE_XML_DECL_START = "<?xml"_re;

Res<> Parser::_parseVersionInfo(Io::SScan &s) {
    // versionInfo ::= S 'version' Eq ("'" VersionNum "'" | '"' VersionNum '"')

    logDebug("Parsing version info");

    try$(_parseS(s));

    if (not s.skip("version"_re))
        return Error::invalidData("expected 'version'");

    return Ok();
}

Res<> Parser::_parseXmlDecl(Io::SScan &s) {
    // XMLDecl ::= '<?xml' VersionInfo EncodingDecl? SDDecl? S? '?>'

    logDebug("Parsing XML declaration");

    if (not s.skip(RE_XML_DECL_START))
        return Error::invalidData("expected '<?xml'");

    // TODO: Parse version info

    return Ok();
}

Res<> Parser::_parseMisc(Io::SScan &s, Dom::Node &parent) {
    // Misc ::= Comment | PI | S

    logDebug("Parsing miscellaneous");

    auto rollback = s.rollbackPoint();

    if (s.match(RE_COMMENT_START) != Match::NO) {
        auto c = try$(_parseComment(s));
        parent.appendChild(c);
    } else if (s.match(RE_PI_START) != Match::NO)
        try$(_parsePi(s));
    else if (s.match(RE_S) != Match::NO)
        try$(_parseS(s));
    else
        return Error::invalidData("unexpected character");

    rollback.disarm();
    return Ok();
}

Res<> Parser::_parseProlog(Io::SScan &s, Dom::Node &parent) {
    // prolog ::= XMLDecl? Misc* (doctypedecl Misc*)?
    auto rollback = s.rollbackPoint();

    logDebug("Parsing prolog");

    if (s.match(RE_XML_DECL_START) != Match::NO)
        try$(Parser::_parseXmlDecl(s));

    while (_parseMisc(s, parent) and not s.ended())
        ;

    // TODO: Parse doctype declaration

    rollback.disarm();
    return Ok();
}

// 2.9 MARK: Standalone Document Declaration
// https://www.w3.org/TR/xml/#sec-rmd

// 3 MARK: Logical Structures
// https://www.w3.org/TR/xml/#sec-logical-struct

Res<Strong<Dom::Element>> Parser::_parseElement(Io::SScan &s, Ns ns) {
    // element ::= EmptyElemTag | STag content ETag

    logDebug("Parsing element");

    auto rollback = s.rollbackPoint();

    if (auto r = _parseEmptyElementTag(s, ns)) {
        rollback.disarm();
        return r;
    }

    if (auto r = _parseStartTag(s, ns)) {
        auto el = r.unwrap();
        try$(_parseContent(s, ns, *el));
        try$(_parseEndTag(s, *el));
        auto te = _flush();
        if (te.len())
            el->appendChild(makeStrong<Dom::Text>(te));

        rollback.disarm();
        return Ok(el);
    }

    return Error::invalidData("expected element");
}

// 3.1 MARK: Start-Tags, End-Tags, and Empty-Element Tags
// https://www.w3.org/TR/xml/#sec-starttags

Res<Strong<Dom::Element>> Parser::_parseStartTag(Io::SScan &s, Ns ns) {
    // STag ::= '<' Name (S Attribute)* S? '>'

    logDebug("Parsing start tag");

    auto rollback = s.rollbackPoint();
    if (not s.skip('<'))
        return Error::invalidData("expected '<'");

    auto name = try$(_parseName(s));

    auto el = makeStrong<Dom::Element>(TagName::make(name, ns));

    try$(_parseS(s));

    while (not s.skip('>') and not s.ended()) {
        try$(_parseAttribute(s, ns, *el));
        try$(_parseS(s));
    }

    rollback.disarm();
    return Ok(el);
}

Res<> Parser::_parseAttribute(Io::SScan &s, Ns ns, Dom::Element &el) {
    // Attribute ::= Name Eq AttValue

    logDebug("Parsing attribute");

    auto rollback = s.rollbackPoint();

    auto name = try$(_parseName(s));

    if (not s.skip('='))
        return Error::invalidData("expected '='");

    auto value = try$(_parseAttValue(s));

    el.setAttribute(AttrName::make(name, ns), value);

    rollback.disarm();
    return Ok();
}

Res<String> Parser::_parseAttValue(Io::SScan &s) {
    // AttValue ::= '"' ([^<&"] | Reference)* '"'
    //              |  "'" ([^<&'] | Reference)* "'"

    logDebug("Parsing attribute value");

    StringBuilder sb;

    auto rollback = s.rollbackPoint();

    auto quote = s.next();
    if (quote != '"' and quote != '\'')
        return Error::invalidData("expected '\"' or '''");

    while (s.curr() != quote and not s.ended()) {
        if (auto r = _parseReference(s))
            sb.append(r.unwrap());
        else
            sb.append(s.next());
    }

    if (s.curr() != quote)
        return Error::invalidData("expected closing quote");

    s.next();

    rollback.disarm();

    return Ok(sb.take());
}

Res<> Parser::_parseEndTag(Io::SScan &s, Dom::Element &el) {
    // '</' Name S? '>'

    logDebug("Parsing end tag");

    auto rollback = s.rollbackPoint();

    if (not s.skip("</"_re))
        return Error::invalidData("expected '</'");

    auto name = try$(_parseName(s));
    if (name != el.tagName.name())
        return Error::invalidData("expected end tag name to match start tag name");

    try$(_parseS(s));

    if (not s.skip('>'))
        return Error::invalidData("expected '>'");

    rollback.disarm();
    return Ok();
}

Res<> Parser::_parseContentItem(Io::SScan &s, Ns ns, Dom::Element &el) {
    // (element | Reference | CDSect | PI | Comment)

    logDebug("Parsing content item");

    if (auto r = _parseElement(s, ns)) {
        el.appendChild(r.unwrap());
        return Ok();
    } else if (auto r = _parseReference(s)) {
        auto te = _flush();
        if (te.len())
            el.appendChild(makeStrong<Dom::Text>(te));
        _append(r.unwrap());
        return Ok();
    } else if (auto r = _parseCDSect(s)) {
        return Ok();
    } else if (auto r = _parsePi(s)) {
        return Ok();
    } else if (auto r = _parseComment(s)) {
        auto te = _flush();
        if (te.len())
            el.appendChild(makeStrong<Dom::Text>(te));
        el.appendChild(r.unwrap());
        return Ok();
    } else {
        return Error::invalidData("expected content item");
    }
}

Res<> Parser::_parseContent(Io::SScan &s, Ns ns, Dom::Element &el) {
    // content ::= CharData? ((element | Reference | CDSect | PI | Comment) CharData?)*

    logDebug("Parsing content");

    try$(_parseCharData(s));
    while (_parseContentItem(s, ns, el))
        try$(_parseCharData(s));

    return Ok();
}

Res<Strong<Dom::Element>> Parser::_parseEmptyElementTag(Io::SScan &s, Ns ns) {
    // EmptyElemTag ::= '<' Name (S Attribute)* S? '/>'

    logDebug("Parsing empty element tag");

    auto rollback = s.rollbackPoint();
    if (not s.skip('<'))
        return Error::invalidData("expected '<'");

    auto name = try$(_parseName(s));

    auto el = makeStrong<Dom::Element>(TagName::make(name, ns));
    try$(_parseS(s));
    while (not s.skip("/>"_re) and not s.ended()) {
        try$(_parseAttribute(s, ns, *el));
        try$(_parseS(s));
    }

    rollback.disarm();
    return Ok(el);
}

// 4.1 MARK: Character and Entity References
// https://www.w3.org/TR/xml/#NT-CharRef

Res<Rune> Parser::_parseCharRef(Io::SScan &s) {
    // CharRef ::= '&#' [0-9]+ ';' | '&#x' [0-9a-fA-F]+ ';'

    logDebug("Parsing character reference");

    auto rollback = s.rollbackPoint();

    if (not s.skip("&#"_re))
        return Error::invalidData("expected '&#'");

    Rune r = REPLACEMENT;

    if (s.skip('x')) {
        auto val = Io::atoi(s, {.base = 16});
        if (not val)
            return Error::invalidData("expected hexadecimal number");
        r = val.unwrap();
    } else {
        auto val = Io::atoi(s, {.base = 10});
        if (not val)
            return Error::invalidData("expected decimal number");
        r = val.unwrap();
    }

    if (not s.skip(';'))
        return Error::invalidData("expected ';'");

    rollback.disarm();
    return Ok(r);
}

Res<Rune> Parser::_parseEntityRef(Io::SScan &s) {
    // EntityRef ::= '&' Name ';'

    logDebug("Parsing entity reference");

    auto rollback = s.rollbackPoint();

    if (not s.skip('&'))
        return Error::invalidData("expected '&'");

    auto name = try$(_parseName(s));

    if (not s.skip(';'))
        return Error::invalidData("expected ';'");

    rollback.disarm();
    if (name == "lt")
        return Ok('<');
    else if (name == "gt")
        return Ok('>');
    else if (name == "amp")
        return Ok('&');
    else if (name == "apos")
        return Ok('\'');
    else if (name == "quot")
        return Ok('"');

    rollback.arm();
    return Error::invalidData("unknown entity reference");
}

Res<Rune> Parser::_parseReference(Io::SScan &s) {
    // Reference ::= EntityRef | CharRef

    logDebug("Parsing reference");

    if (auto r = _parseCharRef(s))
        return r;
    else if (auto r = _parseEntityRef(s))
        return r;
    else
        return Error::invalidData("expected reference");
}

} // namespace Web::Xml
