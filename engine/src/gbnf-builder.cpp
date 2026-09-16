// llama.cpp
#include <json-schema-to-grammar.h>
#include "gbnf-builder.h"

namespace
{
    std::string escapeGbnfString(const std::string &value)
    {
        std::string out;
        out.reserve(value.size());

        for (char c : value)
        {
            if (c == '\\' || c == '"')
                out += '\\';

            out += c;
        }

        return out;
    }

    std::string makeCloseTag(const std::string &open_tag)
    {
        if (open_tag.size() < 3 ||
            open_tag.front() != '<' ||
            open_tag.back() != '>')
        {
            throw std::invalid_argument(
                "Invalid GBNF trigger tag: " + open_tag);
        }

        return "</" + open_tag.substr(1);
    }
}
//
std::string buildGbnfBoolean()
{
    return "root ::= \"yes\" | \"no\"\n";
}

std::string buildGbnfPlainEnum(const std::vector<std::string> &values)
{
    if (values.empty())
        return {};

    std::string root = "root ::= ";

    for (size_t i = 0; i < values.size(); ++i)
    {
        if (i > 0)
            root += " | ";

        root += "\"";
        root += escapeGbnfString(values[i]);
        root += "\"";
    }

    root += "\n";

    return root;
}

std::string buildGbnfJsonSchema(const std::string &schema_json)
{
    common_json schema;
    try
    {
        schema = common_json::parse(schema_json);
    }
    catch (const std::exception &e)
    {
        throw std::invalid_argument(std::string("Invalid JSON schema: ") + e.what());
    }

    // force_gbnf = true:
    // always return GBNF even when LLAMA_USE_LLGUIDANCE is enabled.
    return json_schema_to_grammar(schema, /*force_gbnf=*/true);
}

std::string buildGbnfToolCall(const std::string &arguments_schema, const std::string &trigger)
{
    std::string body_gbnf = buildGbnfJsonSchema(arguments_schema);

    constexpr std::string_view root_rule = "root ::=";

    const size_t pos = body_gbnf.find(root_rule);
    if (pos == std::string::npos)
        throw std::runtime_error("JSON schema grammar does not contain root rule");
    body_gbnf.replace(
        pos,
        root_rule.size(),
        "tool-call-body ::= ");

    const std::string close_tag = makeCloseTag(trigger);

    return "root ::= \"" + escapeGbnfString(trigger) +
           "\" space  tool-call-body space  \"" +
           escapeGbnfString(close_tag) + "\"\n" +
           body_gbnf;
}
