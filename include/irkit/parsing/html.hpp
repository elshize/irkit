// MIT License
//
// Copyright (c) 2018 Michal Siedlaczek
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

//! \file
//! \author Michal Siedlaczek
//! \copyright MIT License

#pragma once

#include <gumbo.h>
#include <string>

namespace irk::parsing::html {

// TODO(michal): Make it iterative rather than recursive.
inline std::string cleantext(GumboNode* node)
{
    if (node->type == GUMBO_NODE_TEXT) {
        return std::string(node->v.text.text);  // NOLINT
    }
    if (node->type == GUMBO_NODE_ELEMENT
        && node->v.element.tag != GUMBO_TAG_SCRIPT  // NOLINT
        && node->v.element.tag != GUMBO_TAG_STYLE) {  // NOLINT
        std::string contents;
        GumboVector* children = &node->v.element.children;  // NOLINT
        for (unsigned int i = 0; i < children->length; ++i) {
            const std::string text = cleantext(
                reinterpret_cast<GumboNode*>(children->data[i]));
            if (i != 0 && not text.empty()) {
                contents.append(" ");
            }
            contents.append(text);
        }
        return contents;
    }
    return std::string();
}

//! Returns a clean text content of a HTML string.
/*!
 * This function gumbo library to parse the HTML tree and fix any potential tag
 * mismatches that could otherwise cause XML parsing errors.
 * @returns a string with text stripped of HTML tags
 */
inline std::string cleantext(const std::string& html)
{
    GumboOutput* output = gumbo_parse(html.c_str());
    std::string content = cleantext(output->root);
    gumbo_destroy_output(&kGumboDefaultOptions, output);
    return content;
}

}  // namespace irk::parsing::html
