#pragma once

#include <gumbo.h>
#include <string>

namespace irk::parsing::html {

// TODO: Make it iterative rather than recursive.
std::string cleantext(GumboNode* node)
{
    if (node->type == GUMBO_NODE_TEXT) {
        return std::string(node->v.text.text);
    } else if (node->type == GUMBO_NODE_ELEMENT
        && node->v.element.tag != GUMBO_TAG_SCRIPT
        && node->v.element.tag != GUMBO_TAG_STYLE) {
        std::string contents = "";
        GumboVector* children = &node->v.element.children;
        for (unsigned int i = 0; i < children->length; ++i) {
            const std::string text = cleantext((GumboNode*)children->data[i]);
            if (i != 0 && !text.empty()) {
                contents.append(" ");
            }
            contents.append(text);
        }
        return contents;
    } else {
        return "";
    }
}

//! Returns a clean text content of a HTML string.
/*!
 * This function gumbo library to parse the HTML tree and fix any potential tag
 * mismatches that could otherwise cause XML parsing errors.
 * @returns a string with text stripped of HTML tags
 */
std::string cleantext(const std::string& html)
{
    GumboOutput* output = gumbo_parse(html.c_str());
    std::string content = cleantext(output->root);
    gumbo_destroy_output(&kGumboDefaultOptions, output);
    return content;
}

};  // namespace irk::parsing::html
