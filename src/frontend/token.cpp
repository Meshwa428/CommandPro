#include "synapse/frontend/token.h"
#include <sstream>

namespace syn {

bool Token::is_literal() const
{
    return kind == TokenKind::Int ||
           kind == TokenKind::Float ||
           kind == TokenKind::Duration ||
           kind == TokenKind::String ||
           kind == TokenKind::True ||
           kind == TokenKind::False ||
           kind == TokenKind::None;
}

bool Token::is_keyword() const
{
    return kind >= TokenKind::Let && kind <= TokenKind::As;
}

bool Token::is_command_keyword() const
{
    return kind >= TokenKind::Mouse && kind <= TokenKind::Read;
}

bool Token::is_aug_assign() const
{
    return kind >= TokenKind::PlusEq && kind <= TokenKind::QQEq;
}

bool Token::is_term() const
{
    return kind == TokenKind::Newline || kind == TokenKind::Semicolon;
}

std::string Token::to_string() const
{
    std::ostringstream ss;
    ss << "Token(" << kind_name(kind) << ", [" << span.start << ".." << span.end << ")";
    if (kind == TokenKind::Int) {
        ss << ", int=" << int_val;
    } else if (kind == TokenKind::Float) {
        ss << ", float=" << float_val;
    } else if (kind == TokenKind::Duration) {
        ss << ", dur_ns=" << duration_ns;
    }
    ss << ")";
    return ss.str();
}

std::string_view Token::kind_name(TokenKind k)
{
    switch (k) {
        case TokenKind::Int:           return "Int";
        case TokenKind::Float:         return "Float";
        case TokenKind::Duration:      return "Duration";
        case TokenKind::String:        return "String";
        case TokenKind::True:          return "True";
        case TokenKind::False:         return "False";
        case TokenKind::None:          return "None";
        case TokenKind::Ident:         return "Ident";
        case TokenKind::Let:           return "Let";
        case TokenKind::Const:         return "Const";
        case TokenKind::Fn:            return "Fn";
        case TokenKind::Return:        return "Return";
        case TokenKind::If:            return "If";
        case TokenKind::Else:          return "Else";
        case TokenKind::While:         return "While";
        case TokenKind::Repeat:        return "Repeat";
        case TokenKind::For:           return "For";
        case TokenKind::In:            return "In";
        case TokenKind::Break:         return "Break";
        case TokenKind::Continue:      return "Continue";
        case TokenKind::Match:         return "Match";
        case TokenKind::Case:          return "Case";
        case TokenKind::Try:           return "Try";
        case TokenKind::Catch:         return "Catch";
        case TokenKind::Finally:       return "Finally";
        case TokenKind::Throw:         return "Throw";
        case TokenKind::And:           return "And";
        case TokenKind::Or:            return "Or";
        case TokenKind::Not:           return "Not";
        case TokenKind::Is:            return "Is";
        case TokenKind::Use:           return "Use";
        case TokenKind::As:            return "As";
        case TokenKind::KwInt:         return "KwInt";
        case TokenKind::KwFloat:       return "KwFloat";
        case TokenKind::KwString:      return "KwString";
        case TokenKind::KwBool:        return "KwBool";
        case TokenKind::KwList:        return "KwList";
        case TokenKind::KwMap:         return "KwMap";
        case TokenKind::KwTuple:       return "KwTuple";
        case TokenKind::Say:           return "Say";
        case TokenKind::Mouse:         return "Mouse";
        case TokenKind::Click:         return "Click";
        case TokenKind::Drag:          return "Drag";
        case TokenKind::Scroll:        return "Scroll";
        case TokenKind::Hold:          return "Hold";
        case TokenKind::Release:       return "Release";
        case TokenKind::Press:         return "Press";
        case TokenKind::Type:          return "Type";
        case TokenKind::Run:           return "Run";
        case TokenKind::Open:          return "Open";
        case TokenKind::Close:         return "Close";
        case TokenKind::Focus:         return "Focus";
        case TokenKind::Move:          return "Move";
        case TokenKind::Resize:        return "Resize";
        case TokenKind::Maximize:      return "Maximize";
        case TokenKind::Minimize:      return "Minimize";
        case TokenKind::Capture:       return "Capture";
        case TokenKind::Wait:          return "Wait";
        case TokenKind::Find:          return "Find";
        case TokenKind::See:           return "See";
        case TokenKind::Tap:           return "Tap";
        case TokenKind::Check:         return "Check";
        case TokenKind::Uncheck:       return "Uncheck";
        case TokenKind::Select:        return "Select";
        case TokenKind::Read:          return "Read";
        case TokenKind::Left:          return "Left";
        case TokenKind::Right:         return "Right";
        case TokenKind::Middle:        return "Middle";
        case TokenKind::Up:            return "Up";
        case TokenKind::Down:          return "Down";
        case TokenKind::MinConfidence: return "MinConfidence";
        case TokenKind::Button:        return "Button";
        case TokenKind::Input:         return "Input";
        case TokenKind::Checkbox:      return "Checkbox";
        case TokenKind::Radio:         return "Radio";
        case TokenKind::Dropdown:      return "Dropdown";
        case TokenKind::Link:          return "Link";
        case TokenKind::Icon:          return "Icon";
        case TokenKind::Toggle:        return "Toggle";
        case TokenKind::Slider:        return "Slider";
        case TokenKind::Tab:           return "Tab";
        case TokenKind::MenuItem:      return "MenuItem";
        case TokenKind::Plus:          return "Plus";
        case TokenKind::Minus:         return "Minus";
        case TokenKind::Star:          return "Star";
        case TokenKind::Slash:         return "Slash";
        case TokenKind::SlashSlash:    return "SlashSlash";
        case TokenKind::Percent:       return "Percent";
        case TokenKind::StarStar:      return "StarStar";
        case TokenKind::EqEq:          return "EqEq";
        case TokenKind::BangEq:        return "BangEq";
        case TokenKind::Lt:            return "Lt";
        case TokenKind::LtEq:          return "LtEq";
        case TokenKind::Gt:            return "Gt";
        case TokenKind::GtEq:          return "GtEq";
        case TokenKind::Eq:            return "Eq";
        case TokenKind::PlusEq:        return "PlusEq";
        case TokenKind::MinusEq:       return "MinusEq";
        case TokenKind::StarEq:        return "StarEq";
        case TokenKind::SlashEq:       return "SlashEq";
        case TokenKind::SlashSlashEq:  return "SlashSlashEq";
        case TokenKind::PercentEq:     return "PercentEq";
        case TokenKind::StarStarEq:    return "StarStarEq";
        case TokenKind::QQEq:          return "QQEq";
        case TokenKind::QQ:            return "QQ";
        case TokenKind::Pipe:          return "Pipe";
        case TokenKind::OptChain:      return "OptChain";
        case TokenKind::Dot:           return "Dot";
        case TokenKind::DotDot:        return "DotDot";
        case TokenKind::LParen:        return "LParen";
        case TokenKind::RParen:        return "RParen";
        case TokenKind::LBracket:      return "LBracket";
        case TokenKind::RBracket:      return "RBracket";
        case TokenKind::LBrace:        return "LBrace";
        case TokenKind::RBrace:        return "RBrace";
        case TokenKind::Comma:         return "Comma";
        case TokenKind::Colon:         return "Colon";
        case TokenKind::Semicolon:     return "Semicolon";
        case TokenKind::Newline:       return "Newline";
        case TokenKind::InterpOpen:    return "InterpOpen";
        case TokenKind::InterpClose:   return "InterpClose";
        case TokenKind::StrPart:       return "StrPart";
        case TokenKind::To:            return "To";
        case TokenKind::By:            return "By";
        case TokenKind::Wildcard:      return "Wildcard";
        case TokenKind::Eof:           return "Eof";
        case TokenKind::Error:         return "Error";
    }
    return "Unknown";
}

} // namespace syn
